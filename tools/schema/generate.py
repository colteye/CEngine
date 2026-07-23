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


def load_game(path: Path) -> tuple[dict[str, Any], dict[str, Any], list[dict[str, Any]]]:
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
    entity_names: set[str] = set()
    asset_names: set[str] = set()
    extensions: set[str] = set()
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
    flattened = {
        "format_version": FORMAT_VERSION,
        "game": root.get("game", root["module"]),
        "content": root.get("content", {}),
        "asset_types": asset_types,
        "entities": entities,
    }
    return root, flattened, list(root.get("entities", []))


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
    checks = [f"!Detail::IsFinite({expression})"]
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
            "    std::size_t offset = 0;",
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
                    f"    if (!Detail::ReadF32(bytes, size, offset, "
                    f"value.transform.position[{index}])) return false;"
                )
                final_checks.append(f"!Detail::IsFinite(value.transform.position[{index}])")
            for index in range(4):
                lines.append(
                    f"    if (!Detail::ReadF32(bytes, size, offset, "
                    f"value.transform.rotation[{index}])) return false;"
                )
                final_checks.append(f"!Detail::IsFinite(value.transform.rotation[{index}])")
            for index in range(3):
                lines.append(
                    f"    if (!Detail::ReadF32(bytes, size, offset, "
                    f"value.transform.scale[{index}])) return false;"
                )
                final_checks.append(f"!Detail::IsFinite(value.transform.scale[{index}])")
        elif field_type == "f32":
            lines.append(
                f"    if (!Detail::ReadF32(bytes, size, offset, {target})) return false;"
            )
            final_checks.extend(_bounds(field, target))
        elif field_type in ("u32", "entity"):
            lines.append(
                f"    if (!Detail::ReadU32(bytes, size, offset, {target})) return false;"
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
                    f"    if (!Detail::ReadU32(bytes, size, offset, {name}_value) ||",
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
                    f"    if (!Detail::ReadU32(bytes, size, offset, {name}_value) ||",
                    f"        ({allowed})) return false;",
                    f"    {target} = static_cast<{field['enum']}>({name}_value);",
                ]
            )
        elif field_type in ("vec2", "vec3"):
            components = ("x", "y") if field_type == "vec2" else ("x", "y", "z")
            for component in components:
                lines.append(
                    f"    if (!Detail::ReadF32(bytes, size, offset, "
                    f"{target}.{component})) return false;"
                )
                final_checks.extend(_bounds(field, f"{target}.{component}"))
        elif field_type == "asset":
            lines.append(
                f"    if (!Detail::ReadU32(bytes, size, offset, {target}.index)) "
                "return false;"
            )
            if not field.get("optional", False):
                final_checks.append(f"{target}.index == InvalidIndex")
        elif field_type == "asset_list":
            lines.extend(
                [
                    f"    if (!Detail::ReadU32(bytes, size, offset, {target}.first) ||",
                    f"        !Detail::ReadU32(bytes, size, offset, {target}.count)) "
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
                    f"    if (!Detail::ReadU32(bytes, size, offset, {name}_value) ||",
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
    lines.extend(["    output = value;", "    return true;", "}", ""])


def generate_cpp(
    root: dict[str, Any],
    entities: list[dict[str, Any]],
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
        "#include <cstring>",
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
    lines.extend(
        [
            "namespace Detail {",
            "inline bool ReadU32(const std::uint8_t* bytes, std::size_t size,",
            "    std::size_t& offset, std::uint32_t& value)",
            "{",
            "    if (offset > size || size - offset < 4u) return false;",
            "    value = static_cast<std::uint32_t>(bytes[offset])",
            "        | (static_cast<std::uint32_t>(bytes[offset + 1u]) << 8u)",
            "        | (static_cast<std::uint32_t>(bytes[offset + 2u]) << 16u)",
            "        | (static_cast<std::uint32_t>(bytes[offset + 3u]) << 24u);",
            "    offset += 4u;",
            "    return true;",
            "}",
            "inline bool ReadF32(const std::uint8_t* bytes, std::size_t size,",
            "    std::size_t& offset, float& value)",
            "{",
            "    std::uint32_t bits = 0;",
            "    if (!ReadU32(bytes, size, offset, bits)) return false;",
            "    std::memcpy(&value, &bits, sizeof(value));",
            "    return true;",
            "}",
            "inline bool IsFinite(float value)",
            "{",
            "    std::uint32_t bits = 0;",
            "    std::memcpy(&bits, &value, sizeof(bits));",
            "    return (bits & 0x7f800000u) != 0x7f800000u;",
            "}",
            "} // namespace Detail",
            "",
        ]
    )
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
        root, bundle, owned_entities = load_game(args.manifest)
        if args.cpp_header is not None:
            generate_cpp(root, owned_entities, args.namespace, args.cpp_header)
        if args.bundle is not None:
            write_bundle(bundle, args.bundle)
    except ValueError as error:
        parser.error(str(error))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
