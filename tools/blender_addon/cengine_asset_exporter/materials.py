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
from ceassetlib.texture import normalize_pillow_dds_format


MATERIAL_TEXTURE_SLOTS = {
    "base color": "base_color",
    "metallic": "metallic",
    "roughness": "roughness",
    "normal": "normal",
    "ambient occlusion": "ao",
    "occlusion": "ao",
}

MATERIAL_HEADER = struct.Struct("<4sHHIIIIIIIfffffff")
MATERIAL_TEXTURE = struct.Struct("<III")
MATERIAL_MAGIC = b"CEMA"
MATERIAL_VERSION = 3
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
    generated_textures: tuple[Path, ...] = ()


@dataclass(frozen=True)
class MaterialFactors:
    base_color: tuple[float, float, float, float]
    metallic: float
    roughness: float
    ao: float


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


def material_mra_output_path(blend_source: Path, output_root: Path, material_name: str) -> Path:
    return output_dir_for_source(blend_source, output_root) / "textures" / f"{clean_asset_name(material_name)}_mra.dds"


def principled_value(material: object, socket_name: str, fallback: float) -> float:
    nodes = getattr(getattr(material, "node_tree", None), "nodes", ())
    for node in nodes:
        if getattr(node, "type", "") != "BSDF_PRINCIPLED":
            continue
        inputs = getattr(node, "inputs", None)
        getter = getattr(inputs, "get", None)
        socket = getter(socket_name) if callable(getter) else None
        if socket is not None:
            return max(0.0, min(1.0, float(getattr(socket, "default_value", fallback))))
    return fallback


def principled_color(material: object, socket_name: str,
                     fallback: tuple[float, float, float, float]) -> tuple[float, float, float, float]:
    nodes = getattr(getattr(material, "node_tree", None), "nodes", ())
    for node in nodes:
        if getattr(node, "type", "") != "BSDF_PRINCIPLED":
            continue
        inputs = getattr(node, "inputs", None)
        getter = getattr(inputs, "get", None)
        socket = getter(socket_name) if callable(getter) else None
        value = getattr(socket, "default_value", None) if socket is not None else None
        if value is not None and len(value) >= 4:
            return tuple(max(0.0, min(1.0, float(value[index]))) for index in range(4))
    return fallback


def material_factors(material: object, bindings: list[TextureBinding]) -> MaterialFactors:
    slots = {binding.slot for binding in bindings}
    packed_mra = bool(slots & {"metallic_roughness_ao", "orm"})
    base_color = (1.0, 1.0, 1.0, 1.0) if "base_color" in slots else principled_color(
        material, "Base Color", (1.0, 1.0, 1.0, 1.0))
    metallic = 1.0 if packed_mra or "metallic" in slots else principled_value(material, "Metallic", 0.0)
    roughness = 1.0 if packed_mra or "roughness" in slots else principled_value(material, "Roughness", 0.5)
    ao = 1.0
    if not packed_mra and "ao" not in slots:
        ao = max(0.0, min(1.0, float(getattr(material, "get", lambda _key, default=None: default)("ce_ao", 1.0))))
    return MaterialFactors(base_color, metallic, roughness, ao)


def write_mra_texture(bindings: list[TextureBinding], output: Path) -> None:
    by_slot = {binding.slot: binding for binding in bindings}
    metallic = by_slot.get("metallic")
    roughness = by_slot.get("roughness")
    ao = by_slot.get("ao")
    sources = [binding.output for binding in (metallic, roughness, ao) if binding is not None]
    if not sources:
        raise ValueError("packed MRA generation requires at least one authored texture input")

    try:
        from PIL import Image
    except ImportError as error:
        raise RuntimeError("Pillow is required to pack material MRA textures") from error

    opened = []
    channels = []
    try:
        for source in sources:
            image = Image.open(source)
            opened.append(image)
        size = max((image.size for image in opened), key=lambda value: value[0] * value[1])

        def channel(binding: TextureBinding | None, value: int):
            if binding is None:
                result = Image.new("L", size, value)
                channels.append(result)
                return result
            image = next(image for image, source in zip(opened, sources) if source == binding.output)
            result = image.convert("L")
            if result.size != size:
                resized = result.resize(size, Image.Resampling.BILINEAR)
                result.close()
                result = resized
            channels.append(result)
            return result

        alpha = Image.new("L", size, 255)
        channels.append(alpha)
        packed = Image.merge("RGBA", (
            channel(metallic, 255),
            channel(roughness, 255),
            channel(ao, 255),
            alpha,
        ))
        output.parent.mkdir(parents=True, exist_ok=True)
        temporary = output.with_name(output.name + ".tmp")
        try:
            packed.save(temporary, format="DDS", pixel_format=normalize_pillow_dds_format("DXT5"))
            temporary.replace(output)
        finally:
            packed.close()
            if temporary.exists():
                temporary.unlink()
    finally:
        for image in channels:
            image.close()
        for image in opened:
            image.close()


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
    if node_type == "NORMAL_MAP" and socket_name.strip().lower() == "color":
        return "normal"
    if node_type != "BSDF_PRINCIPLED":
        return ""
    return MATERIAL_TEXTURE_SLOTS.get(socket_name.strip().lower(), "")


def material_link_slot(link: object, links: list[object]) -> str:
    from_node = getattr(link, "from_node", None)
    if getattr(from_node, "type", "") != "TEX_IMAGE":
        return ""
    to_node = getattr(link, "to_node", None)
    to_node_type = str(getattr(to_node, "type", ""))
    socket_name = str(getattr(getattr(link, "to_socket", None), "name", ""))
    slot = texture_slot_name(socket_name, to_node_type)
    if slot != "normal" or to_node_type != "NORMAL_MAP":
        return slot
    return slot if any(
        getattr(candidate, "from_node", None) is to_node and
        getattr(getattr(candidate, "to_node", None), "type", "") == "BSDF_PRINCIPLED" and
        str(getattr(getattr(candidate, "to_socket", None), "name", "")).strip().lower() == "normal"
        for candidate in links
    ) else ""


def material_texture_bindings(
    material: object,
    image_source_path: Callable[[object], Path | None],
    texture_output_path_for_source: Callable[[Path], Path | None],
) -> list[TextureBinding]:
    node_tree = getattr(material, "node_tree", None)
    links = list(getattr(node_tree, "links", ())) if node_tree is not None else []
    bindings: list[TextureBinding] = []
    seen: set[tuple[str, Path]] = set()

    for link in links:
        from_node = getattr(link, "from_node", None)
        if getattr(from_node, "type", "") != "TEX_IMAGE":
            continue
        image = getattr(from_node, "image", None)
        if image is None:
            continue

        slot = material_link_slot(link, links)
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


def supported_material_images(material: object) -> list[object]:
    node_tree = getattr(material, "node_tree", None)
    links = list(getattr(node_tree, "links", ())) if node_tree is not None else []
    images: list[object] = []
    seen: set[int] = set()
    for link in links:
        from_node = getattr(link, "from_node", None)
        if getattr(from_node, "type", "") != "TEX_IMAGE":
            continue
        slot = material_link_slot(link, links)
        if not slot:
            continue
        image = getattr(from_node, "image", None)
        if image is None or id(image) in seen:
            continue
        seen.add(id(image))
        images.append(image)
    return images


def material_payload(
    source: Path,
    material: object,
    bindings: list[TextureBinding],
    asset_path: Callable[[Path], str] = generic_path,
    factors: MaterialFactors | None = None,
) -> bytes:
    shader = str(getattr(material, "get", lambda _key, default=None: default)("ce_shader", "pbr"))
    shader_id = MATERIAL_SHADER_IDS.get(shader.strip().lower())
    if shader_id is None:
        raise RuntimeError(f"unsupported CEngine material shader: {shader}")

    del source
    factors = factors if factors is not None else material_factors(material, bindings)
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
        *factors.base_color,
        factors.metallic,
        factors.roughness,
        factors.ao,
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
    dds_format: str = "DXT5",
) -> MaterialExport:
    start = time.perf_counter()
    bindings = material_texture_bindings(material, image_source_path, texture_output_path_for_source)
    factors = material_factors(material, bindings)
    packed_mra = next((binding for binding in bindings if binding.slot in {"metallic_roughness_ao", "orm"}), None)
    separate_mra = [binding for binding in bindings if binding.slot in {"metallic", "roughness", "ao"}]
    generated_textures: tuple[Path, ...] = ()
    if packed_mra is None and separate_mra:
        mra_output = material_mra_output_path(blend_source, output_root, material.name)
        write_mra_texture(bindings, mra_output)
        packed_mra = TextureBinding("metallic_roughness_ao", mra_output, mra_output)
        generated_textures = (mra_output,)
    elif packed_mra is None:
        stale_mra = material_mra_output_path(blend_source, output_root, material.name)
        if stale_mra.is_file():
            stale_mra.unlink()
    payload_bindings = [binding for binding in bindings
                        if binding.slot not in {"metallic", "roughness", "ao"}]
    if packed_mra is not None:
        payload_bindings.append(packed_mra)
    output = material_output_path(blend_source, output_root, material.name)
    if logger is not None:
        logger(f"Material {material.name}: {len(bindings)} texture binding(s) -> {output}")
        if packed_mra is not None:
            logger(f"Material {material.name}: packed MRA -> {packed_mra.output}")
        else:
            logger(
                f"Material {material.name}: constants base={factors.base_color}, "
                f"metallic={factors.metallic:.3f}, roughness={factors.roughness:.3f}, ao={factors.ao:.3f}"
            )
    desc = make_asset_desc(
        AssetType.MATERIAL,
        asset_path(output),
        source_hash if source_hash is not None else hash_file(blend_source),
        material_payload(blend_source, material, payload_bindings, asset_path, factors),
    )
    write_binary_asset(output, desc)
    if logger is not None:
        logger(f"Material {material.name}: wrote {output.name} in {elapsed(start)}")
    return MaterialExport(material, output, generated_textures)


def write_material_assets(
    blend_source: Path,
    output_root: Path,
    objects: Iterable[object],
    image_source_path: Callable[[object], Path | None],
    texture_output_path_for_source: Callable[[Path], Path | None],
    asset_path: Callable[[Path], str] = generic_path,
    logger: Callable[[str], None] | None = None,
    source_hash: int | None = None,
    dds_format: str = "DXT5",
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
            dds_format,
        ))
    return outputs
