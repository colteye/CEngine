#!/usr/bin/env python3
#   _____ ______             _
#  / ____|  ____|           (_)
# | |    | |__   _ __   __ _ _ _ __   ___
# | |    |  __| | '_ \ / _` | | '_ \ / _ \
# | |____| |____| | | | (_| | | | | |  __/
#  \_____|______|_| |_|\__, |_|_| |_|\___|
#                       __/ |
#                      |___/

"""Generate tiny, dependency-free C++ readers from a CEngine game file.

Author:
    Erik Coltey"""

from __future__ import annotations

import argparse
import json
import math
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Any


FORMAT_VERSION = 1
INVALID_INDEX = 0xFFFFFFFF
WIRE_SIZES = {
    "f32": 4,
    "u32": 4,
    "entity": 4,
    "bool": 4,
    "enum": 4,
    "vec2": 8,
    "vec3": 12,
    "transform": 40,
    "asset": 4,
    "asset_list": 8,
    "flags": 4,
}

RECORD_SCALARS = {"u8", "u16", "u32", "u64", "i32", "f32", "string", "bytes", "guid"}


@dataclass(frozen=True)
class LoadedManifest:
    """TODO: Describe `LoadedManifest`."""

    path: Path
    document: dict[str, Any]


def _read_manifest(path: Path) -> dict[str, Any]:
    """TODO: Describe `_read_manifest`.

    Args:
        path: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    try:
        document = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ValueError(f"cannot read game file {path}: {error}") from error
    if not isinstance(document, dict) or document.get("format_version") != FORMAT_VERSION:
        raise ValueError(f"unsupported game file: {path}")
    module = document.get("module")
    if not isinstance(module, dict) or not isinstance(module.get("id"), str):
        raise ValueError(f"game file has no module id: {path}")
    return document


def _load_manifest_tree(root: Path) -> list[LoadedManifest]:
    """TODO: Describe `_load_manifest_tree`.

    Args:
        root: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    ordered: list[LoadedManifest] = []
    visiting: set[Path] = set()
    loaded: set[Path] = set()

    def visit(path: Path) -> None:
        """TODO: Describe `visit`.

        Args:
            path: TODO: Describe this parameter.
        """
        resolved = path.resolve()
        if resolved in visiting:
            raise ValueError(f"game file import cycle contains {resolved}")
        if resolved in loaded:
            return
        visiting.add(resolved)
        document = _read_manifest(resolved)
        imports = document.get("imports", [])
        if not isinstance(imports, list) or any(not isinstance(item, str) for item in imports):
            raise ValueError(f"game file imports must be strings: {resolved}")
        for item in imports:
            visit(resolved.parent / item)
        visiting.remove(resolved)
        loaded.add(resolved)
        ordered.append(LoadedManifest(resolved, document))

    visit(root)
    return ordered


def _identifier(value: str) -> str:
    """TODO: Describe `_identifier`.

    Args:
        value: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    result = re.sub(r"[^A-Za-z0-9_]", "_", value)
    if not result or result[0].isdigit():
        result = f"_{result}"
    return result


def _pascal(value: str) -> str:
    """TODO: Describe `_pascal`.

    Args:
        value: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    words = [word for word in re.split(r"[^A-Za-z0-9]+", value) if word]
    result = "".join(word[0].upper() + word[1:] for word in words)
    if not result or result[0].isdigit():
        result = f"_{result}"
    return result


def _require_string(value: Any, label: str) -> str:
    """TODO: Describe `_require_string`.

    Args:
        value: TODO: Describe this parameter.
        label: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    if not isinstance(value, str) or not value:
        raise ValueError(f"{label} must be a non-empty string")
    return value


def _validate_field(field: dict[str, Any], entity: str) -> None:
    """TODO: Describe `_validate_field`.

    Args:
        field: TODO: Describe this parameter.
        entity: TODO: Describe this parameter.
    """
    name = _require_string(field.get("name"), f"{entity} field name")
    if _identifier(name) != name:
        raise ValueError(f"{entity}.{name} is not a C++ identifier")
    field_type = _require_string(field.get("type"), f"{entity}.{name} field type")
    if field_type not in WIRE_SIZES:
        raise ValueError(f"{entity}.{name} uses unsupported type {field_type}")
    if field_type == "flags":
        members = field.get("members")
        if not isinstance(members, list) or not members:
            raise ValueError(f"{entity}.{name} flags must contain members")
        bits: set[int] = set()
        for member in members:
            if not isinstance(member, dict):
                raise ValueError(f"{entity}.{name} flag member must be an object")
            member_name = _require_string(member.get("name"), f"{entity}.{name} flag name")
            if _identifier(member_name) != member_name:
                raise ValueError(f"{entity}.{name}.{member_name} is not a C++ identifier")
            bit = member.get("bit")
            if not isinstance(bit, int) or bit < 0 or bit > 31 or bit in bits:
                raise ValueError(f"{entity}.{name} has an invalid or duplicate flag bit")
            bits.add(bit)
    if field_type == "enum":
        _require_string(field.get("enum"), f"{entity}.{name} enum name")
        values = field.get("values")
        if not isinstance(values, dict) or not values:
            raise ValueError(f"{entity}.{name} enum must define values")
        if any(
            not isinstance(key, str) or _identifier(key) != key or not isinstance(value, int)
            for key, value in values.items()
        ):
            raise ValueError(f"{entity}.{name} enum values are invalid")
    if field_type in ("asset", "asset_list"):
        _require_string(field.get("asset_type"), f"{entity}.{name} asset type")


def _validate_entity(entity: dict[str, Any], source: Path) -> None:
    """TODO: Describe `_validate_entity`.

    Args:
        entity: TODO: Describe this parameter.
        source: TODO: Describe this parameter.
    """
    classname = _require_string(entity.get("classname"), f"entity classname in {source}")
    if not isinstance(entity.get("version"), int) or entity["version"] <= 0:
        raise ValueError(f"{classname} must have a positive version")
    fields = entity.get("fields")
    if not isinstance(fields, list) or not fields:
        raise ValueError(f"{classname} must define fields")
    seen: set[str] = set()
    logical: set[str] = set()
    for field in fields:
        if not isinstance(field, dict):
            raise ValueError(f"{classname} field must be an object")
        _validate_field(field, classname)
        name = field["name"]
        if name in seen:
            raise ValueError(f"{classname} repeats field {name}")
        seen.add(name)
        names = (
            {member["name"] for member in field["members"]}
            if field["type"] == "flags"
            else {name}
        )
        overlap = logical.intersection(names)
        if overlap:
            raise ValueError(f"{classname} repeats property {sorted(overlap)[0]}")
        logical.update(names)
    transforms = [field for field in fields if field["type"] == "transform"]
    if len(transforms) != 1 or transforms[0]["name"] != "transform":
        raise ValueError(f"{classname} must define exactly one transform field")


def _validate_wire_field(field: dict[str, Any], record: str) -> None:
    """Validate one field in an owned cooked-wire record."""
    name = _require_string(field.get("name"), f"{record} wire field name")
    if _identifier(name) != name:
        raise ValueError(f"{record}.{name} is not a C++ identifier")
    field_type = _require_string(field.get("type"), f"{record}.{name} wire field type")
    if field_type not in RECORD_SCALARS | {"array", "list", "record"}:
        raise ValueError(f"{record}.{name} uses unsupported wire type {field_type}")
    if field_type in {"array", "list"}:
        item = _require_string(field.get("item"), f"{record}.{name} item type")
        if item not in RECORD_SCALARS and _identifier(item) != item:
            raise ValueError(f"{record}.{name} has an invalid item type")
    if field_type == "array":
        count = field.get("count")
        if not isinstance(count, int) or count <= 0:
            raise ValueError(f"{record}.{name} array count must be positive")
    if field_type == "list":
        maximum = field.get("max_count")
        if not isinstance(maximum, int) or maximum <= 0:
            raise ValueError(f"{record}.{name} list requires a positive max_count")
    if field_type == "record":
        _require_string(field.get("record"), f"{record}.{name} record type")
    if field_type in {"string", "bytes"}:
        maximum = field.get("max_size")
        if not isinstance(maximum, int) or maximum <= 0:
            raise ValueError(f"{record}.{name} requires a positive max_size")


def _validate_wire_record(record: dict[str, Any], source: Path) -> None:
    """Validate one schema-owned cooked-wire record."""
    name = _require_string(record.get("name"), f"wire record name in {source}")
    if _identifier(name) != name:
        raise ValueError(f"wire record {name} is not an identifier")
    fields = record.get("fields")
    if not isinstance(fields, list) or not fields:
        raise ValueError(f"wire record {name} must define fields")
    names: set[str] = set()
    for field in fields:
        if not isinstance(field, dict):
            raise ValueError(f"wire record {name} field must be an object")
        _validate_wire_field(field, name)
        if field["name"] in names:
            raise ValueError(f"wire record {name} repeats field {field['name']}")
        names.add(field["name"])


def load_game(path: Path) -> tuple[dict[str, Any], dict[str, Any], list[dict[str, Any]], list[dict[str, Any]]]:
    """TODO: Describe `load_game`.

    Args:
        path: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    manifests = _load_manifest_tree(path)
    root = manifests[-1].document
    entities: list[dict[str, Any]] = []
    asset_types: list[dict[str, Any]] = []
    wire_records: list[dict[str, Any]] = []
    entity_names: set[str] = set()
    asset_names: set[str] = set()
    extensions: set[str] = set()
    record_names: set[str] = set()
    for manifest in manifests:
        owner = manifest.document["module"]["id"]
        for entity in manifest.document.get("entities", []):
            if not isinstance(entity, dict):
                raise ValueError(f"entity entry must be an object: {manifest.path}")
            _validate_entity(entity, manifest.path)
            classname = entity["classname"]
            if classname in entity_names:
                raise ValueError(f"duplicate entity classname {classname}")
            entity_names.add(classname)
            entities.append({**entity, "owner": owner})
        for asset_type in manifest.document.get("asset_types", []):
            if not isinstance(asset_type, dict):
                raise ValueError(f"asset type entry must be an object: {manifest.path}")
            name = _require_string(asset_type.get("name"), "asset type name")
            extension = _require_string(asset_type.get("extension"), f"{name} extension")
            if name in asset_names or extension in extensions:
                raise ValueError(f"duplicate asset type or extension: {name} {extension}")
            asset_names.add(name)
            extensions.add(extension)
            asset_types.append({**asset_type, "owner": owner})
        for record in manifest.document.get("wire_records", []):
            if not isinstance(record, dict):
                raise ValueError(f"wire record entry must be an object: {manifest.path}")
            _validate_wire_record(record, manifest.path)
            name = record["name"]
            if name in record_names:
                raise ValueError(f"duplicate wire record {name}")
            record_names.add(name)
            wire_records.append({**record, "owner": owner})
    for record in wire_records:
        for field in record["fields"]:
            referenced: str | None = None
            if field["type"] == "record":
                referenced = field["record"]
            elif field["type"] in {"array", "list"} and field["item"] not in RECORD_SCALARS:
                referenced = field["item"]
            if referenced is not None and referenced not in record_names:
                raise ValueError(f"{record['name']}.{field['name']} references unknown record {referenced}")
    flattened = {
        "format_version": FORMAT_VERSION,
        "game": root.get("game", root["module"]),
        "content": root.get("content", {}),
        "asset_types": asset_types,
        "wire_records": wire_records,
        "entities": entities,
    }
    return root, flattened, list(root.get("entities", [])), list(root.get("wire_records", []))


def _cpp_float(value: Any) -> str:
    """TODO: Describe `_cpp_float`.

    Args:
        value: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    number = float(value)
    if not math.isfinite(number):
        raise ValueError("schema defaults must be finite")
    text = repr(number)
    if "." not in text and "e" not in text.lower():
        text += ".0"
    return f"{text}f"


def _enum_default(field: dict[str, Any]) -> str:
    """TODO: Describe `_enum_default`.

    Args:
        field: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    default = int(field.get("default", next(iter(field["values"].values()))))
    for name, value in field["values"].items():
        if value == default:
            return f"{field['enum']}::{name}"
    raise ValueError(f"{field['name']} enum default is not a declared value")


def _field_default(field: dict[str, Any]) -> str:
    """TODO: Describe `_field_default`.

    Args:
        field: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    field_type = field["type"]
    value = field.get("default")
    if field_type == "f32":
        return _cpp_float(0.0 if value is None else value)
    if field_type == "u32":
        return f"{int(0 if value is None else value)}u"
    if field_type == "entity":
        return "InvalidIndex" if value is None else f"{int(value)}u"
    if field_type == "bool":
        return "true" if bool(value) else "false"
    if field_type == "enum":
        return _enum_default(field)
    if field_type == "vec2":
        values = value or [0.0, 0.0]
        return f"{{{_cpp_float(values[0])}, {_cpp_float(values[1])}}}"
    if field_type == "vec3":
        values = value or [0.0, 0.0, 0.0]
        return (
            f"{{{_cpp_float(values[0])}, {_cpp_float(values[1])}, "
            f"{_cpp_float(values[2])}}}"
        )
    if field_type in ("asset", "asset_list", "transform"):
        return "{}"
    raise ValueError(f"no default for {field_type}")


def _field_type(field: dict[str, Any]) -> str:
    """TODO: Describe `_field_type`.

    Args:
        field: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    return {
        "f32": "float",
        "u32": "std::uint32_t",
        "entity": "std::uint32_t",
        "bool": "bool",
        "enum": field.get("enum", ""),
        "vec2": "Vec2",
        "vec3": "Vec3",
        "transform": "Transform",
        "asset": "Asset",
        "asset_list": "AssetList",
    }[field["type"]]


def _record_size(entity: dict[str, Any]) -> int:
    """TODO: Describe `_record_size`.

    Args:
        entity: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    return sum(WIRE_SIZES[field["type"]] for field in entity["fields"])


def _bounds(field: dict[str, Any], expression: str) -> list[str]:
    """TODO: Describe `_bounds`.

    Args:
        field: TODO: Describe this parameter.
        expression: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    checks = [f"!CEngine::Assets::IsFinite({expression})"]
    if "min" in field:
        op = "<=" if field.get("min_exclusive", False) else "<"
        checks.append(f"{expression} {op} {_cpp_float(field['min'])}")
    if "max" in field:
        op = ">=" if field.get("max_exclusive", False) else ">"
        checks.append(f"{expression} {op} {_cpp_float(field['max'])}")
    return checks


def _emit_read(lines: list[str], entity: dict[str, Any]) -> None:
    """TODO: Describe `_emit_read`.

    Args:
        lines: TODO: Describe this parameter.
        entity: TODO: Describe this parameter.
    """
    symbol = _pascal(entity["classname"])
    lines.extend(
        [
            f"inline constexpr std::size_t {symbol}Size = {_record_size(entity)}u;",
            f"inline bool Read(const std::uint8_t* bytes, std::size_t size,",
            f"    std::uint32_t auxiliary_base, {symbol}& output)",
            "{",
            f"    if (bytes == nullptr || size != {symbol}Size) return false;",
            f"    {symbol} value{{}};",
            "    CEngine::Assets::Reader reader(std::span(bytes, size));",
            "    (void)auxiliary_base;",
        ]
    )
    final_checks: list[str] = []
    for field in entity["fields"]:
        name = field["name"]
        field_type = field["type"]
        target = f"value.properties.{name}"
        if field_type == "transform":
            for index in range(3):
                lines.append(
                    f"    if (!reader.F32("
                    f"value.transform.position[{index}])) return false;"
                )
                final_checks.append(f"!CEngine::Assets::IsFinite(value.transform.position[{index}])")
            for index in range(4):
                lines.append(
                    f"    if (!reader.F32("
                    f"value.transform.rotation[{index}])) return false;"
                )
                final_checks.append(f"!CEngine::Assets::IsFinite(value.transform.rotation[{index}])")
            for index in range(3):
                lines.append(
                    f"    if (!reader.F32("
                    f"value.transform.scale[{index}])) return false;"
                )
                final_checks.append(f"!CEngine::Assets::IsFinite(value.transform.scale[{index}])")
        elif field_type == "f32":
            lines.append(
                f"    if (!reader.F32({target})) return false;"
            )
            final_checks.extend(_bounds(field, target))
        elif field_type in ("u32", "entity"):
            lines.append(
                f"    if (!reader.U32({target})) return false;"
            )
            if field_type == "entity" and not field.get("optional", False):
                final_checks.append(f"{target} == InvalidIndex")
            if field_type == "u32":
                if "min" in field:
                    final_checks.append(
                        f"{target} < {int(field['min'])}u")
                if "max" in field:
                    final_checks.append(
                        f"{target} > {int(field['max'])}u")
        elif field_type == "bool":
            lines.extend(
                [
                    f"    std::uint32_t {name}_value = 0;",
                    f"    if (!reader.U32({name}_value) ||",
                    f"        {name}_value > 1u) return false;",
                    f"    {target} = {name}_value != 0u;",
                ]
            )
        elif field_type == "enum":
            allowed = " && ".join(
                f"{name}_value != {int(value)}u"
                for value in sorted(set(field["values"].values()))
            )
            lines.extend(
                [
                    f"    std::uint32_t {name}_value = 0;",
                    f"    if (!reader.U32({name}_value) ||",
                    f"        ({allowed})) return false;",
                    f"    {target} = static_cast<{field['enum']}>({name}_value);",
                ]
            )
        elif field_type in ("vec2", "vec3"):
            components = ("x", "y") if field_type == "vec2" else ("x", "y", "z")
            for component in components:
                lines.append(
                    f"    if (!reader.F32("
                    f"{target}.{component})) return false;"
                )
                final_checks.extend(_bounds(field, f"{target}.{component}"))
        elif field_type == "asset":
            lines.append(
                f"    if (!reader.U32({target}.index)) "
                "return false;"
            )
            if not field.get("optional", False):
                final_checks.append(f"{target}.index == InvalidIndex")
        elif field_type == "asset_list":
            lines.extend(
                [
                    f"    if (!reader.U32({target}.first) ||",
                    f"        !reader.U32({target}.count)) "
                    "return false;",
                    f"    if ({target}.count == 0u) {target}.first = 0u;",
                    f"    else if ({target}.first == InvalidIndex ||",
                    f"        {target}.first > InvalidIndex - auxiliary_base) return false;",
                    f"    else {target}.first += auxiliary_base;",
                ]
            )
        elif field_type == "flags":
            mask = sum(1 << int(member["bit"]) for member in field["members"])
            lines.extend(
                [
                    f"    std::uint32_t {name}_value = 0;",
                    f"    if (!reader.U32({name}_value) ||",
                    f"        ({name}_value & ~0x{mask:x}u) != 0u) return false;",
                ]
            )
            for member in field["members"]:
                lines.append(
                    f"    value.properties.{member['name']} = "
                    f"({name}_value & (1u << {int(member['bit'])}u)) != 0u;"
                )
    for field in entity["fields"]:
        if "greater_than" in field:
            final_checks.append(
                f"value.properties.{field['name']} <= "
                f"value.properties.{field['greater_than']}"
            )
    if final_checks:
        lines.append("    if (" + "\n        || ".join(final_checks) + ") return false;")
    lines.extend(["    if (!reader.Done()) return false;", "    output = value;", "    return true;", "}", ""])


def _wire_cpp_type(field_type: str) -> str:
    """Return the generated C++ host type for a wire scalar or record name."""
    scalar = {
        "u8": "std::uint8_t",
        "u16": "std::uint16_t",
        "u32": "std::uint32_t",
        "u64": "std::uint64_t",
        "i32": "std::int32_t",
        "f32": "float",
        "string": "std::string",
        "bytes": "std::vector<std::byte>",
        "guid": "std::array<std::uint8_t, 16>",
    }
    return scalar.get(field_type, _pascal(field_type))


def _wire_field_type(field: dict[str, Any]) -> str:
    """Return the complete generated C++ type for a wire field."""
    field_type = field["type"]
    if field_type == "record":
        return _pascal(field["record"])
    if field_type == "array":
        return f"std::array<{_wire_cpp_type(field['item'])}, {int(field['count'])}>"
    if field_type == "list":
        return f"std::vector<{_wire_cpp_type(field['item'])}>"
    return _wire_cpp_type(field_type)


def _emit_wire_checks(
    lines: list[str], field: dict[str, Any], expression: str, indent: str
) -> None:
    """Emit schema constraints after one scalar has been decoded."""
    checks: list[str] = []
    if field.get("finite", False) or field["type"] == "f32":
        checks.append(f"!CEngine::Assets::IsFinite({expression})")
    if "value" in field:
        checks.append(f"{expression} != {field['value']}")
    if "min" in field:
        checks.append(f"{expression} < {field['min']}")
    if "max" in field:
        checks.append(f"{expression} > {field['max']}")
    if "mask" in field:
        checks.append(f"({expression} & ~{field['mask']}u) != 0u")
    if "values" in field:
        checks.append(
            "("
            + " && ".join(f"{expression} != {value}" for value in field["values"])
            + ")"
        )
    if checks:
        lines.append(f"{indent}if (" + " || ".join(checks) + ") return false;")


def _emit_wire_value(
    lines: list[str],
    field: dict[str, Any],
    expression: str,
    token: str,
    indent: str = "    ",
) -> None:
    """Emit one generated read operation, including nested owned containers."""
    field_type = field["type"]
    methods = {
        "u8": "U8",
        "u16": "U16",
        "u32": "U32",
        "u64": "U64",
        "i32": "I32",
        "f32": "F32",
    }
    if field_type in methods:
        lines.append(f"{indent}if (!reader.{methods[field_type]}({expression})) return false;")
        _emit_wire_checks(lines, field, expression, indent)
        return
    if field_type == "guid":
        lines.append(
            f"{indent}if (!reader.Read(std::as_writable_bytes(std::span({expression})))) return false;"
        )
        return
    if field_type in {"string", "bytes"}:
        size = f"{token}_size"
        data = f"{token}_bytes"
        lines.append(f"{indent}std::uint32_t {size} = 0;")
        lines.append(
            f"{indent}if (!reader.U32({size}) || {size} > {int(field['max_size'])}u) return false;"
        )
        lines.append(f"{indent}const auto {data} = reader.Take({size});")
        lines.append(f"{indent}if ({data}.size() != {size}) return false;")
        if field_type == "string":
            lines.append(
                f"{indent}{expression}.assign(reinterpret_cast<const char*>({data}.data()), {data}.size());"
            )
            if field.get("non_empty", False):
                lines.append(f"{indent}if ({expression}.empty()) return false;")
        else:
            lines.append(f"{indent}{expression}.assign({data}.begin(), {data}.end());")
        return
    if field_type == "record":
        lines.append(f"{indent}if (!Read(reader, {expression})) return false;")
        return
    if field_type == "array":
        index = f"{token}_index"
        lines.append(f"{indent}for (std::size_t {index} = 0; {index} < {expression}.size(); ++{index}) {{")
        nested = {**field, "type": field["item"]}
        _emit_wire_value(lines, nested, f"{expression}[{index}]", f"{token}_item", indent + "    ")
        lines.append(f"{indent}}}")
        return
    if field_type == "list":
        count = f"{token}_count"
        index = f"{token}_index"
        item = f"{token}_item"
        lines.append(f"{indent}std::uint32_t {count} = 0;")
        minimum = int(field.get("min_count", 0))
        lines.append(
            f"{indent}if (!reader.U32({count}) || {count} < {minimum}u || "
            f"{count} > {int(field['max_count'])}u) return false;"
        )
        lines.append(f"{indent}{expression}.clear();")
        lines.append(f"{indent}{expression}.reserve({count});")
        lines.append(f"{indent}for (std::uint32_t {index} = 0; {index} < {count}; ++{index}) {{")
        lines.append(f"{indent}    {_wire_cpp_type(field['item'])} {item}{{}};")
        nested = {**field, "type": field["item"]}
        _emit_wire_value(lines, nested, item, f"{token}_item", indent + "    ")
        lines.append(f"{indent}    {expression}.push_back(std::move({item}));")
        lines.append(f"{indent}}}")
        return
    if field_type not in RECORD_SCALARS:
        lines.append(f"{indent}if (!Read(reader, {expression})) return false;")
        return
    raise ValueError(f"cannot emit wire type {field_type}")


def _emit_wire_records(lines: list[str], records: list[dict[str, Any]]) -> None:
    """Emit owned schema records and their shared-runtime readers."""
    if not records:
        return
    lines.append("namespace Wire {")
    lines.append("")
    for record in records:
        symbol = _pascal(record["name"])
        lines.append(f"struct {symbol} {{")
        for field in record["fields"]:
            lines.append(f"    {_wire_field_type(field)} {field['name']} = {{}};")
        lines.extend(["};", ""])
    for record in records:
        symbol = _pascal(record["name"])
        lines.append(f"inline bool Read(CEngine::Assets::Reader& reader, {symbol}& output)")
        lines.append("{")
        lines.append(f"    {symbol} value{{}};")
        for field in record["fields"]:
            _emit_wire_value(
                lines,
                field,
                f"value.{field['name']}",
                f"{record['name']}_{field['name']}",
            )
        lines.extend(["    output = std::move(value);", "    return true;", "}", ""])
        lines.append(f"inline bool Read(std::span<const std::byte> bytes, {symbol}& output)")
        lines.append("{")
        lines.append("    CEngine::Assets::Reader reader(bytes);")
        lines.append("    return Read(reader, output) && reader.Done();")
        lines.extend(["}", ""])
    lines.extend(["} // namespace Wire", ""])


def generate_cpp(
    root: dict[str, Any],
    entities: list[dict[str, Any]],
    wire_records: list[dict[str, Any]],
    namespace: str,
    header_path: Path,
) -> None:
    """TODO: Describe `generate_cpp`.

    Args:
        root: TODO: Describe this parameter.
        entities: TODO: Describe this parameter.
        namespace: TODO: Describe this parameter.
        header_path: TODO: Describe this parameter.
    """
    namespace_parts = namespace.split("::")
    if any(not part or _identifier(part) != part for part in namespace_parts):
        raise ValueError(f"invalid C++ namespace: {namespace}")

    enums: dict[str, dict[str, int]] = {}
    for entity in entities:
        for field in entity["fields"]:
            if field["type"] != "enum":
                continue
            name = field["enum"]
            values = field["values"]
            if name in enums and enums[name] != values:
                raise ValueError(f"enum {name} has conflicting definitions")
            enums[name] = values

    guard = _identifier(f"{root['module']['id']}_entities_generated_h").upper()
    lines = [
        f"#ifndef {guard}",
        f"#define {guard}",
        "",
        "#include <cstddef>",
        "#include <cstdint>",
        "#include <array>",
        "#include <span>",
        "#include <string>",
        "#include <utility>",
        "#include <vector>",
        '#include "assets/reader.h"',
        "",
    ]
    lines.extend(f"namespace {part} {{" for part in namespace_parts)
    lines.extend(
        [
            "",
            "inline constexpr std::uint32_t InvalidIndex = 0xffffffffu;",
            "",
            "struct Vec2 { float x = 0.0f; float y = 0.0f; };",
            "struct Vec3 { float x = 0.0f; float y = 0.0f; float z = 0.0f; };",
            "struct Transform {",
            "    float position[3] = {0.0f, 0.0f, 0.0f};",
            "    float rotation[4] = {0.0f, 0.0f, 0.0f, 1.0f};",
            "    float scale[3] = {1.0f, 1.0f, 1.0f};",
            "};",
            "struct Asset {",
            "    std::uint32_t index = InvalidIndex;",
            "    constexpr explicit operator bool() const { return index != InvalidIndex; }",
            "};",
            "struct AssetList { std::uint32_t first = 0; std::uint32_t count = 0; };",
            "template <typename PropertyType, std::uint16_t VersionValue>",
            "struct EntityData {",
            "    using Properties = PropertyType;",
            "    static constexpr std::uint16_t Version = VersionValue;",
            "    Transform transform = {};",
            "    PropertyType properties = {};",
            "};",
            "",
        ]
    )
    _emit_wire_records(lines, wire_records)
    for name, values in enums.items():
        members = ", ".join(f"{key} = {value}" for key, value in values.items())
        lines.append(f"enum class {name} : std::uint32_t {{ {members} }};")
    if enums:
        lines.append("")
    for entity in entities:
        symbol = _pascal(entity["classname"])
        lines.append(f"struct {symbol}Properties {{")
        for field in entity["fields"]:
            if field["type"] == "transform":
                continue
            if field["type"] == "flags":
                for member in field["members"]:
                    default = "true" if bool(member.get("default", False)) else "false"
                    lines.append(f"    bool {member['name']} = {default};")
            else:
                lines.append(
                    f"    {_field_type(field)} {field['name']} = {_field_default(field)};"
                )
        lines.extend([
            "};",
            f"using {symbol} = "
            f"EntityData<{symbol}Properties, {int(entity['version'])}u>;",
            "",
        ])
    for entity in entities:
        _emit_read(lines, entity)
    lines.extend(
        f"}} // namespace {part}" for part in reversed(namespace_parts)
    )
    lines.extend(["", f"#endif // {guard}", ""])
    header_path.parent.mkdir(parents=True, exist_ok=True)
    header_path.write_text("\n".join(lines), encoding="utf-8")


def write_bundle(bundle: dict[str, Any], path: Path) -> None:
    """TODO: Describe `write_bundle`.

    Args:
        bundle: TODO: Describe this parameter.
        path: TODO: Describe this parameter.
    """
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(bundle, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def main(argv: list[str] | None = None) -> int:
    """TODO: Describe `main`.

    Args:
        argv: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    parser = argparse.ArgumentParser(
        description="Generate CEngine schema readers and a flattened game file."
    )
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--cpp-header", type=Path)
    parser.add_argument("--namespace")
    parser.add_argument("--bundle", type=Path)
    args = parser.parse_args(argv)
    if (args.cpp_header is None) != (args.namespace is None):
        parser.error("--cpp-header and --namespace must be supplied together")
    if args.cpp_header is None and args.bundle is None:
        parser.error("at least one output must be requested")
    try:
        root, bundle, owned_entities, owned_wire_records = load_game(args.manifest)
        if args.cpp_header is not None:
            generate_cpp(root, owned_entities, owned_wire_records, args.namespace, args.cpp_header)
        if args.bundle is not None:
            write_bundle(bundle, args.bundle)
    except ValueError as error:
        parser.error(str(error))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
