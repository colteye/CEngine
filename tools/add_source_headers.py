#!/usr/bin/env python3
#   _____ ______             _
#  / ____|  ____|           (_)
# | |    | |__   _ __   __ _ _ _ __   ___
# | |    |  __| | '_ \ / _` | | '_ \ / _ |
# | |____| |____| | | | (_| | | | | |  __/
#  \_____|______|_| |_|\__, |_|_| |_|\___|
#                       __/ |
#                      |___/


"""Add the standard CEngine banner to project-owned source files.

Author:
    Erik Coltey"""

from __future__ import annotations

import argparse
import ast
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


ASCII_ART = r"""  _____ ______             _
 / ____|  ____|           (_)
| |    | |__   _ __   __ _ _ _ __   ___
| |    |  __| | '_ \ / _` | | '_ \ / _ |
| |____| |____| | | | (_| | | | | |  __/
 \_____|______|_| |_|\__, |_|_| |_|\___|
                      __/ |
                     |___/"""

SOURCE_EXTENSIONS = {
    ".c",
    ".cc",
    ".cpp",
    ".cxx",
    ".cu",
    ".cuh",
    ".h",
    ".hh",
    ".hpp",
    ".hxx",
    ".ipp",
    ".inl",
    ".tpp",
    ".glsl",
    ".vert",
    ".frag",
    ".comp",
    ".geom",
    ".tesc",
    ".tese",
    ".hlsl",
    ".metal",
    ".py",
    ".sh",
    ".bash",
    ".zsh",
    ".cmake",
}

EXCLUDED_DIRECTORIES = {
    ".git",
    "build",
    "external",
    "third_party",
    "vendor",
}

PYTHON_ENCODING = re.compile(r"^#.*coding[=:]\s*[-\w.]+")

CXX_EXTENSIONS = {
    ".c",
    ".cc",
    ".cpp",
    ".cxx",
    ".cu",
    ".cuh",
    ".h",
    ".hh",
    ".hpp",
    ".hxx",
    ".ipp",
    ".inl",
    ".tpp",
}

CXX_FUNCTION_KEYWORDS = {
    "alignas",
    "catch",
    "decltype",
    "for",
    "if",
    "noexcept",
    "requires",
    "sizeof",
    "static_assert",
    "switch",
    "while",
}


@dataclass(frozen=True)
class CxxSymbol:
    """Describe a C++ declaration that needs a Doxygen block."""

    line: int
    kind: str
    name: str
    parameters: tuple[str, ...] = ()
    template_parameters: tuple[str, ...] = ()
    returns_value: bool = False


def comment_prefix(path: Path) -> str:
    """TODO: Describe `comment_prefix`.

    Args:
        path: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    if (
        path.suffix in {".py", ".sh", ".bash", ".zsh", ".cmake"}
        or path.name == "CMakeLists.txt"
    ):
        return "#"
    return "//"


def make_header(prefix: str, newline: str) -> str:
    """TODO: Describe `make_header`.

    Args:
        prefix: TODO: Describe this parameter.
        newline: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    return (
        newline.join(
            f"{prefix} {line}".rstrip() for line in ASCII_ART.splitlines()
        )
        + newline * 2
    )


def read_source_text(path: Path) -> tuple[str, bool, str]:
    """Read UTF-8 source while retaining its BOM and newline convention."""
    original = path.read_bytes()
    has_bom = original.startswith(b"\xef\xbb\xbf")
    encoded_text = original[3:] if has_bom else original

    try:
        text = encoded_text.decode("utf-8")
    except UnicodeDecodeError as error:
        raise RuntimeError(f"{path} is not UTF-8 text") from error

    newline = "\r\n" if b"\r\n" in encoded_text else "\n"
    return text, has_bom, newline


def write_source_text(path: Path, text: str, *, has_bom: bool) -> None:
    """Write UTF-8 source while restoring its original BOM."""
    bom = b"\xef\xbb\xbf" if has_bom else b""
    path.write_bytes(bom + text.encode("utf-8"))


def line_offsets(text: str) -> list[int]:
    """Return the character offset at which each source line begins."""
    offsets = [0]
    offsets.extend(match.end() for match in re.finditer(r"\n", text))
    return offsets


def python_function_parameters(
    node: ast.FunctionDef | ast.AsyncFunctionDef,
) -> list[str]:
    """Return documentable parameter names from a Python function."""
    arguments = node.args
    parameters = [
        argument.arg for argument in (*arguments.posonlyargs, *arguments.args)
    ]
    if parameters and parameters[0] in {"self", "cls"}:
        parameters.pop(0)
    if arguments.vararg is not None:
        parameters.append(f"*{arguments.vararg.arg}")
    parameters.extend(argument.arg for argument in arguments.kwonlyargs)
    if arguments.kwarg is not None:
        parameters.append(f"**{arguments.kwarg.arg}")
    return parameters


def python_function_result(
    node: ast.FunctionDef | ast.AsyncFunctionDef,
) -> str | None:
    """Classify a Python function result without entering nested definitions."""
    found_return = False
    found_yield = False

    def visit(current: ast.AST) -> None:
        """TODO: Describe `visit`.

        Args:
            current: TODO: Describe this parameter.
        """
        nonlocal found_return, found_yield
        for child in ast.iter_child_nodes(current):
            if child is not node and isinstance(
                child, (ast.ClassDef, ast.FunctionDef, ast.AsyncFunctionDef)
            ):
                continue
            if isinstance(child, (ast.Yield, ast.YieldFrom)):
                found_yield = True
            elif isinstance(child, ast.Return) and child.value is not None:
                found_return = True
            visit(child)

    visit(node)
    if found_yield:
        return "Yields"
    if found_return and node.name != "__init__":
        return "Returns"
    return None


def make_python_symbol_doc(
    node: ast.ClassDef | ast.FunctionDef | ast.AsyncFunctionDef,
    indent: str,
    newline: str,
) -> str:
    """Build a PEP 257 and Google-style placeholder docstring."""
    lines = [f'{indent}"""TODO: Describe `{node.name}`.']

    if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
        parameters = python_function_parameters(node)
        result_section = python_function_result(node)
        if parameters:
            lines.extend(["", f"{indent}Args:"])
            lines.extend(
                f"{indent}    {parameter}: TODO: Describe this parameter."
                for parameter in parameters
            )
        if result_section is not None:
            lines.extend(
                [
                    "",
                    f"{indent}{result_section}:",
                    f"{indent}    TODO: Describe the produced value.",
                ]
            )

    if len(lines) == 1:
        lines[0] += '"""'
    else:
        lines.append(f'{indent}"""')
    suffix = newline * 2 if isinstance(node, ast.ClassDef) else newline
    return newline.join(lines) + suffix


def make_python_module_doc(newline: str) -> str:
    """Build the standard placeholder module docstring."""
    return newline.join(
        [
            '"""TODO: Briefly describe this module.',
            "",
            "Author:",
            "    Erik Coltey",
            '"""',
            "",
        ]
    ) + newline


def ensure_python_module_doc(path: Path, *, check_only: bool) -> bool:
    """Ensure a Python module has a summary and author."""
    text, has_bom, newline = read_source_text(path)
    tree = ast.parse(text, filename=str(path))
    module_doc = ast.get_docstring(tree, clean=False)

    if module_doc is None:
        prefix = comment_prefix(path)
        header = make_header(prefix, newline)
        insertion_point = preamble_end(text)
        body = text[insertion_point:]
        if not body.startswith(header):
            raise RuntimeError(f"{path} does not have the managed CEngine header")
        insertion_point += len(header)
        if not check_only:
            updated = (
                text[:insertion_point]
                + make_python_module_doc(newline)
                + text[insertion_point:]
            )
            write_source_text(path, updated, has_bom=has_bom)
        return True

    if re.search(r"(?m)^Author:\s*$", module_doc):
        return False

    expression = tree.body[0]
    if not isinstance(expression, ast.Expr) or not isinstance(
        expression.value, ast.Constant
    ):
        raise RuntimeError(f"Unable to locate the module docstring in {path}")

    if not check_only:
        offsets = line_offsets(text)
        start = offsets[expression.lineno - 1] + expression.col_offset
        end = offsets[expression.end_lineno - 1] + expression.end_col_offset
        content = module_doc.rstrip() + f"{newline}{newline}Author:{newline}    Erik Coltey"
        replacement = f'"""{content}"""'
        updated = text[:start] + replacement + text[end:]
        write_source_text(path, updated, has_bom=has_bom)
    return True


def ensure_python_symbol_docs(path: Path, *, check_only: bool) -> int:
    """Add placeholder docstrings to undocumented Python classes and functions."""
    text, has_bom, newline = read_source_text(path)
    tree = ast.parse(text, filename=str(path))
    nodes = [
        node
        for node in ast.walk(tree)
        if isinstance(node, (ast.ClassDef, ast.FunctionDef, ast.AsyncFunctionDef))
        and ast.get_docstring(node, clean=False) is None
    ]
    if not nodes or check_only:
        return len(nodes)

    offsets = line_offsets(text)
    edits: list[tuple[int, int, str]] = []
    for node in nodes:
        indent = " " * (node.col_offset + 4)
        doc = make_python_symbol_doc(node, indent, newline)
        first_statement = node.body[0]

        if first_statement.lineno == node.lineno:
            line_start = offsets[node.lineno - 1]
            line_end = offsets[node.lineno] if node.lineno < len(offsets) else len(text)
            line = text[line_start:line_end]
            statement_offset = first_statement.col_offset
            replacement = (
                line[:statement_offset].rstrip()
                + newline
                + doc
                + indent
                + line[statement_offset:]
            )
            edits.append((line_start, line_end, replacement))
        else:
            insertion_point = offsets[first_statement.lineno - 1]
            edits.append((insertion_point, insertion_point, doc))

    for start, end, replacement in sorted(edits, reverse=True):
        text = text[:start] + replacement + text[end:]
    write_source_text(path, text, has_bom=has_bom)
    return len(nodes)


def make_cpp_file_doc(relative_path: Path, newline: str) -> str:
    """Build a standard Doxygen file block."""
    return newline.join(
        [
            "/**",
            f" * @file {relative_path.as_posix()}",
            " * @brief TODO: Describe the purpose of this file.",
            " * @author Erik Coltey",
            " */",
            "",
        ]
    ) + newline


def ensure_cpp_file_doc(
    path: Path, repo_root: Path, *, check_only: bool
) -> bool:
    """Ensure a C++ source file has a Doxygen file block."""
    text, has_bom, newline = read_source_text(path)
    header = make_header(comment_prefix(path), newline)
    compact_header = header.removesuffix(newline)
    if text.startswith(header):
        body = text[len(header) :]
    elif text == compact_header:
        body = ""
    else:
        raise RuntimeError(f"{path} does not have the managed CEngine header")

    relative_path = path.relative_to(repo_root)
    file_doc = make_cpp_file_doc(relative_path, newline)
    compact_file_doc = file_doc.removesuffix(newline)
    if body == compact_file_doc:
        return False
    if body == file_doc:
        if not check_only:
            write_source_text(
                path, header + compact_file_doc, has_bom=has_bom
            )
        return True
    if re.match(r"/\*\*[^\0]*?@file\b", body[:1024], re.DOTALL):
        return False

    if not check_only:
        applied_file_doc = compact_file_doc if not body else file_doc
        updated = header + applied_file_doc + body
        write_source_text(path, updated, has_bom=has_bom)
    return True


def mask_cpp_source(text: str) -> str:
    """Mask C++ comments and literals while preserving source positions."""
    characters = list(text)
    index = 0
    length = len(text)

    def mask(start: int, end: int) -> None:
        """TODO: Describe `mask`.

        Args:
            start: TODO: Describe this parameter.
            end: TODO: Describe this parameter.
        """
        for position in range(start, end):
            if characters[position] not in {"\n", "\r"}:
                characters[position] = " "

    while index < length:
        if text.startswith("//", index):
            end = text.find("\n", index)
            end = length if end < 0 else end
            mask(index, end)
            index = end
            continue
        if text.startswith("/*", index):
            end = text.find("*/", index + 2)
            end = length if end < 0 else end + 2
            mask(index, end)
            index = end
            continue
        if text.startswith('R"', index):
            delimiter_end = text.find("(", index + 2, index + 20)
            if delimiter_end >= 0:
                delimiter = text[index + 2 : delimiter_end]
                terminator = ")" + delimiter + '"'
                end = text.find(terminator, delimiter_end + 1)
                end = length if end < 0 else end + len(terminator)
                mask(index, end)
                index = end
                continue
        if text[index] in {'"', "'"}:
            quote = text[index]
            end = index + 1
            while end < length:
                if text[end] == "\\":
                    end += 2
                    continue
                end += 1
                if text[end - 1] == quote:
                    break
            mask(index, min(end, length))
            index = end
            continue
        index += 1

    masked = "".join(characters)
    lines = masked.splitlines(keepends=True)
    in_directive = False
    for line_index, line in enumerate(lines):
        stripped = line.lstrip()
        if in_directive or stripped.startswith("#"):
            ending = line[len(line.rstrip("\r\n")) :]
            directive = line[: len(line) - len(ending)]
            in_directive = directive.rstrip().endswith("\\")
            lines[line_index] = " " * len(directive) + ending
        else:
            in_directive = False
    return "".join(lines)


def matching_cpp_braces(masked: str) -> dict[int, int]:
    """Map opening C++ braces to their matching closing braces."""
    stack: list[int] = []
    matches: dict[int, int] = {}
    for index, character in enumerate(masked):
        if character == "{":
            stack.append(index)
        elif character == "}" and stack:
            matches[stack.pop()] = index
    return matches


def strip_cpp_declaration_prefix(segment: str) -> tuple[str, int]:
    """Strip whitespace and access labels from a C++ declaration segment."""
    offset = len(segment) - len(segment.lstrip())
    segment = segment[offset:]
    access_matches = list(
        re.finditer(r"\b(?:public|protected|private)\s*:\s*", segment)
    )
    if access_matches:
        access_end = access_matches[-1].end()
        offset += access_end
        segment = segment[access_end:]
        whitespace = len(segment) - len(segment.lstrip())
        offset += whitespace
        segment = segment.lstrip()
    return segment, offset


def split_cpp_parameters(parameters: str) -> list[str]:
    """Split a C++ parameter list at top-level commas."""
    parts: list[str] = []
    start = 0
    depths = {"(": 0, "[": 0, "{": 0, "<": 0}
    closing = {")": "(", "]": "[", "}": "{", ">": "<"}
    for index, character in enumerate(parameters):
        if character in depths:
            depths[character] += 1
        elif character in closing:
            key = closing[character]
            depths[key] = max(0, depths[key] - 1)
        elif character == "," and not any(depths.values()):
            parts.append(parameters[start:index])
            start = index + 1
    parts.append(parameters[start:])
    return parts


def cpp_parameter_name(parameter: str) -> str | None:
    """Extract a documentable name from one C++ parameter."""
    parameter = parameter.strip()
    if not parameter or parameter == "void":
        return None

    depths = {"(": 0, "[": 0, "{": 0, "<": 0}
    closing = {")": "(", "]": "[", "}": "{", ">": "<"}
    default_index: int | None = None
    for index, character in enumerate(parameter):
        if character in depths:
            depths[character] += 1
        elif character in closing:
            key = closing[character]
            depths[key] = max(0, depths[key] - 1)
        elif character == "=" and not any(depths.values()):
            default_index = index
            break
    if default_index is not None:
        parameter = parameter[:default_index].rstrip()

    function_pointer = re.search(r"\(\s*[*&]\s*([A-Za-z_]\w*)\s*\)", parameter)
    if function_pointer:
        return function_pointer.group(1)

    match = re.search(r"([A-Za-z_]\w*)\s*(?:\[[^\]]*\]\s*)?$", parameter)
    if match is None:
        return None
    name = match.group(1)
    if match.start(1) >= 2 and parameter[match.start(1) - 2 : match.start(1)] == "::":
        return None

    identifiers = re.findall(r"[A-Za-z_]\w*", parameter)
    qualifiers = {
        "const",
        "constexpr",
        "register",
        "restrict",
        "signed",
        "static",
        "unsigned",
        "volatile",
    }
    meaningful = [identifier for identifier in identifiers if identifier not in qualifiers]
    return name if len(meaningful) >= 2 else None


def cpp_template_parameters(segment: str) -> tuple[str, ...]:
    """Extract template parameter names from a C++ declaration."""
    match = re.search(r"\btemplate\s*<(.+?)>\s*", segment, re.DOTALL)
    if match is None:
        return ()

    names: list[str] = []
    for parameter in split_cpp_parameters(match.group(1)):
        parameter = parameter.split("=", 1)[0].strip()
        type_parameter = re.search(
            r"\b(?:typename|class)\s*(?:\.\.\.)?\s*([A-Za-z_]\w*)",
            parameter,
        )
        if type_parameter:
            names.append(type_parameter.group(1))
            continue
        identifiers = re.findall(r"[A-Za-z_]\w*", parameter)
        if len(identifiers) >= 2:
            names.append(identifiers[-1])
    return tuple(names)


def cpp_function_symbol(
    segment: str, line: int, class_name: str | None
) -> CxxSymbol | None:
    """Parse a function declaration from a masked C++ segment."""
    if not segment or re.match(
        r"^(?:case\b|default\b|do\b|else\b|return\b|throw\b|typedef\b|using\b)",
        segment,
    ):
        return None

    pairs: list[tuple[int, int]] = []
    depth = 0
    start = 0
    for index, character in enumerate(segment):
        if character == "(":
            if depth == 0:
                start = index
            depth += 1
        elif character == ")" and depth:
            depth -= 1
            if depth == 0:
                pairs.append((start, index))

    for open_index, close_index in pairs:
        prefix = segment[:open_index].rstrip()
        operator_match = re.search(
            r"(operator\s*(?:\(\)|\[\]|[^\s(]+))\s*$", prefix
        )
        name_match = re.search(
            r"(~?[A-Za-z_]\w*(?:::\s*~?[A-Za-z_]\w*)*)\s*$", prefix
        )
        match = operator_match or name_match
        if match is None:
            continue
        if "=" in prefix[: match.start()]:
            continue
        name = re.sub(r"\s+", "", match.group(1))
        simple_name = name.split("::")[-1]
        if simple_name in CXX_FUNCTION_KEYWORDS:
            continue

        parameter_text = segment[open_index + 1 : close_index]
        if parameter_text.lstrip().startswith(("*", "&")):
            continue
        parameters = tuple(
            parameter_name
            for parameter in split_cpp_parameters(parameter_text)
            if (parameter_name := cpp_parameter_name(parameter)) is not None
        )

        name_parts = [part.removeprefix("~") for part in name.split("::")]
        constructor = (
            class_name is not None
            and simple_name.removeprefix("~") == class_name
        ) or (len(name_parts) >= 2 and name_parts[-1] == name_parts[-2])
        return_prefix = prefix[: match.start()].strip()
        returns_value = (
            not constructor
            and not re.search(r"\bvoid\s*$", return_prefix)
            and not simple_name.startswith("~")
        )
        return CxxSymbol(
            line=line,
            kind="function",
            name=name,
            parameters=parameters,
            template_parameters=cpp_template_parameters(segment),
            returns_value=returns_value,
        )
    return None


def cpp_type_symbol(segment: str, line: int) -> CxxSymbol | None:
    """Parse a class, struct, union, or enum definition."""
    matches = list(
        re.finditer(
            r"\b(enum(?:\s+class)?|class|struct|union)\s+([A-Za-z_]\w*)",
            segment,
        )
    )
    if not matches:
        return None
    match = matches[-1]
    return CxxSymbol(
        line=line,
        kind="type",
        name=match.group(2),
        template_parameters=cpp_template_parameters(segment),
    )


def scan_cpp_symbols(text: str) -> list[CxxSymbol]:
    """Locate documentable C++ type and function declarations."""
    masked = mask_cpp_source(text)
    braces = matching_cpp_braces(masked)
    symbols: list[CxxSymbol] = []

    def scan_region(
        start: int,
        end: int,
        scope_kind: str,
        class_name: str | None = None,
    ) -> None:
        """TODO: Describe `scan_region`.

        Args:
            start: TODO: Describe this parameter.
            end: TODO: Describe this parameter.
            scope_kind: TODO: Describe this parameter.
            class_name: TODO: Describe this parameter.
        """
        statement_start = start
        index = start
        parenthesis_depth = 0
        bracket_depth = 0
        while index < end:
            character = masked[index]
            if character == "(":
                parenthesis_depth += 1
            elif character == ")":
                parenthesis_depth = max(0, parenthesis_depth - 1)
            elif character == "[":
                bracket_depth += 1
            elif character == "]":
                bracket_depth = max(0, bracket_depth - 1)
            elif (
                character == "{"
                and parenthesis_depth == 0
                and bracket_depth == 0
            ):
                close = braces.get(index)
                if close is None or close > end:
                    index += 1
                    continue

                raw_segment = masked[statement_start:index]
                segment, segment_offset = strip_cpp_declaration_prefix(raw_segment)
                absolute_start = statement_start + segment_offset
                line = masked.count("\n", 0, absolute_start) + 1

                if re.search(r"\bnamespace(?:\s+[A-Za-z_]\w*(?:::\w+)*)?\s*$", segment):
                    scan_region(index + 1, close, "namespace")
                elif scope_kind != "function":
                    type_symbol = cpp_type_symbol(segment, line)
                    if type_symbol is not None:
                        symbols.append(type_symbol)
                        type_kind_match = list(
                            re.finditer(
                                r"\b(enum(?:\s+class)?|class|struct|union)\s+"
                                r"([A-Za-z_]\w*)",
                                segment,
                            )
                        )[-1]
                        type_kind = type_kind_match.group(1)
                        if not type_kind.startswith("enum"):
                            scan_region(
                                index + 1,
                                close,
                                "class",
                                type_kind_match.group(2),
                            )
                    else:
                        function_symbol = cpp_function_symbol(
                            segment, line, class_name
                        )
                        if function_symbol is not None:
                            symbols.append(function_symbol)

                index = close + 1
                statement_start = index
                continue

            if (
                character == ";"
                and parenthesis_depth == 0
                and bracket_depth == 0
            ):
                if scope_kind in {"namespace", "class"}:
                    raw_segment = masked[statement_start:index]
                    segment, segment_offset = strip_cpp_declaration_prefix(
                        raw_segment
                    )
                    absolute_start = statement_start + segment_offset
                    line = masked.count("\n", 0, absolute_start) + 1
                    function_symbol = cpp_function_symbol(
                        segment, line, class_name
                    )
                    if function_symbol is not None:
                        symbols.append(function_symbol)
                statement_start = index + 1
            index += 1

    scan_region(0, len(masked), "namespace")
    unique = {(symbol.line, symbol.kind, symbol.name): symbol for symbol in symbols}
    return sorted(unique.values(), key=lambda symbol: symbol.line)


def has_cpp_doc(lines: list[str], declaration_line: int) -> bool:
    """Return whether a declaration already has an adjacent Doxygen block."""
    index = declaration_line - 2
    while index >= 0 and not lines[index].strip():
        index -= 1
    if index < 0:
        return False

    stripped = lines[index].lstrip()
    if stripped.startswith(("///", "//!")):
        return True
    if not stripped.rstrip().endswith("*/"):
        return False

    while index >= 0 and "/*" not in lines[index]:
        index -= 1
    return index >= 0 and lines[index].lstrip().startswith(("/**", "/*!"))


def make_cpp_symbol_doc(symbol: CxxSymbol, indent: str, newline: str) -> str:
    """Build a standard Doxygen placeholder for a C++ symbol."""
    lines = [
        f"{indent}/**",
        f"{indent} * @brief TODO: Describe {symbol.name}.",
    ]
    details: list[str] = []
    details.extend(
        f"{indent} * @tparam {parameter} TODO: Describe this template parameter."
        for parameter in symbol.template_parameters
    )
    if symbol.kind == "function":
        details.extend(
            f"{indent} * @param {parameter} TODO: Describe this parameter."
            for parameter in symbol.parameters
        )
        if symbol.returns_value:
            details.append(
                f"{indent} * @return TODO: Describe the return value."
            )
    if details:
        lines.append(f"{indent} *")
        lines.extend(details)
    lines.append(f"{indent} */")
    return newline.join(lines) + newline


def ensure_cpp_symbol_docs(path: Path, *, check_only: bool) -> int:
    """Add Doxygen placeholders to undocumented C++ declarations."""
    text, has_bom, newline = read_source_text(path)
    lines = text.splitlines(keepends=True)
    symbols = [
        symbol
        for symbol in scan_cpp_symbols(text)
        if not has_cpp_doc(lines, symbol.line)
    ]
    if not symbols or check_only:
        return len(symbols)

    for symbol in reversed(symbols):
        line_index = symbol.line - 1
        declaration_line = lines[line_index]
        indent_match = re.match(r"[ \t]*", declaration_line)
        indent = indent_match.group(0) if indent_match else ""
        lines.insert(
            line_index,
            make_cpp_symbol_doc(symbol, indent, newline),
        )
    write_source_text(path, "".join(lines), has_bom=has_bom)
    return len(symbols)


def source_files(repo_root: Path) -> list[Path]:
    """TODO: Describe `source_files`.

    Args:
        repo_root: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    result = subprocess.run(
        [
            "git",
            "-C",
            str(repo_root),
            "ls-files",
            "--cached",
            "--others",
            "--exclude-standard",
            "-z",
        ],
        check=True,
        capture_output=True,
    )

    files: list[Path] = []
    for raw_path in result.stdout.split(b"\0"):
        if not raw_path:
            continue

        relative_path = Path(raw_path.decode("utf-8"))
        if any(part in EXCLUDED_DIRECTORIES for part in relative_path.parts):
            continue
        if (
            relative_path.name != "CMakeLists.txt"
            and relative_path.suffix not in SOURCE_EXTENSIONS
        ):
            continue

        path = repo_root / relative_path
        if path.is_file():
            files.append(path)

    return sorted(files)


def preamble_end(text: str) -> int:
    """TODO: Describe `preamble_end`.

    Args:
        text: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    lines = text.splitlines(keepends=True)
    line_count = 0

    if lines and lines[0].startswith("#!"):
        line_count = 1

    if line_count < len(lines) and line_count < 2:
        if PYTHON_ENCODING.match(lines[line_count]):
            line_count += 1

    return sum(len(line) for line in lines[:line_count])


def add_header(path: Path, *, check_only: bool) -> bool:
    """Add the managed ASCII-art banner to one source file."""
    text, has_bom, newline = read_source_text(path)
    header = make_header(comment_prefix(path), newline)
    compact_header = header.removesuffix(newline)
    hash_comment_source = comment_prefix(path) == "#"
    insertion_point = preamble_end(text) if hash_comment_source else 0
    body = text[insertion_point:]

    if body == compact_header or (body.startswith(header) and body != header):
        return False

    if not check_only:
        applied_header = compact_header if not body or body == header else header
        updated = text[:insertion_point] + applied_header + body.removeprefix(header)
        write_source_text(path, updated, has_bom=has_bom)

    return True


def document_source(
    path: Path, repo_root: Path, *, check_only: bool
) -> tuple[bool, int]:
    """Apply the banner, file documentation, and symbol documentation."""
    changed = add_header(path, check_only=check_only)
    symbol_count = 0
    if changed and check_only:
        return True, symbol_count

    if path.suffix == ".py":
        changed |= ensure_python_module_doc(path, check_only=check_only)
        symbol_count = ensure_python_symbol_docs(path, check_only=check_only)
    elif path.suffix in CXX_EXTENSIONS:
        changed |= ensure_cpp_file_doc(
            path, repo_root, check_only=check_only
        )
        symbol_count = ensure_cpp_symbol_docs(path, check_only=check_only)

    return changed or symbol_count > 0, symbol_count


def parse_args() -> argparse.Namespace:
    """Parse command-line options."""
    parser = argparse.ArgumentParser(
        description=(
            "Add the CEngine banner and standard placeholder documentation "
            "to project source files."
        )
    )
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument(
        "--check",
        action="store_true",
        help=(
            "report missing headers or documentation and return a non-zero "
            "status without editing files"
        ),
    )
    mode.add_argument(
        "--dry-run",
        action="store_true",
        help="report files that would change without editing them",
    )
    return parser.parse_args()


def main() -> int:
    """Apply or check managed source documentation."""
    args = parse_args()
    repo_root = Path(__file__).resolve().parents[1]
    check_only = args.check or args.dry_run
    results = [
        (path, *document_source(path, repo_root, check_only=check_only))
        for path in source_files(repo_root)
    ]
    changed = [path for path, needs_changes, _ in results if needs_changes]
    symbol_count = sum(count for _, _, count in results)

    action = "Missing documentation" if check_only else "Updated documentation"
    for path in changed:
        print(f"{action}: {path.relative_to(repo_root)}")

    if args.check and changed:
        print(
            f"{len(changed)} source file(s) are missing managed documentation; "
            f"{symbol_count} symbol docstring(s) are required."
        )
        return 1

    if args.check:
        print("All source files and symbols have managed documentation.")
    elif args.dry_run:
        print(
            f"{len(changed)} source file(s) and {symbol_count} symbol(s) "
            "would be updated."
        )
    else:
        print(
            f"Updated {len(changed)} source file(s) and {symbol_count} "
            "symbol docstring(s)."
        )
    return 0


if __name__ == "__main__":
    sys.exit(main())
