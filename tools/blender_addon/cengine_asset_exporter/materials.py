from __future__ import annotations

import struct
import time
import re
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
    "alpha": "alpha",
    "metallic": "metallic",
    "roughness": "roughness",
    "normal": "normal",
    "ambient occlusion": "ao",
    "occlusion": "ao",
}

MATERIAL_HEADER = struct.Struct("<4sHHIIIIIIIIffffffff")
MATERIAL_TEXTURE = struct.Struct("<III")
MATERIAL_MAGIC = b"CEMA"
MATERIAL_VERSION = 4
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
MATERIAL_RENDER_MODE_IDS = {
    "opaque": 0,
    "alpha_clip": 1,
    "alpha_hash": 2,
    "transparent": 3,
    "unlit": 4,
}


@dataclass(frozen=True)
class TextureBinding:
    slot: str
    source: Path
    output: Path
    bump_strength: float = 1.0
    bump_distance: float = 1.0
    bump_invert: bool = False


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
class MaterialSurface:
    render_mode: int
    alpha_cutoff: float


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


def material_bump_normal_output_path(blend_source: Path, output_root: Path, material_name: str) -> Path:
    return output_dir_for_source(blend_source, output_root) / "textures" / \
        f"{clean_asset_name(material_name)}_bump_normal.dds"


def material_base_alpha_output_path(blend_source: Path, output_root: Path, material_name: str) -> Path:
    return output_dir_for_source(blend_source, output_root) / "textures" / \
        f"{clean_asset_name(material_name)}_base_alpha.dds"


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
    authored_base_color = principled_color(material, "Base Color", (1.0, 1.0, 1.0, 1.0))
    alpha = principled_value(material, "Alpha", 1.0)
    base_color = (1.0, 1.0, 1.0, alpha) if "base_color" in slots else (
        authored_base_color[0], authored_base_color[1], authored_base_color[2], alpha)
    metallic = 1.0 if packed_mra or "metallic" in slots else principled_value(material, "Metallic", 0.0)
    roughness = 1.0 if packed_mra or "roughness" in slots else principled_value(material, "Roughness", 0.5)
    ao = 1.0
    if not packed_mra and "ao" not in slots:
        ao = max(0.0, min(1.0, float(getattr(material, "get", lambda _key, default=None: default)("ce_ao", 1.0))))
    return MaterialFactors(base_color, metallic, roughness, ao)


def material_has_authored_alpha(material: object) -> bool:
    nodes = getattr(getattr(material, "node_tree", None), "nodes", ())
    principled_nodes = [node for node in nodes if getattr(node, "type", "") == "BSDF_PRINCIPLED"]
    links = getattr(getattr(material, "node_tree", None), "links", ())
    if any(
        any(same_node(getattr(link, "to_node", None), node) and
            str(getattr(getattr(link, "to_socket", None), "name", "")).strip().lower() == "alpha"
            for node in principled_nodes)
        for link in links
    ):
        return True
    return principled_value(material, "Alpha", 1.0) < 1.0


def material_surface(material: object, shader_id: int) -> MaterialSurface:
    if shader_id == MATERIAL_SHADER_IDS["unlit"]:
        return MaterialSurface(MATERIAL_RENDER_MODE_IDS["unlit"], 0.5)
    if not material_has_authored_alpha(material):
        return MaterialSurface(MATERIAL_RENDER_MODE_IDS["opaque"], 0.5)

    surface_method = str(getattr(material, "surface_render_method", "") or "").upper()
    blend_method = str(getattr(material, "blend_method", "") or "").upper()
    if blend_method == "CLIP":
        mode = MATERIAL_RENDER_MODE_IDS["alpha_clip"]
    elif blend_method == "BLEND" or surface_method == "BLENDED":
        mode = MATERIAL_RENDER_MODE_IDS["transparent"]
    else:
        # Blender 4.2+ uses DITHERED; older files expose HASHED. Both map to
        # the engine's stable alpha-hash path.
        mode = MATERIAL_RENDER_MODE_IDS["alpha_hash"]
    cutoff = max(0.0, min(1.0, float(getattr(material, "alpha_threshold", 0.5))))
    return MaterialSurface(mode, cutoff)


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


def write_bump_normal_texture(binding: TextureBinding, output: Path, dds_format: str) -> None:
    try:
        from PIL import Image, ImageFilter
    except ImportError as error:
        raise RuntimeError("Pillow is required to convert bump maps to normal maps") from error

    slope = max(0.0, binding.bump_strength) * max(0.0, binding.bump_distance)
    sign = -1.0 if binding.bump_invert else 1.0
    # Sobel's kernel already sums eight weighted samples. Dividing by eight
    # makes ordinary height gradients so small that BC3 quantizes whole maps to
    # a flat normal. Keep the authored strength as the slope scale instead.
    divisor = 1.0 / max(slope, 1.0e-6)
    horizontal = tuple(sign * value for value in (1, 0, -1, 2, 0, -2, 1, 0, -1))
    vertical = tuple(sign * value for value in (-1, -2, -1, 0, 0, 0, 1, 2, 1))
    with Image.open(binding.source) as source:
        height = source.convert("L")
        try:
            if slope == 0.0:
                red = Image.new("L", height.size, 128)
                green = Image.new("L", height.size, 128)
            else:
                red = height.filter(ImageFilter.Kernel((3, 3), horizontal, scale=divisor, offset=128))
                green = height.filter(ImageFilter.Kernel((3, 3), vertical, scale=divisor, offset=128))
            blue = Image.new("L", height.size, 255)
            alpha = Image.new("L", height.size, 255)
            try:
                normal = Image.merge("RGBA", (red, green, blue, alpha))
                output.parent.mkdir(parents=True, exist_ok=True)
                temporary = output.with_name(output.name + ".tmp")
                try:
                    normal.save(temporary, format="DDS",
                                pixel_format=normalize_pillow_dds_format(dds_format))
                    temporary.replace(output)
                finally:
                    normal.close()
                    if temporary.exists():
                        temporary.unlink()
            finally:
                red.close()
                green.close()
                blue.close()
                alpha.close()
        finally:
            height.close()


def write_base_alpha_texture(
    base_color: TextureBinding | None,
    opacity: TextureBinding,
    output: Path,
    dds_format: str,
) -> None:
    try:
        from PIL import Image
    except ImportError as error:
        raise RuntimeError("Pillow is required to pack material opacity into base color") from error

    with Image.open(opacity.source) as opacity_source:
        opacity_rgba = opacity_source.convert("RGBA")
        opacity_alpha = opacity_rgba.getchannel("A")
        alpha_extrema = opacity_alpha.getextrema()
        if alpha_extrema == (255, 255):
            opacity_alpha.close()
            opacity_alpha = opacity_source.convert("L")
        try:
            if base_color is None:
                packed = Image.new("RGBA", opacity_alpha.size, (255, 255, 255, 255))
            else:
                with Image.open(base_color.source) as base_source:
                    packed = base_source.convert("RGBA")
            try:
                if opacity_alpha.size != packed.size:
                    resized = opacity_alpha.resize(packed.size, Image.Resampling.BILINEAR)
                    opacity_alpha.close()
                    opacity_alpha = resized
                packed.putalpha(opacity_alpha)
                output.parent.mkdir(parents=True, exist_ok=True)
                temporary = output.with_name(output.name + ".tmp")
                try:
                    packed.save(temporary, format="DDS", pixel_format=normalize_pillow_dds_format(dds_format))
                    temporary.replace(output)
                finally:
                    if temporary.exists():
                        temporary.unlink()
            finally:
                packed.close()
        finally:
            opacity_alpha.close()
            opacity_rgba.close()


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


def same_node(left: object, right: object) -> bool:
    return left is right or (left is not None and right is not None and left == right)


def image_pixels_are_grayscale(image: object) -> bool | None:
    pixels = getattr(image, "pixels", None)
    if pixels is None:
        return None
    try:
        pixel_count = len(pixels) // 4
        if pixel_count == 0:
            return None
        step = max(1, pixel_count // 64)
        for pixel in range(0, pixel_count, step):
            offset = pixel * 4
            red, green, blue = float(pixels[offset]), float(pixels[offset + 1]), float(pixels[offset + 2])
            if max(abs(red - green), abs(red - blue), abs(green - blue)) > 1.0e-4:
                return False
        return True
    except (IndexError, RuntimeError, TypeError):
        return None


def source_is_grayscale(source: Path) -> bool:
    try:
        from PIL import Image, ImageChops
    except ImportError:
        return False
    try:
        with Image.open(source) as image:
            if image.mode in {"1", "L", "LA", "I", "I;16", "F"}:
                return True
            rgb = image.convert("RGB")
            red, green, blue = rgb.split()
            try:
                return ImageChops.difference(red, green).getbbox() is None and \
                    ImageChops.difference(red, blue).getbbox() is None
            finally:
                red.close()
                green.close()
                blue.close()
                rgb.close()
    except (OSError, ValueError):
        return False


def image_is_bump_source(image: object, source: Path | None = None) -> bool:
    names = [
        str(getattr(image, "name", "") or ""),
        str(getattr(image, "filepath", "") or ""),
        str(source or ""),
    ]
    for name in names:
        tokens = set(re.split(r"[^a-z0-9]+", Path(name).stem.lower()))
        if tokens & {"bump", "height", "disp", "displacement"}:
            return True
    sampled = image_pixels_are_grayscale(image)
    if sampled is not None:
        return sampled
    return source_is_grayscale(source) if source is not None else False


def material_link_slot(link: object, links: list[object]) -> str:
    from_node = getattr(link, "from_node", None)
    if getattr(from_node, "type", "") != "TEX_IMAGE":
        return ""
    to_node = getattr(link, "to_node", None)
    to_node_type = str(getattr(to_node, "type", ""))
    socket_name = str(getattr(getattr(link, "to_socket", None), "name", ""))
    if to_node_type == "BUMP" and socket_name.strip().lower() == "height":
        return "bump" if any(
            same_node(getattr(candidate, "from_node", None), to_node) and
            getattr(getattr(candidate, "to_node", None), "type", "") == "BSDF_PRINCIPLED" and
            str(getattr(getattr(candidate, "to_socket", None), "name", "")).strip().lower() == "normal"
            for candidate in links
        ) else ""
    slot = texture_slot_name(socket_name, to_node_type)
    if slot != "normal" or to_node_type != "NORMAL_MAP":
        return slot
    return slot if any(
        same_node(getattr(candidate, "from_node", None), to_node) and
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
        if slot == "normal" and str(getattr(getattr(link, "to_node", None), "type", "")) == "NORMAL_MAP" and \
                image_is_bump_source(image, source):
            slot = "bump"
        output = source if slot in {"bump", "alpha"} else texture_output_path_for_source(source)
        if output is None: continue

        key = (slot, source)
        if key in seen:
            continue
        seen.add(key)
        if slot == "bump":
            target_node = getattr(link, "to_node", None)
            target_type = str(getattr(target_node, "type", ""))
            inputs = getattr(target_node, "inputs", None)
            getter = getattr(inputs, "get", None)
            strength_socket = getter("Strength") if callable(getter) else None
            distance_socket = getter("Distance") if callable(getter) else None
            # A grayscale image connected to Normal Map is height data despite
            # the node type. Its Strength controls decoded tangent normals and
            # is not a Blender Bump height scale (some imported assets set it
            # to zero), so use a unit height conversion for this recovery path.
            normal_map_height = target_type == "NORMAL_MAP"
            bindings.append(TextureBinding(
                slot,
                source,
                output,
                1.0 if normal_map_height else
                    max(0.0, min(1.0, float(getattr(strength_socket, "default_value", 1.0)))),
                1.0 if normal_map_height else
                    max(0.0, float(getattr(distance_socket, "default_value", 1.0))),
                False if normal_map_height else bool(getattr(target_node, "invert", False)),
            ))
        else:
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
        if not slot or slot in {"bump", "alpha"}:
            continue
        image = getattr(from_node, "image", None)
        if image is None or id(image) in seen:
            continue
        if slot == "normal" and str(getattr(getattr(link, "to_node", None), "type", "")) == "NORMAL_MAP" and \
                image_is_bump_source(image):
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
    surface = material_surface(material, shader_id)
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
        surface.render_mode,
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
        surface.alpha_cutoff,
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
    base_color = next((binding for binding in bindings if binding.slot == "base_color"), None)
    opacity = next((binding for binding in bindings if binding.slot == "alpha"), None)
    packed_mra = next((binding for binding in bindings if binding.slot in {"metallic_roughness_ao", "orm"}), None)
    separate_mra = [binding for binding in bindings if binding.slot in {"metallic", "roughness", "ao"}]
    generated_textures: list[Path] = []
    if opacity is not None:
        base_alpha_output = material_base_alpha_output_path(blend_source, output_root, material.name)
        write_base_alpha_texture(base_color, opacity, base_alpha_output, dds_format)
        base_color = TextureBinding("base_color", base_color.source if base_color is not None else opacity.source,
                                    base_alpha_output)
        generated_textures.append(base_alpha_output)
    else:
        stale_base_alpha = material_base_alpha_output_path(blend_source, output_root, material.name)
        if stale_base_alpha.is_file():
            stale_base_alpha.unlink()
    direct_normal = next((binding for binding in bindings if binding.slot == "normal"), None)
    bump = next((binding for binding in bindings if binding.slot == "bump"), None)
    if direct_normal is None and bump is not None:
        bump_normal_output = material_bump_normal_output_path(
            blend_source, output_root, material.name)
        write_bump_normal_texture(bump, bump_normal_output, dds_format)
        direct_normal = TextureBinding("normal", bump.source, bump_normal_output)
        generated_textures.append(bump_normal_output)
    else:
        stale_bump_normal = material_bump_normal_output_path(blend_source, output_root, material.name)
        if stale_bump_normal.is_file():
            stale_bump_normal.unlink()
    if packed_mra is None and separate_mra:
        mra_output = material_mra_output_path(blend_source, output_root, material.name)
        write_mra_texture(bindings, mra_output)
        packed_mra = TextureBinding("metallic_roughness_ao", mra_output, mra_output)
        generated_textures.append(mra_output)
    elif packed_mra is None:
        stale_mra = material_mra_output_path(blend_source, output_root, material.name)
        if stale_mra.is_file():
            stale_mra.unlink()
    payload_bindings = [binding for binding in bindings
                        if binding.slot not in {"base_color", "alpha", "metallic", "roughness", "ao", "bump", "normal"}]
    if base_color is not None:
        payload_bindings.append(base_color)
    if direct_normal is not None:
        payload_bindings.append(direct_normal)
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
    return MaterialExport(material, output, tuple(generated_textures))


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
