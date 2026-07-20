from __future__ import annotations

import struct
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Iterable

from ceassetlib.assetfile import make_asset_desc, write_binary_asset
from ceassetlib.collection_export import clean_asset_name
from ceassetlib.formats import AssetType
from ceassetlib.ids import guid_from_stable_name, hash_file
from ceassetlib.paths import generic_path, output_dir_for_source


MATERIAL_TEXTURE_SLOTS = {
    "base color": "base_color",
    "alpha": "opacity",
    "emission": "emissive",
    "emission color": "emissive",
    "metallic": "metallic",
    "roughness": "roughness",
    "normal": "normal",
}

MATERIAL_HEADER = struct.Struct("<4sHHIIIIIII")
MATERIAL_TEXTURE = struct.Struct("<III")
MATERIAL_MAGIC = b"CEMA"
MATERIAL_VERSION = 1
MATERIAL_SHADER_IDS = {
    "pbr": 1,
    "pbr_standard": 1,
    "unlit": 2,
}
MATERIAL_TEXTURE_SLOT_IDS = {
    "base_color": 1,
    "normal": 2,
    "metallic_roughness_ao": 3,
    "orm": 3,
    "roughness": 4,
    "metallic": 5,
    "opacity": 6,
    "emissive": 7,
}


@dataclass(frozen=True)
class TextureBinding:
    slot: str
    source: Path
    output: Path


@dataclass(frozen=True)
class MaterialExport:
    source: object
    output: Path


def material_output_path(blend_source: Path, output_root: Path, material_name: str) -> Path:
    return output_dir_for_source(blend_source, output_root) / "materials" / f"{clean_asset_name(material_name)}.cmat"


def object_materials(objects: Iterable[object]) -> list[object]:
    materials: list[object] = []
    seen: set[int] = set()
    for obj in objects:
        for slot in getattr(obj, "material_slots", ()):
            material = getattr(slot, "material", None)
            if material is None:
                continue
            key = id(material)
            if key in seen:
                continue
            seen.add(key)
            materials.append(material)
    return sorted(materials, key=lambda material: material.name)


def texture_slot_name(socket_name: str, node_type: str = "") -> str:
    if node_type == "NORMAL_MAP":
        return "normal"
    return MATERIAL_TEXTURE_SLOTS.get(socket_name.strip().lower(), "")


def material_texture_bindings(
    material: object,
    image_source_path: Callable[[object], Path | None],
    texture_output_path_for_source: Callable[[Path], Path | None],
) -> list[TextureBinding]:
    node_tree = getattr(material, "node_tree", None)
    links = getattr(node_tree, "links", ()) if node_tree is not None else ()
    bindings: list[TextureBinding] = []
    seen: set[tuple[str, Path]] = set()

    for link in links:
        from_node = getattr(link, "from_node", None)
        if getattr(from_node, "type", "") != "TEX_IMAGE":
            continue
        image = getattr(from_node, "image", None)
        if image is None:
            continue

        to_socket = getattr(link, "to_socket", None)
        to_node = getattr(link, "to_node", None)
        slot = texture_slot_name(
            str(getattr(to_socket, "name", "")),
            str(getattr(to_node, "type", "")),
        )
        if not slot:
            continue

        source = image_source_path(image)
        if source is None:
            continue
        output = texture_output_path_for_source(source)
        if output is None:
            continue

        key = (slot, source)
        if key in seen:
            continue
        seen.add(key)
        bindings.append(TextureBinding(slot, source, output))

    return sorted(bindings, key=lambda binding: (binding.slot, generic_path(binding.source)))


def material_payload(
    source: Path,
    material: object,
    bindings: list[TextureBinding],
    asset_path: Callable[[Path], str] = generic_path,
) -> bytes:
    shader = str(getattr(material, "get", lambda _key, default=None: default)("ce_shader", "pbr"))
    shader_id = MATERIAL_SHADER_IDS.get(shader.strip().lower())
    if shader_id is None:
        raise RuntimeError(f"unsupported CEngine material shader: {shader}")

    del source
    strings = bytearray()

    def append_string(text: str) -> tuple[int, int]:
        encoded = text.encode("utf-8")
        offset = len(strings)
        strings.extend(encoded)
        return offset, len(encoded)

    name_offset, name_size = append_string(material.name)
    texture_rows = bytearray()
    for binding in bindings:
        slot_id = MATERIAL_TEXTURE_SLOT_IDS.get(binding.slot)
        if slot_id is None:
            continue
        path_offset, path_size = append_string(asset_path(binding.output))
        texture_rows.extend(MATERIAL_TEXTURE.pack(slot_id, path_offset, path_size))

    string_table_offset = MATERIAL_HEADER.size + len(texture_rows)
    header = MATERIAL_HEADER.pack(
        MATERIAL_MAGIC,
        MATERIAL_VERSION,
        MATERIAL_HEADER.size,
        shader_id,
        len(texture_rows) // MATERIAL_TEXTURE.size,
        MATERIAL_HEADER.size,
        string_table_offset,
        len(strings),
        name_offset,
        name_size,
    )
    return bytes(header + texture_rows + strings)


def write_material_asset(
    blend_source: Path,
    output_root: Path,
    material: object,
    image_source_path: Callable[[object], Path | None],
    texture_output_path_for_source: Callable[[Path], Path | None],
    asset_path: Callable[[Path], str] = generic_path,
) -> MaterialExport:
    bindings = material_texture_bindings(material, image_source_path, texture_output_path_for_source)
    output = material_output_path(blend_source, output_root, material.name)
    desc = make_asset_desc(
        AssetType.MATERIAL,
        asset_path(output),
        hash_file(blend_source),
        material_payload(blend_source, material, bindings, asset_path),
    )
    write_binary_asset(output, desc)
    return MaterialExport(material, output)


def write_material_assets(
    blend_source: Path,
    output_root: Path,
    objects: Iterable[object],
    image_source_path: Callable[[object], Path | None],
    texture_output_path_for_source: Callable[[Path], Path | None],
    asset_path: Callable[[Path], str] = generic_path,
) -> list[MaterialExport]:
    return [
        write_material_asset(
            blend_source,
            output_root,
            material,
            image_source_path,
            texture_output_path_for_source,
            asset_path,
        )
        for material in object_materials(objects)
    ]
