from __future__ import annotations

import math
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Iterable

from ceassetlib.blender_scene import LightmapPlacement
from ceassetlib.collection_export import clean_asset_name
from ceassetlib.paths import output_dir_for_source
from ceassetlib.texture import write_rgba_dxt5


LIGHTMAP_UV = "CEngineLightmap"
BAKE_UV = "CEngineBake"
DEFAULT_RESOLUTION = 1024
DEFAULT_PADDING = 4
DEFAULT_RGBM_RANGE = 8.0


@dataclass(frozen=True)
class AtlasTile:
    scale: tuple[float, float]
    offset: tuple[float, float]


def plan_atlas(names: Iterable[str], resolution: int, padding: int) -> dict[str, AtlasTile]:
    ordered = sorted(set(names))
    if not ordered:
        return {}
    if resolution <= 0 or padding < 0:
        raise ValueError("lightmap resolution and padding must be positive")

    columns = math.ceil(math.sqrt(len(ordered)))
    rows = math.ceil(len(ordered) / columns)
    tile_width = resolution // columns
    tile_height = resolution // rows
    if tile_width <= padding * 2 or tile_height <= padding * 2:
        raise ValueError("lightmap atlas is too small for its placements and padding")

    scale = ((tile_width - padding * 2) / resolution,
             (tile_height - padding * 2) / resolution)
    result: dict[str, AtlasTile] = {}
    for index, name in enumerate(ordered):
        column = index % columns
        row = index // columns
        result[name] = AtlasTile(
            scale,
            ((column * tile_width + padding) / resolution,
             (row * tile_height + padding) / resolution),
        )
    return result


def encode_rgbm(pixels: Iterable[float], rgbm_range: float) -> bytes:
    values = list(pixels)
    if len(values) % 4 != 0:
        raise ValueError("Blender lightmap pixels must be RGBA")
    if rgbm_range <= 0.0:
        raise ValueError("RGBM range must be positive")

    encoded = bytearray(len(values))
    for index in range(0, len(values), 4):
        red = max(0.0, values[index])
        green = max(0.0, values[index + 1])
        blue = max(0.0, values[index + 2])
        multiplier = min(1.0, max(red, green, blue) / rgbm_range)
        multiplier = math.ceil(multiplier * 255.0) / 255.0
        denominator = multiplier * rgbm_range
        if denominator > 0.0:
            red, green, blue = red / denominator, green / denominator, blue / denominator
        else:
            red = green = blue = 0.0
        encoded[index] = round(min(red, 1.0) * 255.0)
        encoded[index + 1] = round(min(green, 1.0) * 255.0)
        encoded[index + 2] = round(min(blue, 1.0) * 255.0)
        encoded[index + 3] = round(multiplier * 255.0)
    return bytes(encoded)


def _static_meshes(objects: Iterable[object]) -> list[object]:
    return sorted((obj for obj in objects
                   if getattr(obj, "type", "") == "MESH"
                   and not bool(getattr(obj, "get", lambda *_: False)("ce_dynamic", False))),
                  key=lambda obj: obj.name)


def _select_only(blender: object, obj: object) -> None:
    for selected in tuple(getattr(blender.context, "selected_objects", ())):
        selected.select_set(False)
    obj.select_set(True)
    blender.context.view_layer.objects.active = obj


def ensure_lightmap_uvs(blender: object, objects: Iterable[object]) -> None:
    previous_active = getattr(blender.context.view_layer.objects, "active", None)
    previous_selected = tuple(getattr(blender.context, "selected_objects", ()))
    previous_mode = str(getattr(blender.context, "mode", "OBJECT"))
    try:
        if previous_mode != "OBJECT":
            blender.ops.object.mode_set(mode="OBJECT")
        for obj in _static_meshes(objects):
            layers = obj.data.uv_layers
            layer = layers.get(LIGHTMAP_UV)
            if layer is not None:
                continue
            layer = layers.new(name=LIGHTMAP_UV)
            layers.active = layer
            _select_only(blender, obj)
            blender.ops.object.mode_set(mode="EDIT")
            blender.ops.mesh.select_all(action="SELECT")
            blender.ops.uv.lightmap_pack(PREF_CONTEXT="ALL_FACES", PREF_MARGIN_DIV=0.1)
            blender.ops.object.mode_set(mode="OBJECT")
    finally:
        if str(getattr(blender.context, "mode", "OBJECT")) != "OBJECT":
            blender.ops.object.mode_set(mode="OBJECT")
        for selected in tuple(getattr(blender.context, "selected_objects", ())):
            selected.select_set(False)
        for selected in previous_selected:
            selected.select_set(True)
        blender.context.view_layer.objects.active = previous_active


def _copy_bake_uv(mesh: object, tile: AtlasTile) -> None:
    source = mesh.uv_layers.get(LIGHTMAP_UV)
    if source is None:
        raise RuntimeError(f"mesh has no {LIGHTMAP_UV} UV layer")
    old = mesh.uv_layers.get(BAKE_UV)
    if old is not None:
        mesh.uv_layers.remove(old)
    target = mesh.uv_layers.new(name=BAKE_UV)
    for source_uv, target_uv in zip(source.data, target.data):
        target_uv.uv = (
            float(source_uv.uv[0]) * tile.scale[0] + tile.offset[0],
            float(source_uv.uv[1]) * tile.scale[1] + tile.offset[1],
        )
    mesh.uv_layers.active = target
    target.active_render = True


def _save_rgbm(pixels: Iterable[float], resolution: int, rgbm_range: float,
               dds_path: Path) -> None:
    write_rgba_dxt5(dds_path, resolution, resolution, encode_rgbm(pixels, rgbm_range))


def bake_scene_lightmaps(
    blender: object,
    source: Path,
    output_root: Path,
    objects: Iterable[object],
    scene_name: str,
    logger: Callable[[str], None] | None = None,
) -> tuple[dict[str, LightmapPlacement], list[Path]]:
    has_baked_lights = any(
        getattr(obj, "type", "") == "LIGHT" and
        str(getattr(obj, "get", lambda *_: "realtime")(
            "ce_light_mode", "realtime")).lower() in {"baked", "mixed"}
        for obj in objects)
    if not has_baked_lights:
        return {}, []
    static_meshes = _static_meshes(objects)
    if not static_meshes:
        return {}, []

    scene = blender.context.scene
    resolution = int(getattr(scene, "get", lambda *_: DEFAULT_RESOLUTION)(
        "ce_lightmap_resolution", DEFAULT_RESOLUTION))
    padding = int(getattr(scene, "get", lambda *_: DEFAULT_PADDING)(
        "ce_lightmap_padding", DEFAULT_PADDING))
    rgbm_range = float(getattr(scene, "get", lambda *_: DEFAULT_RGBM_RANGE)(
        "ce_lightmap_rgbm_range", DEFAULT_RGBM_RANGE))
    tiles = plan_atlas((obj.name for obj in static_meshes), resolution, padding)
    ensure_lightmap_uvs(blender, static_meshes)

    output_dir = output_dir_for_source(source, output_root) / "lightmaps"
    output = output_dir / f"{clean_asset_name(scene_name)}_0.dds"
    image = blender.data.images.new(
        f"CEngine_{scene_name}_Lightmap", width=resolution, height=resolution,
        alpha=True, float_buffer=True)
    previous_engine = scene.render.engine
    previous_bake_clear = scene.render.bake.use_clear
    previous_active = getattr(blender.context.view_layer.objects, "active", None)
    previous_selected = tuple(getattr(blender.context, "selected_objects", ()))
    original_meshes: dict[object, object] = {}
    temporary_nodes: list[tuple[object, object]] = []
    temporary_materials: list[object] = []
    replaced_slots: list[object] = []
    material_states: list[tuple[object, bool, object | None, tuple[object, ...]]] = []
    prepared_materials: set[int] = set()
    hidden_lights: list[tuple[object, bool]] = []
    try:
        scene.render.engine = "CYCLES"
        scene.render.bake.use_clear = True
        for obj in objects:
            if getattr(obj, "type", "") != "LIGHT":
                continue
            mode = str(getattr(obj, "get", lambda *_: "realtime")(
                "ce_light_mode", "realtime")).lower()
            hidden_lights.append((obj, bool(obj.hide_render)))
            obj.hide_render = mode != "baked"

        for obj in static_meshes:
            original_meshes[obj] = obj.data
            obj.data = obj.data.copy()
            _copy_bake_uv(obj.data, tiles[obj.name])
            if not obj.material_slots:
                material = blender.data.materials.new(f"CEngineBake_{obj.name}")
                material.use_nodes = True
                obj.data.materials.append(material)
                temporary_materials.append(material)
            for slot in obj.material_slots:
                material = slot.material
                if material is None:
                    material = blender.data.materials.new(f"CEngineBake_{obj.name}")
                    material.use_nodes = True
                    slot.material = material
                    temporary_materials.append(material)
                    replaced_slots.append(slot)
                key = int(material.as_pointer())
                if key in prepared_materials:
                    continue
                prepared_materials.add(key)
                previous_active_node = material.node_tree.nodes.active if material.use_nodes else None
                previous_selection = tuple(
                    node for node in material.node_tree.nodes if node.select) if material.use_nodes else ()
                material_states.append(
                    (material, bool(material.use_nodes), previous_active_node, previous_selection))
                material.use_nodes = True
                for existing in material.node_tree.nodes:
                    existing.select = False
                node = material.node_tree.nodes.new("ShaderNodeTexImage")
                node.image = image
                material.node_tree.nodes.active = node
                node.select = True
                temporary_nodes.append((material, node))

        for index, obj in enumerate(static_meshes):
            _select_only(blender, obj)
            scene.render.bake.use_clear = index == 0
            blender.ops.object.bake(type="COMBINED", margin=padding, use_selected_to_active=False)

        _save_rgbm(image.pixels[:], resolution, rgbm_range, output)
    finally:
        for material, node in reversed(temporary_nodes):
            material.node_tree.nodes.remove(node)
        for material, used_nodes, active_node, selected_nodes in reversed(material_states):
            if used_nodes:
                for node in material.node_tree.nodes:
                    node.select = node in selected_nodes
                material.node_tree.nodes.active = active_node
            material.use_nodes = used_nodes
        for slot in replaced_slots:
            slot.material = None
        for obj, mesh in original_meshes.items():
            temporary = obj.data
            obj.data = mesh
            blender.data.meshes.remove(temporary)
        for material in temporary_materials:
            blender.data.materials.remove(material)
        for light, hidden in hidden_lights:
            light.hide_render = hidden
        blender.data.images.remove(image)
        scene.render.engine = previous_engine
        scene.render.bake.use_clear = previous_bake_clear
        for selected in tuple(getattr(blender.context, "selected_objects", ())):
            selected.select_set(False)
        for selected in previous_selected:
            selected.select_set(True)
        blender.context.view_layer.objects.active = previous_active

    if logger is not None:
        logger(f"Lightmap bake: wrote {output.name} ({resolution}x{resolution}, {len(static_meshes)} placement(s))")
    placements = {
        name: LightmapPlacement(output, tile.scale, tile.offset, rgbm_range)
        for name, tile in tiles.items()
    }
    return placements, [output]
