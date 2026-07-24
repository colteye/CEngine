#   _____ ______             _
#  / ____|  ____|           (_)
# | |    | |__   _ __   __ _ _ _ __   ___
# | |    |  __| | '_ \ / _` | | '_ \ / _ |
# | |____| |____| | | | (_| | | | | |  __/
#  \_____|______|_| |_|\__, |_|_| |_|\___|
#                       __/ |
#                      |___/

"""TODO: Briefly describe this module.

Author:
    Erik Coltey
"""

from __future__ import annotations

import math
import tempfile
from array import array
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Iterable

from ceassetlib.blender_scene import LightmapPlacement
from ceassetlib.collection_export import clean_asset_name, object_role
from ceassetlib.paths import output_dir_for_source
from ceassetlib.texture import write_rgbexp32_dds


LIGHTMAP_UV = "Lightmap"
BAKE_UV = "CEngineBake"
DEFAULT_RESOLUTION = 4096
DEFAULT_PADDING = 8
LIGHTMAP_PACK_MARGIN = 0.0001
NORMAL_SEAM_DOT_THRESHOLD = math.cos(math.radians(45.0))
GEOMETRY_SEAM_ANGLE_DEGREES = 140.0
ENABLE_WARP_FALLBACK = True
MAX_UV_STRETCH_RATIO = 64.0
MAX_TEXEL_DENSITY_RATIO = 4.0
TEXEL_DENSITY_TRIM_FRACTION = 0.10
MIN_TEXEL_DENSITY_SAMPLES = 10
FALLBACK_SMART_PROJECT_ANGLE_DEGREES = 66.0
UV_EPSILON = 1.0e-7
DEFAULT_RGBM_RANGE = 8.0
HDR_LIGHTMAP_DECODE_SCALE = 1.0
DEFAULT_DENOISE = True
DEFAULT_SAMPLES = 1024
ALPHA_CLIP_ENABLED = False
ALPHA_CLIP_THRESHOLD = 0.5
MINIMUM_LIGHTMAP_ENERGY = 1.0e-5
INDIRECT_BAKE_LIGHT_MODES = frozenset({"baked", "mixed"})
DIRECT_BAKE_LIGHT_MODES = frozenset({"baked"})


@dataclass(frozen=True)
class AtlasTile:
    """TODO: Describe `AtlasTile`."""

    scale: tuple[float, float]
    offset: tuple[float, float]


@dataclass(frozen=True)
class BakeTarget:
    """TODO: Describe `BakeTarget`."""

    key: str
    source: object
    world_matrix: object
    prop_name: str | None = None
    prefab_instance: str | None = None
    prefab_object_index: int | None = None


@dataclass(frozen=True)
class LightmapSettings:
    """TODO: Describe `LightmapSettings`."""

    resolution: int = DEFAULT_RESOLUTION
    padding: int = DEFAULT_PADDING
    samples: int = DEFAULT_SAMPLES
    denoise: bool = DEFAULT_DENOISE

    @classmethod
    def from_scene(cls, scene: object) -> "LightmapSettings":
        """TODO: Describe `from_scene`.

        Args:
            scene: TODO: Describe this parameter.

        Returns:
            TODO: Describe the produced value.
        """
        getter = getattr(scene, "get", lambda _key, default: default)
        return cls(
            resolution=int(getter(
                "ce_lightmap_resolution", DEFAULT_RESOLUTION)),
            padding=int(getter("ce_lightmap_padding", DEFAULT_PADDING)),
            samples=max(1, int(getter(
                "ce_lightmap_samples", DEFAULT_SAMPLES))),
            denoise=bool(getter("ce_lightmap_denoise", DEFAULT_DENOISE)),
        )


def _light_mode(light: object) -> str:
    """Return the normalized CEngine bake mode for a Blender light."""
    return str(getattr(light, "get", lambda *_: "realtime")(
        "ce_light_mode", "realtime")).strip().lower()


def _set_bake_light_visibility(
    light_states: Iterable[tuple[object, bool]],
    allowed_modes: frozenset[str],
) -> None:
    """Expose only originally-visible lights whose mode participates in a pass."""
    for light, was_hidden in light_states:
        light.hide_render = was_hidden or _light_mode(light) not in allowed_modes


def plan_atlas(names: Iterable[str], resolution: int, padding: int) -> dict[str, AtlasTile]:
    """TODO: Describe `plan_atlas`.

    Args:
        names: TODO: Describe this parameter.
        resolution: TODO: Describe this parameter.
        padding: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    ordered = sorted(set(names))
    if not ordered:
        return {}
    if resolution <= 0 or padding < 0:
        raise ValueError("lightmap resolution and padding must be positive")
    if len(ordered) == 1:
        return {ordered[0]: AtlasTile((1.0, 1.0), (0.0, 0.0))}

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
    """TODO: Describe `encode_rgbm`.

    Args:
        pixels: TODO: Describe this parameter.
        rgbm_range: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    values = pixels if hasattr(pixels, "__len__") and hasattr(pixels, "__getitem__") else list(pixels)
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


def encode_rgbexp32(pixels: Iterable[float]) -> bytes:
    """TODO: Describe `encode_rgbexp32`.

    Args:
        pixels: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    values = pixels if hasattr(pixels, "__len__") and hasattr(pixels, "__getitem__") else list(pixels)
    if len(values) % 4 != 0:
        raise ValueError("Blender lightmap pixels must be RGBA")

    encoded = bytearray(len(values))
    for index in range(0, len(values), 4):
        red = max(0.0, float(values[index]))
        green = max(0.0, float(values[index + 1]))
        blue = max(0.0, float(values[index + 2]))
        maximum = max(red, green, blue)
        if maximum <= 0.0:
            exponent = -128
            scale = 0.0
        else:
            exponent = max(-127, min(127, math.ceil(math.log2(maximum))))
            scale = 255.0 / math.ldexp(1.0, exponent)
        encoded[index] = round(min(255.0, red * scale))
        encoded[index + 1] = round(min(255.0, green * scale))
        encoded[index + 2] = round(min(255.0, blue * scale))
        encoded[index + 3] = exponent + 128
    return bytes(encoded)


def _static_meshes(objects: Iterable[object]) -> list[object]:
    """TODO: Describe `_static_meshes`.

    Args:
        objects: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    return sorted((obj for obj in objects
                   if getattr(obj, "type", "") == "MESH"
                   and object_role(obj) != "occluder"
                   and str(getattr(obj, "get", lambda *_: "None")(
                       "ce_physics_motion", "None")).lower()
                       not in {"dynamic", "kinematic"}),
                  key=lambda obj: obj.name)


def _select_only(blender: object, obj: object) -> None:
    """TODO: Describe `_select_only`.

    Args:
        blender: TODO: Describe this parameter.
        obj: TODO: Describe this parameter.
    """
    for selected in tuple(getattr(blender.context, "selected_objects", ())):
        selected.select_set(False)
    obj.select_set(True)
    blender.context.view_layer.objects.active = obj


def _edge_has_visual_seam(mesh: object, edge_index: int,
                          edge_face_loops: list[list[tuple[int, tuple[int, int]]]]) -> bool:
    """TODO: Describe `_edge_has_visual_seam`.

    Args:
        mesh: TODO: Describe this parameter.
        edge_index: TODO: Describe this parameter.
        edge_face_loops: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    face_loops = edge_face_loops[edge_index]
    if len(face_loops) != 2:
        return True

    first_face_index, first_loops = face_loops[0]
    second_face_index, second_loops = face_loops[1]
    geometry_seam_dot = math.cos(math.radians(GEOMETRY_SEAM_ANGLE_DEGREES))
    if mesh.polygons[first_face_index].normal.dot(
            mesh.polygons[second_face_index].normal) < geometry_seam_dot:
        return True

    first_by_vertex = {
        mesh.loops[loop_index].vertex_index: loop_index for loop_index in first_loops
    }
    second_by_vertex = {
        mesh.loops[loop_index].vertex_index: loop_index for loop_index in second_loops
    }
    for vertex_index in first_by_vertex.keys() & second_by_vertex.keys():
        first_normal = mesh.corner_normals[first_by_vertex[vertex_index]].vector
        second_normal = mesh.corner_normals[second_by_vertex[vertex_index]].vector
        if first_normal.dot(second_normal) < NORMAL_SEAM_DOT_THRESHOLD:
            return True
    return False


def _mark_visual_seams(mesh: object) -> list[bool]:
    """TODO: Describe `_mark_visual_seams`.

    Args:
        mesh: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    original_seams = [bool(edge.use_seam) for edge in mesh.edges]
    sharp_edges = mesh.attributes.get("sharp_edge")
    edge_face_loops: list[list[tuple[int, tuple[int, int]]]] = [
        [] for _ in mesh.edges
    ]
    for polygon in mesh.polygons:
        loop_indices = list(polygon.loop_indices)
        for offset, loop_index in enumerate(loop_indices):
            next_loop_index = loop_indices[(offset + 1) % len(loop_indices)]
            edge_index = mesh.loops[loop_index].edge_index
            edge_face_loops[edge_index].append(
                (polygon.index, (loop_index, next_loop_index)))

    for edge in mesh.edges:
        explicitly_sharp = (
            sharp_edges is not None
            and sharp_edges.domain == "EDGE"
            and bool(sharp_edges.data[edge.index].value)
        )
        edge.use_seam = (
            original_seams[edge.index]
            or explicitly_sharp
            or _edge_has_visual_seam(mesh, edge.index, edge_face_loops)
        )
    return original_seams


def _restore_seams(mesh: object, original_seams: Iterable[bool]) -> None:
    """TODO: Describe `_restore_seams`.

    Args:
        mesh: TODO: Describe this parameter.
        original_seams: TODO: Describe this parameter.
    """
    for edge, use_seam in zip(mesh.edges, original_seams):
        edge.use_seam = use_seam


def _uv_coordinates_match(first_uv: object, second_uv: object) -> bool:
    """TODO: Describe `_uv_coordinates_match`.

    Args:
        first_uv: TODO: Describe this parameter.
        second_uv: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    return (abs(first_uv.x - second_uv.x) <= UV_EPSILON
            and abs(first_uv.y - second_uv.y) <= UV_EPSILON)


def _uv_islands(mesh: object, uv_layer: object) -> list[list[int]]:
    """TODO: Describe `_uv_islands`.

    Args:
        mesh: TODO: Describe this parameter.
        uv_layer: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    parents = list(range(len(mesh.polygons)))
    edge_faces: list[list[tuple[int, tuple[int, int]]]] = [[] for _ in mesh.edges]

    def find(index: int) -> int:
        """TODO: Describe `find`.

        Args:
            index: TODO: Describe this parameter.

        Returns:
            TODO: Describe the produced value.
        """
        while parents[index] != index:
            parents[index] = parents[parents[index]]
            index = parents[index]
        return index

    def union(first: int, second: int) -> None:
        """TODO: Describe `union`.

        Args:
            first: TODO: Describe this parameter.
            second: TODO: Describe this parameter.
        """
        first_root = find(first)
        second_root = find(second)
        if first_root != second_root:
            parents[second_root] = first_root

    for polygon in mesh.polygons:
        loop_indices = list(polygon.loop_indices)
        for offset, loop_index in enumerate(loop_indices):
            next_loop_index = loop_indices[(offset + 1) % len(loop_indices)]
            edge_faces[mesh.loops[loop_index].edge_index].append(
                (polygon.index, (loop_index, next_loop_index)))

    for entries in edge_faces:
        if len(entries) != 2:
            continue
        first_polygon, first_loops = entries[0]
        second_polygon, second_loops = entries[1]
        first_by_vertex = {
            mesh.loops[index].vertex_index: uv_layer.data[index].uv for index in first_loops
        }
        second_by_vertex = {
            mesh.loops[index].vertex_index: uv_layer.data[index].uv for index in second_loops
        }
        if first_by_vertex.keys() == second_by_vertex.keys() and all(
                _uv_coordinates_match(first_by_vertex[vertex], second_by_vertex[vertex])
                for vertex in first_by_vertex):
            union(first_polygon, second_polygon)

    islands: dict[int, list[int]] = {}
    for polygon in mesh.polygons:
        islands.setdefault(find(polygon.index), []).append(polygon.index)
    return list(islands.values())


def _triangle_aspect_ratio(points: list[object]) -> float:
    """TODO: Describe `_triangle_aspect_ratio`.

    Args:
        points: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    lengths = ((points[1] - points[0]).length,
               (points[2] - points[1]).length,
               (points[0] - points[2]).length)
    shortest = min(lengths)
    return float("inf") if shortest <= UV_EPSILON else max(lengths) / shortest


def _trimmed_density_ratio(densities: Iterable[float]) -> float | None:
    """TODO: Describe `_trimmed_density_ratio`.

    Args:
        densities: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    values = sorted(float(value) for value in densities)
    if len(values) < MIN_TEXEL_DENSITY_SAMPLES:
        return None
    trim_count = max(1, int(len(values) * TEXEL_DENSITY_TRIM_FRACTION))
    trimmed = values[trim_count:len(values) - trim_count]
    if len(trimmed) < 2 or trimmed[0] <= 0.0:
        return None
    return trimmed[-1] / trimmed[0]


def _island_needs_fallback(mesh: object, uv_layer: object,
                           polygon_indices: Iterable[int]) -> bool:
    """TODO: Describe `_island_needs_fallback`.

    Args:
        mesh: TODO: Describe this parameter.
        uv_layer: TODO: Describe this parameter.
        polygon_indices: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    texel_densities: list[float] = []
    for polygon_index in polygon_indices:
        loop_indices = list(mesh.polygons[polygon_index].loop_indices)
        for offset in range(1, len(loop_indices) - 1):
            triangle_loops = (loop_indices[0], loop_indices[offset], loop_indices[offset + 1])
            world_points = [
                mesh.vertices[mesh.loops[index].vertex_index].co for index in triangle_loops
            ]
            uv_points = [uv_layer.data[index].uv for index in triangle_loops]
            world_area = ((world_points[1] - world_points[0]).cross(
                world_points[2] - world_points[0]).length * 0.5)
            uv_area = abs(
                (uv_points[1].x - uv_points[0].x) * (uv_points[2].y - uv_points[0].y)
                - (uv_points[1].y - uv_points[0].y) *
                (uv_points[2].x - uv_points[0].x)) * 0.5
            if world_area <= UV_EPSILON or uv_area <= UV_EPSILON:
                continue
            world_aspect = _triangle_aspect_ratio(world_points)
            uv_aspect = _triangle_aspect_ratio(uv_points)
            if max(uv_aspect / world_aspect,
                   world_aspect / uv_aspect) > MAX_UV_STRETCH_RATIO:
                return True
            texel_densities.append(uv_area / world_area)

    density_ratio = _trimmed_density_ratio(texel_densities)
    return density_ratio is not None and density_ratio > MAX_TEXEL_DENSITY_RATIO


def _smart_project_bad_islands(blender: object, obj: object) -> int:
    """TODO: Describe `_smart_project_bad_islands`.

    Args:
        blender: TODO: Describe this parameter.
        obj: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    mesh = obj.data
    uv_layer = mesh.uv_layers.get(LIGHTMAP_UV)
    if uv_layer is None:
        raise RuntimeError(f"mesh has no {LIGHTMAP_UV} UV layer")
    bad_polygons: set[int] = set()
    for island in _uv_islands(mesh, uv_layer):
        if _island_needs_fallback(mesh, uv_layer, island):
            bad_polygons.update(island)
    if not bad_polygons:
        return 0

    for polygon in mesh.polygons:
        polygon.select = polygon.index in bad_polygons
    blender.ops.object.mode_set(mode="EDIT")
    blender.ops.uv.smart_project(
        angle_limit=math.radians(FALLBACK_SMART_PROJECT_ANGLE_DEGREES),
        island_margin=0.0,
        area_weight=0.0,
        correct_aspect=True,
        scale_to_bounds=False,
    )
    blender.ops.object.mode_set(mode="OBJECT")
    return len(bad_polygons)


def ensure_lightmap_uvs(
    blender: object,
    objects: Iterable[object],
    resolution: int = DEFAULT_RESOLUTION,
    padding: int = DEFAULT_PADDING,
) -> None:
    """TODO: Describe `ensure_lightmap_uvs`.

    Args:
        blender: TODO: Describe this parameter.
        objects: TODO: Describe this parameter.
        resolution: TODO: Describe this parameter.
        padding: TODO: Describe this parameter.
    """
    previous_active = getattr(blender.context.view_layer.objects, "active", None)
    previous_selected = tuple(getattr(blender.context, "selected_objects", ()))
    previous_mode = str(getattr(blender.context, "mode", "OBJECT"))
    try:
        if previous_mode != "OBJECT":
            blender.ops.object.mode_set(mode="OBJECT")
        for obj in _static_meshes(objects):
            layers = obj.data.uv_layers
            if not layers:
                raise RuntimeError(f"mesh {obj.name} has no UV0 layer to repack")
            previous_uv = getattr(layers, "active", None)
            previous_uv_name = str(getattr(previous_uv, "name", ""))
            previous_render_uvs = [
                (str(getattr(existing, "name", "")), bool(getattr(existing, "active_render", False)))
                for existing in layers
            ]
            uv0 = layers[0]
            uv0_coordinates = [0.0] * (len(uv0.data) * 2)
            uv0.data.foreach_get("uv", uv0_coordinates)
            layer = layers.get(LIGHTMAP_UV)
            if layer is not None:
                # UV1 is a cooked derivative, not authored material data.
                layers.remove(layer)
            layer = layers.new(name=LIGHTMAP_UV)
            layer.data.foreach_set("uv", uv0_coordinates)
            obj.data.update()
            try:
                layers.active = layer
                _select_only(blender, obj)
                original_seams = _mark_visual_seams(obj.data)
                blender.ops.object.mode_set(mode="EDIT")
                blender.ops.mesh.select_all(action="SELECT")
                blender.ops.uv.unwrap(method="ANGLE_BASED")
                blender.ops.object.mode_set(mode="OBJECT")
                _restore_seams(obj.data, original_seams)
                if ENABLE_WARP_FALLBACK:
                    _smart_project_bad_islands(blender, obj)
                blender.ops.object.mode_set(mode="EDIT")
                blender.ops.mesh.select_all(action="SELECT")
                blender.ops.uv.average_islands_scale()
                blender.ops.uv.pack_islands(
                    rotate=True,
                    scale=True,
                    merge_overlap=False,
                    margin=LIGHTMAP_PACK_MARGIN,
                )
                blender.ops.object.mode_set(mode="OBJECT")
            finally:
                if str(getattr(blender.context, "mode", "OBJECT")) != "OBJECT":
                    blender.ops.object.mode_set(mode="OBJECT")
                for existing_name, active_render in previous_render_uvs:
                    existing = layers.get(existing_name)
                    if existing is not None:
                        existing.active_render = active_render
                restored_uv = layers.get(previous_uv_name) if previous_uv_name else None
                if restored_uv is not None:
                    layers.active = restored_uv
    finally:
        if str(getattr(blender.context, "mode", "OBJECT")) != "OBJECT":
            blender.ops.object.mode_set(mode="OBJECT")
        for selected in tuple(getattr(blender.context, "selected_objects", ())):
            selected.select_set(False)
        for selected in previous_selected:
            selected.select_set(True)
        blender.context.view_layer.objects.active = previous_active


def _copy_bake_uv(mesh: object, tile: AtlasTile) -> None:
    """TODO: Describe `_copy_bake_uv`.

    Args:
        mesh: TODO: Describe this parameter.
        tile: TODO: Describe this parameter.
    """
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


def _save_hdr(pixels: Iterable[float], resolution: int, dds_path: Path) -> None:
    """TODO: Describe `_save_hdr`.

    Args:
        pixels: TODO: Describe this parameter.
        resolution: TODO: Describe this parameter.
        dds_path: TODO: Describe this parameter.
    """
    write_rgbexp32_dds(dds_path, resolution, resolution, encode_rgbexp32(pixels))


def _bake_targets(objects: Iterable[object],
                  prefab_objects: dict[str, list[object]]) -> list[BakeTarget]:
    """TODO: Describe `_bake_targets`.

    Args:
        objects: TODO: Describe this parameter.
        prefab_objects: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    targets: list[BakeTarget] = []
    for obj in sorted(objects, key=lambda value: str(getattr(value, "name", ""))):
        if (getattr(obj, "type", "") == "MESH" and
                object_role(obj) != "occluder" and
                str(getattr(obj, "get", lambda *_: "None")(
                    "ce_physics_motion", "None")).lower()
                    not in {"dynamic", "kinematic"}):
            targets.append(BakeTarget(f"prop:{obj.name}", obj, obj.matrix_world, prop_name=obj.name))
            continue
        sources = prefab_objects.get(str(getattr(obj, "name", "")))
        if sources is None:
            continue
        for object_index, source in enumerate(sorted(sources, key=lambda value: value.name)):
            if (getattr(source, "type", "") != "MESH" or
                    object_role(source) == "occluder" or
                    str(getattr(source, "get", lambda *_: "None")(
                        "ce_physics_motion", "None")).lower()
                        in {"dynamic", "kinematic"}):
                continue
            targets.append(BakeTarget(
                f"prefab:{obj.name}:{object_index}:{source.name}", source,
                obj.matrix_world @ source.matrix_world,
                prefab_instance=obj.name, prefab_object_index=object_index))
    return targets


def _maximum_rgb(pixels: Iterable[float]) -> float:
    """TODO: Describe `_maximum_rgb`.

    Args:
        pixels: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    values = pixels if hasattr(pixels, "__len__") and hasattr(pixels, "__getitem__") else list(pixels)
    if len(values) % 4 != 0:
        raise RuntimeError("Blender lightmap pixels must be RGBA")
    return max((max(values[index:index + 3])
                for index in range(0, len(values), 4)), default=0.0)


def _active_material_output(node_tree: object) -> object | None:
    """TODO: Describe `_active_material_output`.

    Args:
        node_tree: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    for node in node_tree.nodes:
        if getattr(node, "type", "") == "OUTPUT_MATERIAL" and bool(
                getattr(node, "is_active_output", False)):
            return node
    return None


def _prepare_material_alpha_for_bake(material: object) -> dict[str, object] | None:
    """Temporarily express Principled alpha as transparent/opaque geometry."""
    if material is None or not bool(getattr(material, "use_nodes", False)):
        return None
    node_tree = material.node_tree
    output_node = _active_material_output(node_tree)
    if output_node is None:
        return None
    surface_input = output_node.inputs.get("Surface")
    if surface_input is None or not surface_input.is_linked:
        return None
    output_surface_link = surface_input.links[0]
    shader_socket = output_surface_link.from_socket
    shader_node = output_surface_link.from_node
    if getattr(shader_node, "type", "") != "BSDF_PRINCIPLED":
        return None
    alpha_input = shader_node.inputs.get("Alpha")
    if alpha_input is None:
        return None
    alpha_link = alpha_input.links[0] if alpha_input.is_linked else None
    original_alpha_default = float(alpha_input.default_value)
    if alpha_link is None and original_alpha_default >= 1.0 - UV_EPSILON:
        return None

    temporary_nodes: list[object] = []
    transparent_node = node_tree.nodes.new("ShaderNodeBsdfTransparent")
    transparent_node.name = "LightBake_TemporaryTransparent"
    transparent_node.label = "Temporary Light Bake Transparency"
    temporary_nodes.append(transparent_node)
    mix_node = node_tree.nodes.new("ShaderNodeMixShader")
    mix_node.name = "LightBake_TemporaryAlphaMix"
    mix_node.label = "Temporary Light Bake Alpha Mix"
    temporary_nodes.append(mix_node)
    mix_node.location = (output_node.location.x - 220.0, output_node.location.y)
    transparent_node.location = (mix_node.location.x - 220.0,
                                 mix_node.location.y + 120.0)

    alpha_source_socket = None
    if alpha_link is not None:
        alpha_source_socket = alpha_link.from_socket
        node_tree.links.remove(alpha_link)
        alpha_input.default_value = 1.0
    else:
        alpha_input.default_value = 1.0

    factor_socket = mix_node.inputs[0]
    if alpha_source_socket is not None:
        if ALPHA_CLIP_ENABLED:
            threshold_node = node_tree.nodes.new("ShaderNodeMath")
            threshold_node.name = "LightBake_TemporaryAlphaThreshold"
            threshold_node.label = "Temporary Light Bake Alpha Threshold"
            threshold_node.operation = "GREATER_THAN"
            threshold_node.inputs[1].default_value = ALPHA_CLIP_THRESHOLD
            threshold_node.location = (mix_node.location.x - 220.0,
                                       mix_node.location.y - 100.0)
            temporary_nodes.append(threshold_node)
            node_tree.links.new(alpha_source_socket, threshold_node.inputs[0])
            node_tree.links.new(threshold_node.outputs[0], factor_socket)
        else:
            node_tree.links.new(alpha_source_socket, factor_socket)
    else:
        factor_socket.default_value = original_alpha_default

    node_tree.links.remove(output_surface_link)
    node_tree.links.new(transparent_node.outputs["BSDF"], mix_node.inputs[1])
    node_tree.links.new(shader_socket, mix_node.inputs[2])
    node_tree.links.new(mix_node.outputs["Shader"], surface_input)
    return {
        "node_tree": node_tree,
        "output_node": output_node,
        "shader_socket": shader_socket,
        "alpha_input": alpha_input,
        "alpha_source_socket": alpha_source_socket,
        "original_alpha_default": original_alpha_default,
        "temporary_nodes": temporary_nodes,
    }


def _restore_material_alpha_after_bake(change: dict[str, object]) -> None:
    """TODO: Describe `_restore_material_alpha_after_bake`.

    Args:
        change: TODO: Describe this parameter.
    """
    node_tree = change["node_tree"]
    output_node = change["output_node"]
    surface_input = output_node.inputs.get("Surface")
    if surface_input is not None:
        for link in list(surface_input.links):
            node_tree.links.remove(link)
    for node in reversed(change["temporary_nodes"]):
        if node.id_data is node_tree:
            node_tree.nodes.remove(node)
    if surface_input is not None:
        node_tree.links.new(change["shader_socket"], surface_input)
    alpha_input = change["alpha_input"]
    alpha_input.default_value = change["original_alpha_default"]
    alpha_source_socket = change["alpha_source_socket"]
    if alpha_source_socket is not None:
        node_tree.links.new(alpha_source_socket, alpha_input)


def _compose_lightmaps(
    blender: object,
    source_images: tuple[object, ...],
    resolution: int,
    denoise: bool,
    logger: Callable[[str], None] | None = None,
) -> array:
    """Add linear bake passes and optionally denoise their combined result.

    Args:
        blender: TODO: Describe this parameter.
        source_images: Linear Blender images to add.
        resolution: TODO: Describe this parameter.
        denoise: Whether to run the compositor denoise filters.
        logger: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    if not source_images:
        raise ValueError("at least one lightmap pass is required")
    scene = blender.context.scene
    previous_group = scene.compositing_node_group
    previous_filepath = scene.render.filepath
    previous_format = scene.render.image_settings.file_format
    previous_color_mode = scene.render.image_settings.color_mode
    previous_color_depth = scene.render.image_settings.color_depth
    previous_resolution = (
        scene.render.resolution_x,
        scene.render.resolution_y,
        scene.render.resolution_percentage,
    )
    compositor = blender.data.node_groups.new(
        "Lightmap_Compositor", "CompositorNodeTree")
    result_image = None
    try:
        scene.compositing_node_group = compositor
        compositor.nodes.clear()
        compositor.interface.clear()
        compositor.interface.new_socket(
            name="Image", in_out="OUTPUT", socket_type="NodeSocketColor")

        image_nodes = []
        for source_image in source_images:
            image_node = compositor.nodes.new("CompositorNodeImage")
            image_node.image = source_image
            image_nodes.append(image_node)
        result_socket = image_nodes[0].outputs["Image"]
        for image_node in image_nodes[1:]:
            add_node = compositor.nodes.new("ShaderNodeVectorMath")
            add_node.operation = "ADD"
            compositor.links.new(result_socket, add_node.inputs[0])
            compositor.links.new(image_node.outputs["Image"], add_node.inputs[1])
            result_socket = add_node.outputs["Vector"]
        if denoise:
            denoise_node = compositor.nodes.new("CompositorNodeDenoise")
            despeckle_node = compositor.nodes.new("CompositorNodeDespeckle")
            despeckle_node.inputs["Factor"].default_value = 1.0
            compositor.links.new(result_socket, denoise_node.inputs["Image"])
            compositor.links.new(
                denoise_node.outputs["Image"], despeckle_node.inputs["Image"])
            result_socket = despeckle_node.outputs["Image"]
        output_node = compositor.nodes.new("NodeGroupOutput")
        compositor.links.new(result_socket, output_node.inputs["Image"])

        if logger is not None:
            operation = "Add -> Denoise -> Despeckle" if denoise else "Add"
            logger(
                f"Lightmap compositor: {len(source_images)} linear pass(es), "
                f"{operation}")
        with tempfile.TemporaryDirectory(prefix="cengine-lightmap-compose-") as temporary:
            scene.render.image_settings.file_format = "OPEN_EXR"
            scene.render.image_settings.color_mode = "RGB"
            scene.render.image_settings.color_depth = "32"
            scene.render.filepath = str(Path(temporary) / "LightBake_Composed.exr")
            scene.render.resolution_x = resolution
            scene.render.resolution_y = resolution
            scene.render.resolution_percentage = 100
            blender.ops.render.render(write_still=True)
            if not Path(scene.render.filepath).is_file():
                raise RuntimeError(
                    f"Composed lightmap EXR was not created: {scene.render.filepath}")
            result_image = blender.data.images.load(scene.render.filepath, check_existing=False)
            result = array("f", [0.0]) * len(source_images[0].pixels)
            result_image.pixels.foreach_get(result)
            return result
    finally:
        if result_image is not None:
            blender.data.images.remove(result_image)
        scene.compositing_node_group = previous_group
        if compositor.users == 0:
            blender.data.node_groups.remove(compositor)
        scene.render.filepath = previous_filepath
        scene.render.image_settings.file_format = previous_format
        scene.render.image_settings.color_mode = previous_color_mode
        scene.render.image_settings.color_depth = previous_color_depth
        scene.render.resolution_x, scene.render.resolution_y, \
            scene.render.resolution_percentage = previous_resolution


def bake_scene_lightmaps(
    blender: object,
    source: Path,
    output_root: Path,
    objects: Iterable[object],
    scene_name: str,
    prefab_objects: dict[str, list[object]] | None = None,
    logger: Callable[[str], None] | None = None,
    settings: LightmapSettings | None = None,
) -> tuple[dict[str, LightmapPlacement],
           dict[str, tuple[tuple[int, LightmapPlacement], ...]], list[Path]]:
    """TODO: Describe `bake_scene_lightmaps`.

    Args:
        blender: TODO: Describe this parameter.
        source: TODO: Describe this parameter.
        output_root: TODO: Describe this parameter.
        objects: TODO: Describe this parameter.
        scene_name: TODO: Describe this parameter.
        prefab_objects: TODO: Describe this parameter.
        logger: TODO: Describe this parameter.
        settings: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    object_list = list(objects)
    has_baked_lights = any(
        getattr(obj, "type", "") == "LIGHT" and
        str(getattr(obj, "get", lambda *_: "realtime")(
            "ce_light_mode", "realtime")).lower() in INDIRECT_BAKE_LIGHT_MODES
        for obj in object_list)
    if not has_baked_lights:
        return {}, {}, []
    targets = _bake_targets(object_list, prefab_objects or {})
    if not targets:
        return {}, {}, []

    scene = blender.context.scene
    settings = settings or LightmapSettings.from_scene(scene)
    resolution = settings.resolution
    padding = settings.padding
    denoise = settings.denoise
    samples = settings.samples
    tiles = plan_atlas((target.key for target in targets), resolution, padding)
    source_meshes = list({int(target.source.as_pointer()): target.source for target in targets}.values())
    ensure_lightmap_uvs(blender, source_meshes, resolution, padding)

    output_dir = output_dir_for_source(source, output_root) / "lightmaps"
    output = output_dir / f"{clean_asset_name(scene_name)}_0.dds"
    pass_images: list[object] = []
    for image_name in ("LightBake_Indirect", "LightBake_Direct"):
        old_image = blender.data.images.get(image_name)
        if old_image is not None:
            blender.data.images.remove(old_image)
        pass_images.append(blender.data.images.new(
            image_name, width=resolution, height=resolution,
            alpha=False, float_buffer=True))
    indirect_image, direct_image = pass_images
    previous_engine = scene.render.engine
    previous_bake_clear = scene.render.bake.use_clear
    previous_pass_color = scene.render.bake.use_pass_color
    previous_pass_direct = scene.render.bake.use_pass_direct
    previous_pass_indirect = scene.render.bake.use_pass_indirect
    cycles = scene.cycles
    previous_samples = cycles.samples
    previous_adaptive_sampling = cycles.use_adaptive_sampling
    previous_adaptive_threshold = cycles.adaptive_threshold
    previous_max_bounces = cycles.max_bounces
    previous_diffuse_bounces = cycles.diffuse_bounces
    previous_glossy_bounces = cycles.glossy_bounces
    previous_transparent_max_bounces = cycles.transparent_max_bounces
    previous_selected_to_active = scene.render.bake.use_selected_to_active
    previous_bake_margin = scene.render.bake.margin
    previous_active = getattr(blender.context.view_layer.objects, "active", None)
    previous_selected = tuple(getattr(blender.context, "selected_objects", ()))
    temporary_objects: list[object] = []
    temporary_meshes: list[object] = []
    temporary_nodes: list[tuple[object, object]] = []
    temporary_materials: list[object] = []
    alpha_material_changes: list[dict[str, object]] = []
    prepared_materials: set[int] = set()
    bake_materials: dict[int, object] = {}
    hidden_objects: list[tuple[object, bool]] = []
    light_states = [
        (obj, bool(obj.hide_render))
        for obj in scene.objects
        if getattr(obj, "type", "") == "LIGHT"
    ]

    def hide_object(obj: object) -> None:
        """TODO: Describe `hide_object`.

        Args:
            obj: TODO: Describe this parameter.
        """
        hidden_objects.append((obj, bool(obj.hide_render)))
        obj.hide_render = True

    def target_image(image: object) -> None:
        """TODO: Describe `target_image`.

        Args:
            image: TODO: Describe this parameter.
        """
        for material, node in temporary_nodes:
            node.image = image
            for existing in material.node_tree.nodes:
                existing.select = False
            material.node_tree.nodes.active = node
            node.select = True

    def bake_pass(image: object, *, direct: bool, indirect: bool) -> None:
        """Bake one diffuse transport component into its own linear image."""
        target_image(image)
        scene.cycles.bake_type = "DIFFUSE"
        scene.render.bake.use_selected_to_active = False
        scene.render.bake.use_clear = True
        scene.render.bake.margin = padding
        scene.render.bake.use_pass_color = False
        scene.render.bake.use_pass_direct = direct
        scene.render.bake.use_pass_indirect = indirect
        for index, target in enumerate(temporary_objects):
            _select_only(blender, target)
            scene.render.bake.use_clear = index == 0
            blender.ops.object.bake(type="DIFFUSE")

    try:
        scene.render.engine = "CYCLES"
        cycles.samples = samples
        cycles.use_adaptive_sampling = True
        cycles.adaptive_threshold = 0.0
        cycles.max_bounces = 8
        cycles.diffuse_bounces = 4
        cycles.glossy_bounces = 1
        cycles.transparent_max_bounces = 8
        if logger is not None:
            logger(
                f"Lightmap Cycles quality: {samples} samples, "
                "separate indirect and baked-direct passes")
        # Replace each receiving source surface with one concrete bake copy.
        # All other scene geometry and lights remain visible, matching the
        # standalone pipeline's lighting environment exactly.
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
                node.name = "LightmapBakeTarget"
                node.label = "Lightmap Bake Target"
                node.image = indirect_image
                material.node_tree.nodes.active = node
                node.select = True
                temporary_nodes.append((material, node))

        processed_materials: set[int] = set()
        for candidate in scene.objects:
            if (getattr(candidate, "type", "") != "MESH"
                    or bool(getattr(candidate, "hide_render", False))):
                continue
            for slot in candidate.material_slots:
                material = slot.material
                if material is None:
                    continue
                material_key = int(material.as_pointer())
                if material_key in processed_materials:
                    continue
                processed_materials.add(material_key)
                change = _prepare_material_alpha_for_bake(material)
                if change is not None:
                    alpha_material_changes.append(change)
        if logger is not None:
            logger(
                "Lightmap alpha preparation: temporarily converted "
                f"{len(alpha_material_changes)} material(s)")

        # Indirect transport is baked from Baked and Mixed lights. Realtime
        # lights never contribute to precomputed lighting.
        _set_bake_light_visibility(light_states, INDIRECT_BAKE_LIGHT_MODES)
        if logger is not None:
            logger(
                "Lightmap indirect pass: World + Baked + Mixed; "
                "Realtime lights hidden")
        bake_pass(indirect_image, direct=False, indirect=True)

        # Direct transport is baked only from Baked lights. The Blender World
        # is not a LIGHT object and intentionally remains enabled so the EXR's
        # geometry-occluded diffuse lighting is stored in the lightmap.
        _set_bake_light_visibility(light_states, DIRECT_BAKE_LIGHT_MODES)
        if logger is not None:
            logger(
                "Lightmap direct pass: World + Baked; "
                "Mixed and Realtime lights hidden")
        bake_pass(direct_image, direct=True, indirect=False)

        combined_pixels = _compose_lightmaps(
            blender, (indirect_image, direct_image), resolution, denoise, logger)
        combined_max = _maximum_rgb(combined_pixels)
        if logger is not None:
            logger(f"Lightmap bake energy: output={combined_max:.6g}")
        if combined_max <= MINIMUM_LIGHTMAP_ENERGY:
            raise RuntimeError(
                "Cycles produced an empty lightmap; check light modes, render visibility, "
                "light linking, and overlapping prototype/instance geometry")

        old_final_image = blender.data.images.get("LightBake_Final")
        if old_final_image is not None:
            blender.data.images.remove(old_final_image)
        final_image = blender.data.images.new(
            "LightBake_Final", width=resolution, height=resolution,
            alpha=False, float_buffer=True)
        final_image.pixels.foreach_set(combined_pixels)
        for target in targets:
            for slot in target.source.material_slots:
                material = slot.material
                if material is None:
                    continue
                if not material.use_nodes:
                    material.use_nodes = True
                old_node = material.node_tree.nodes.get("LightmapBakeTarget")
                if old_node is not None:
                    material.node_tree.nodes.remove(old_node)
                node = material.node_tree.nodes.new("ShaderNodeTexImage")
                node.name = "LightmapBakeTarget"
                node.label = "Lightmap Bake Target"
                node.image = final_image
                for existing in material.node_tree.nodes:
                    existing.select = False
                node.select = True
                material.node_tree.nodes.active = node
        _save_hdr(combined_pixels, resolution, output)
    finally:
        for change in reversed(alpha_material_changes):
            _restore_material_alpha_after_bake(change)
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
        for light, was_hidden in light_states:
            light.hide_render = was_hidden
        for pass_image in pass_images:
            blender.data.images.remove(pass_image)
        scene.render.engine = previous_engine
        scene.render.bake.use_clear = previous_bake_clear
        scene.render.bake.use_pass_color = previous_pass_color
        scene.render.bake.use_pass_direct = previous_pass_direct
        scene.render.bake.use_pass_indirect = previous_pass_indirect
        cycles.samples = previous_samples
        cycles.use_adaptive_sampling = previous_adaptive_sampling
        cycles.adaptive_threshold = previous_adaptive_threshold
        cycles.max_bounces = previous_max_bounces
        cycles.diffuse_bounces = previous_diffuse_bounces
        cycles.glossy_bounces = previous_glossy_bounces
        cycles.transparent_max_bounces = previous_transparent_max_bounces
        scene.render.bake.use_selected_to_active = previous_selected_to_active
        scene.render.bake.margin = previous_bake_margin
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
        placement = LightmapPlacement(
            output, tile.scale, tile.offset, HDR_LIGHTMAP_DECODE_SCALE)
        if target.prop_name is not None:
            prop_placements[target.prop_name] = placement
        elif target.prefab_instance is not None and target.prefab_object_index is not None:
            prefab_placements.setdefault(target.prefab_instance, []).append(
                (target.prefab_object_index, placement))
    return (prop_placements,
            {name: tuple(sorted(values)) for name, values in prefab_placements.items()},
            [output])
