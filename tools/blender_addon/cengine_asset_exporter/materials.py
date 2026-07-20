from __future__ import annotations

import struct
import time
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


@dataclass(frozen=True)
class DefaultMaterial:
    name: str

    def get(self, _key: str, default: object = None) -> object:
        return default


def elapsed(start: float) -> str:
    return f"{time.perf_counter() - start:.2f}s"


def default_material_name_for_object(obj: object) -> str:
    return f"{clean_asset_name(str(getattr(obj, 'name', 'Mesh') or 'Mesh'))}_DefaultMaterial"


def object_slot_materials(obj: object) -> list[object]:
    materials: list[object] = []
    for slot in getattr(obj, "material_slots", ()):
        material = getattr(slot, "material", None)
        if material is not None:
            materials.append(material)
    return materials


def effective_material_names(obj: object) -> list[str]:
    materials = object_slot_materials(obj)
    if materials:
        return [material.name for material in materials]
    if getattr(obj, "type", "") == "MESH":
        return [default_material_name_for_object(obj)]
    return []


def material_output_path(blend_source: Path, output_root: Path, material_name: str) -> Path:
    return output_dir_for_source(blend_source, output_root) / "materials" / f"{clean_asset_name(material_name)}.cmat"


def object_materials(objects: Iterable[object]) -> list[object]:
    materials: list[object] = []
    seen: set[int] = set()
    seen_default_names: set[str] = set()
    for obj in objects:
        slot_materials = object_slot_materials(obj)
        if not slot_materials and getattr(obj, "type", "") == "MESH":
            default_name = default_material_name_for_object(obj)
            if default_name not in seen_default_names:
                seen_default_names.add(default_name)
                materials.append(DefaultMaterial(default_name))
            continue

        for material in slot_materials:
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
    fallback_texture: Path | None = None,
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
    payload_bindings = bindings
    if not payload_bindings and fallback_texture is not None:
        payload_bindings = [TextureBinding("base_color", fallback_texture, fallback_texture)]
    for binding in payload_bindings:
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
    logger: Callable[[str], None] | None = None,
    source_hash: int | None = None,
    fallback_texture: Path | None = None,
) -> MaterialExport:
    start = time.perf_counter()
    bindings = material_texture_bindings(material, image_source_path, texture_output_path_for_source)
    output = material_output_path(blend_source, output_root, material.name)
    if logger is not None:
        logger(f"Material {material.name}: {len(bindings)} texture binding(s) -> {output}")
        if not bindings and fallback_texture is not None:
            logger(f"Material {material.name}: using fallback texture {fallback_texture}")
    desc = make_asset_desc(
        AssetType.MATERIAL,
        asset_path(output),
        source_hash if source_hash is not None else hash_file(blend_source),
        material_payload(blend_source, material, bindings, asset_path, fallback_texture),
    )
    write_binary_asset(output, desc)
    if logger is not None:
        logger(f"Material {material.name}: wrote {output.name} in {elapsed(start)}")
    return MaterialExport(material, output)


def write_material_assets(
    blend_source: Path,
    output_root: Path,
    objects: Iterable[object],
    image_source_path: Callable[[object], Path | None],
    texture_output_path_for_source: Callable[[Path], Path | None],
    asset_path: Callable[[Path], str] = generic_path,
    logger: Callable[[str], None] | None = None,
    source_hash: int | None = None,
    fallback_texture: Path | None = None,
) -> list[MaterialExport]:
    materials = object_materials(objects)
    if logger is not None:
        logger(f"Material export queue: {len(materials)} unique material(s)")
        for obj in objects:
            slot_materials = object_slot_materials(obj)
            if getattr(obj, "type", "") != "MESH":
                continue
            if not slot_materials:
                logger(f"Material fallback: {obj.name} has no material slots; generating {default_material_name_for_object(obj)}")
            elif len(slot_materials) > 1:
                logger(
                    f"Material fallback: {obj.name} has {len(slot_materials)} material slots; "
                    f"using first slot {slot_materials[0].name}"
                )
    outputs: list[MaterialExport] = []
    for index, material in enumerate(materials, start=1):
        if logger is not None:
            logger(f"Material {index}/{len(materials)}: {material.name}")
        outputs.append(write_material_asset(
            blend_source,
            output_root,
            material,
            image_source_path,
            texture_output_path_for_source,
            asset_path,
            logger,
            source_hash,
            fallback_texture,
        ))
    return outputs
