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
MINIMUM_LIGHTMAP_ENERGY = 1.0e-5


@dataclass(frozen=True)
class AtlasTile:
    scale: tuple[float, float]
    offset: tuple[float, float]


@dataclass(frozen=True)
class BakeTarget:
    key: str
    source: object
    world_matrix: object
    prop_name: str | None = None
    prefab_instance: str | None = None
    prefab_object_index: int | None = None


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


def _bake_targets(objects: Iterable[object],
                  prefab_objects: dict[str, list[object]]) -> list[BakeTarget]:
    targets: list[BakeTarget] = []
    for obj in sorted(objects, key=lambda value: str(getattr(value, "name", ""))):
        if (getattr(obj, "type", "") == "MESH" and
                not bool(getattr(obj, "get", lambda *_: False)("ce_dynamic", False))):
            targets.append(BakeTarget(f"prop:{obj.name}", obj, obj.matrix_world, prop_name=obj.name))
            continue
        sources = prefab_objects.get(str(getattr(obj, "name", "")))
        if sources is None:
            continue
        for object_index, source in enumerate(sorted(sources, key=lambda value: value.name)):
            if (getattr(source, "type", "") != "MESH" or
                    bool(getattr(source, "get", lambda *_: False)("ce_dynamic", False))):
                continue
            targets.append(BakeTarget(
                f"prefab:{obj.name}:{object_index}:{source.name}", source,
                obj.matrix_world @ source.matrix_world,
                prefab_instance=obj.name, prefab_object_index=object_index))
    return targets


def _combine_light_passes(indirect: Iterable[float], direct: Iterable[float]) -> list[float]:
    indirect_values = list(indirect)
    direct_values = list(direct)
    if len(indirect_values) != len(direct_values) or len(indirect_values) % 4 != 0:
        raise RuntimeError("Blender lightmap pass sizes do not match")
    result = [0.0] * len(indirect_values)
    for index in range(0, len(result), 4):
        result[index] = max(0.0, indirect_values[index] + direct_values[index])
        result[index + 1] = max(0.0, indirect_values[index + 1] + direct_values[index + 1])
        result[index + 2] = max(0.0, indirect_values[index + 2] + direct_values[index + 2])
        result[index + 3] = 1.0
    return result


def _maximum_rgb(pixels: Iterable[float]) -> float:
    values = list(pixels)
    if len(values) % 4 != 0:
        raise RuntimeError("Blender lightmap pixels must be RGBA")
    return max((max(values[index:index + 3])
                for index in range(0, len(values), 4)), default=0.0)


def bake_scene_lightmaps(
    blender: object,
    source: Path,
    output_root: Path,
    objects: Iterable[object],
    scene_name: str,
    prefab_objects: dict[str, list[object]] | None = None,
    logger: Callable[[str], None] | None = None,
) -> tuple[dict[str, LightmapPlacement],
           dict[str, tuple[tuple[int, LightmapPlacement], ...]], list[Path]]:
    object_list = list(objects)
    has_baked_lights = any(
        getattr(obj, "type", "") == "LIGHT" and
        str(getattr(obj, "get", lambda *_: "realtime")(
            "ce_light_mode", "realtime")).lower() in {"baked", "mixed"}
        for obj in object_list)
    if not has_baked_lights:
        return {}, {}, []
    targets = _bake_targets(object_list, prefab_objects or {})
    if not targets:
        return {}, {}, []

    scene = blender.context.scene
    resolution = int(getattr(scene, "get", lambda *_: DEFAULT_RESOLUTION)(
        "ce_lightmap_resolution", DEFAULT_RESOLUTION))
    padding = int(getattr(scene, "get", lambda *_: DEFAULT_PADDING)(
        "ce_lightmap_padding", DEFAULT_PADDING))
    rgbm_range = float(getattr(scene, "get", lambda *_: DEFAULT_RGBM_RANGE)(
        "ce_lightmap_rgbm_range", DEFAULT_RGBM_RANGE))
    tiles = plan_atlas((target.key for target in targets), resolution, padding)
    source_meshes = list({int(target.source.as_pointer()): target.source for target in targets}.values())
    ensure_lightmap_uvs(blender, source_meshes)

    output_dir = output_dir_for_source(source, output_root) / "lightmaps"
    output = output_dir / f"{clean_asset_name(scene_name)}_0.dds"
    indirect_image = blender.data.images.new(
        f"CEngine_{scene_name}_Indirect", width=resolution, height=resolution,
        alpha=True, float_buffer=True)
    direct_image = blender.data.images.new(
        f"CEngine_{scene_name}_StaticDirect", width=resolution, height=resolution,
        alpha=True, float_buffer=True)
    previous_engine = scene.render.engine
    previous_bake_clear = scene.render.bake.use_clear
    previous_pass_color = scene.render.bake.use_pass_color
    previous_pass_direct = scene.render.bake.use_pass_direct
    previous_pass_indirect = scene.render.bake.use_pass_indirect
    previous_active = getattr(blender.context.view_layer.objects, "active", None)
    previous_selected = tuple(getattr(blender.context, "selected_objects", ()))
    temporary_objects: list[object] = []
    temporary_meshes: list[object] = []
    temporary_nodes: list[tuple[object, object]] = []
    temporary_materials: list[object] = []
    prepared_materials: set[int] = set()
    bake_materials: dict[int, object] = {}
    hidden_objects: list[tuple[object, bool]] = []
    hidden_object_keys: set[int] = set()

    def hide_object(obj: object) -> None:
        key = int(obj.as_pointer())
        if key in hidden_object_keys:
            return
        hidden_object_keys.add(key)
        hidden_objects.append((obj, bool(obj.hide_render)))
        obj.hide_render = True

    def set_light_visibility(visible_modes: set[str]) -> None:
        for obj in object_list:
            if getattr(obj, "type", "") != "LIGHT":
                continue
            mode = str(getattr(obj, "get", lambda *_: "realtime")(
                "ce_light_mode", "realtime")).lower()
            obj.hide_render = mode not in visible_modes

    def target_image(image: object) -> None:
        for material, node in temporary_nodes:
            node.image = image
            for existing in material.node_tree.nodes:
                existing.select = False
            material.node_tree.nodes.active = node
            node.select = True

    def bake_pass(image: object, use_direct: bool, use_indirect: bool,
                  visible_modes: set[str]) -> None:
        target_image(image)
        set_light_visibility(visible_modes)
        scene.render.bake.use_pass_color = False
        scene.render.bake.use_pass_direct = use_direct
        scene.render.bake.use_pass_indirect = use_indirect
        for index, target in enumerate(temporary_objects):
            _select_only(blender, target)
            scene.render.bake.use_clear = index == 0
            blender.ops.object.bake(type="DIFFUSE", margin=padding, use_selected_to_active=False)

    try:
        scene.render.engine = "CYCLES"
        scene.render.bake.use_clear = True
        # Build a closed bake stage. Blender collections used as prefab
        # prototypes can also be linked into the scene, and collection instances
        # may evaluate the same source mesh again. Hiding every original scene
        # object before adding the concrete bake copies guarantees that exactly
        # one copy of each static surface can cast and receive baked light. It
        # also prevents unrelated objects or lights outside the exported scene
        # collection from affecting the result.
        for obj in tuple(getattr(scene, "objects", ())):
            hide_object(obj)
        for obj in object_list:
            hide_object(obj)
        for source in source_meshes:
            hide_object(source)

        if logger is not None:
            logger(
                f"Lightmap bake stage: hid {len(hidden_objects)} original object(s); "
                f"creating {len(targets)} unique static surface copy/copies")

        for bake_index, target in enumerate(targets):
            obj = target.source.copy()
            obj.data = target.source.data.copy()
            obj.name = f"CEngineBake_{bake_index}_{target.source.name}"
            obj.matrix_world = target.world_matrix
            obj.hide_render = False
            blender.context.scene.collection.objects.link(obj)
            temporary_objects.append(obj)
            temporary_meshes.append(obj.data)
            _copy_bake_uv(obj.data, tiles[target.key])
            if not obj.material_slots:
                material = blender.data.materials.new(f"CEngineBake_{target.source.name}")
                material.use_nodes = True
                obj.data.materials.append(material)
                temporary_materials.append(material)
            for slot in obj.material_slots:
                source_material = slot.material
                if source_material is None:
                    material = blender.data.materials.new(f"CEngineBake_{obj.name}")
                    material.use_nodes = True
                    slot.material = material
                    temporary_materials.append(material)
                else:
                    source_key = int(source_material.as_pointer())
                    material = bake_materials.get(source_key)
                    if material is None:
                        material = source_material.copy()
                        bake_materials[source_key] = material
                        temporary_materials.append(material)
                    slot.material = material
                key = int(material.as_pointer())
                if key in prepared_materials:
                    continue
                prepared_materials.add(key)
                material.use_nodes = True
                for existing in material.node_tree.nodes:
                    existing.select = False
                node = material.node_tree.nodes.new("ShaderNodeTexImage")
                node.image = indirect_image
                material.node_tree.nodes.active = node
                node.select = True
                temporary_nodes.append((material, node))

        # Mixed lights contribute bounced light here but retain realtime direct light.
        bake_pass(indirect_image, use_direct=False, use_indirect=True,
                  visible_modes={"baked", "mixed"})
        # Fully baked lights contribute direct light. The world remains visible, so
        # environment illumination is captured without baking mixed direct light twice.
        bake_pass(direct_image, use_direct=True, use_indirect=False,
                  visible_modes={"baked"})
        indirect_pixels = indirect_image.pixels[:]
        direct_pixels = direct_image.pixels[:]
        combined_pixels = _combine_light_passes(indirect_pixels, direct_pixels)
        indirect_max = _maximum_rgb(indirect_pixels)
        direct_max = _maximum_rgb(direct_pixels)
        combined_max = _maximum_rgb(combined_pixels)
        if logger is not None:
            logger(
                f"Lightmap bake energy: indirect={indirect_max:.6g}, "
                f"static-direct={direct_max:.6g}, combined={combined_max:.6g}")
        if combined_max <= MINIMUM_LIGHTMAP_ENERGY:
            raise RuntimeError(
                "Cycles produced an empty lightmap; check light modes, render visibility, "
                "light linking, and overlapping prototype/instance geometry")
        _save_rgbm(combined_pixels, resolution, rgbm_range, output)
    finally:
        for material, node in reversed(temporary_nodes):
            material.node_tree.nodes.remove(node)
        for obj in reversed(temporary_objects):
            blender.data.objects.remove(obj, do_unlink=True)
        for mesh in reversed(temporary_meshes):
            if mesh.users == 0:
                blender.data.meshes.remove(mesh)
        for material in temporary_materials:
            if material.users == 0:
                blender.data.materials.remove(material)
        for obj, hidden in hidden_objects:
            obj.hide_render = hidden
        blender.data.images.remove(indirect_image)
        blender.data.images.remove(direct_image)
        scene.render.engine = previous_engine
        scene.render.bake.use_clear = previous_bake_clear
        scene.render.bake.use_pass_color = previous_pass_color
        scene.render.bake.use_pass_direct = previous_pass_direct
        scene.render.bake.use_pass_indirect = previous_pass_indirect
        for selected in tuple(getattr(blender.context, "selected_objects", ())):
            selected.select_set(False)
        for selected in previous_selected:
            selected.select_set(True)
        blender.context.view_layer.objects.active = previous_active

    if logger is not None:
        logger(f"Lightmap bake: wrote {output.name} ({resolution}x{resolution}, {len(targets)} placement(s))")
    prop_placements: dict[str, LightmapPlacement] = {}
    prefab_placements: dict[str, list[tuple[int, LightmapPlacement]]] = {}
    for target in targets:
        tile = tiles[target.key]
        placement = LightmapPlacement(output, tile.scale, tile.offset, rgbm_range)
        if target.prop_name is not None:
            prop_placements[target.prop_name] = placement
        elif target.prefab_instance is not None and target.prefab_object_index is not None:
            prefab_placements.setdefault(target.prefab_instance, []).append(
                (target.prefab_object_index, placement))
    return (prop_placements,
            {name: tuple(sorted(values)) for name, values in prefab_placements.items()},
            [output])
