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


LIGHTMAP_UV = "CEngineLightmap"
BAKE_UV = "CEngineBake"
DEFAULT_RESOLUTION = 4096
DEFAULT_PADDING = 64
LIGHTMAP_PACK_MARGIN = 0.001
NORMAL_SEAM_DOT_THRESHOLD = 0.9999
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
DEFAULT_SAMPLES = 256
DEFAULT_INDIRECT_CLAMP = 2.0
DEFAULT_INCLUDE_DIRECT = False
MINIMUM_LIGHTMAP_ENERGY = 1.0e-5
INDIRECT_BAKE_LIGHT_MODES = frozenset({"baked", "mixed"})
DIRECT_BAKE_LIGHT_MODES = frozenset({"baked"})


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
    return sorted((obj for obj in objects
                   if getattr(obj, "type", "") == "MESH"
                   and object_role(obj) != "occluder"
                   and not bool(getattr(obj, "get", lambda *_: False)("ce_dynamic", False))),
                  key=lambda obj: obj.name)


def _select_only(blender: object, obj: object) -> None:
    for selected in tuple(getattr(blender.context, "selected_objects", ())):
        selected.select_set(False)
    obj.select_set(True)
    blender.context.view_layer.objects.active = obj


def _edge_has_visual_seam(mesh: object, edge_index: int,
                          edge_face_loops: list[list[tuple[int, tuple[int, int]]]]) -> bool:
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
    for edge, use_seam in zip(mesh.edges, original_seams):
        edge.use_seam = use_seam


def _uv_coordinates_match(first_uv: object, second_uv: object) -> bool:
    return (abs(first_uv.x - second_uv.x) <= UV_EPSILON
            and abs(first_uv.y - second_uv.y) <= UV_EPSILON)


def _uv_islands(mesh: object, uv_layer: object) -> list[list[int]]:
    parents = list(range(len(mesh.polygons)))
    edge_faces: list[list[tuple[int, tuple[int, int]]]] = [[] for _ in mesh.edges]

    def find(index: int) -> int:
        while parents[index] != index:
            parents[index] = parents[parents[index]]
            index = parents[index]
        return index

    def union(first: int, second: int) -> None:
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
    lengths = ((points[1] - points[0]).length,
               (points[2] - points[1]).length,
               (points[0] - points[2]).length)
    shortest = min(lengths)
    return float("inf") if shortest <= UV_EPSILON else max(lengths) / shortest


def _trimmed_density_ratio(densities: Iterable[float]) -> float | None:
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
    write_rgbexp32_dds(dds_path, resolution, resolution, encode_rgbexp32(pixels))


def _bake_targets(objects: Iterable[object],
                  prefab_objects: dict[str, list[object]]) -> list[BakeTarget]:
    targets: list[BakeTarget] = []
    for obj in sorted(objects, key=lambda value: str(getattr(value, "name", ""))):
        if (getattr(obj, "type", "") == "MESH" and
                object_role(obj) != "occluder" and
                not bool(getattr(obj, "get", lambda *_: False)("ce_dynamic", False))):
            targets.append(BakeTarget(f"prop:{obj.name}", obj, obj.matrix_world, prop_name=obj.name))
            continue
        sources = prefab_objects.get(str(getattr(obj, "name", "")))
        if sources is None:
            continue
        for object_index, source in enumerate(sorted(sources, key=lambda value: value.name)):
            if (getattr(source, "type", "") != "MESH" or
                    object_role(source) == "occluder" or
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
    values = pixels if hasattr(pixels, "__len__") and hasattr(pixels, "__getitem__") else list(pixels)
    if len(values) % 4 != 0:
        raise RuntimeError("Blender lightmap pixels must be RGBA")
    return max((max(values[index:index + 3])
                for index in range(0, len(values), 4)), default=0.0)


def _clamp_rgb(pixels: object, maximum: float) -> object:
    if maximum <= 0.0:
        return pixels
    for index in range(0, len(pixels), 4):
        pixels[index] = min(maximum, max(0.0, pixels[index]))
        pixels[index + 1] = min(maximum, max(0.0, pixels[index + 1]))
        pixels[index + 2] = min(maximum, max(0.0, pixels[index + 2]))
    return pixels


def _apply_coverage_mask(pixels: object, coverage: object) -> object:
    if len(pixels) != len(coverage) or len(pixels) % 4 != 0:
        raise RuntimeError("lightmap coverage must match the RGBA denoise output")
    for index in range(0, len(pixels), 4):
        if max(float(coverage[index]), float(coverage[index + 1]),
               float(coverage[index + 2])) <= 0.001:
            pixels[index] = 0.0
            pixels[index + 1] = 0.0
            pixels[index + 2] = 0.0
        else:
            pixels[index] = max(0.0, pixels[index])
            pixels[index + 1] = max(0.0, pixels[index + 1])
            pixels[index + 2] = max(0.0, pixels[index + 2])
        pixels[index + 3] = 1.0
    return pixels


def _denoise_lightmap(
    blender: object,
    pixels: object,
    coverage: object,
    resolution: int,
    logger: Callable[[str], None] | None = None,
) -> array:
    source_image = blender.data.images.new(
        "CEngineLightmapDenoiseSource", width=resolution, height=resolution,
        alpha=True, float_buffer=True)
    coverage_image = blender.data.images.new(
        "CEngineLightmapDenoiseCoverage", width=resolution, height=resolution,
        alpha=True, float_buffer=True)
    denoise_scene = blender.data.scenes.new("CEngineLightmapDenoise")
    camera_data = blender.data.cameras.new("CEngineLightmapDenoiseCamera")
    camera = blender.data.objects.new("CEngineLightmapDenoiseCamera", camera_data)
    compositor = None
    result_image = None
    try:
        source_image.pixels.foreach_set(pixels)
        coverage_image.pixels.foreach_set(coverage)
        denoise_scene.collection.objects.link(camera)
        denoise_scene.camera = camera
        denoise_scene.render.resolution_x = resolution
        denoise_scene.render.resolution_y = resolution
        denoise_scene.render.resolution_percentage = 100
        denoise_scene.render.film_transparent = True
        denoise_scene.render.image_settings.file_format = "OPEN_EXR"
        denoise_scene.render.image_settings.color_mode = "RGBA"
        denoise_scene.render.image_settings.color_depth = "32"
        if hasattr(denoise_scene, "use_nodes"):
            denoise_scene.use_nodes = True

        tree = getattr(denoise_scene, "node_tree", None)
        modern_compositor = tree is None
        if modern_compositor:
            compositor = blender.data.node_groups.new(
                "CEngineLightmapDenoiseCompositor", "CompositorNodeTree")
            denoise_scene.compositing_node_group = compositor
            tree = compositor
        tree.nodes.clear()
        image_node = tree.nodes.new("CompositorNodeImage")
        image_node.image = source_image
        coverage_node = tree.nodes.new("CompositorNodeImage")
        coverage_node.image = coverage_image
        denoise_node = tree.nodes.new("CompositorNodeDenoise")
        if hasattr(denoise_node, "prefilter"):
            denoise_node.prefilter = "ACCURATE"
        if hasattr(denoise_node, "quality"):
            denoise_node.quality = "HIGH"
        hdr_input = denoise_node.inputs.get("HDR")
        if hdr_input is not None:
            hdr_input.default_value = True
        despeckle_node = tree.nodes.new("CompositorNodeDespeckle")
        factor_input = despeckle_node.inputs.get("Factor")
        if factor_input is not None:
            factor_input.default_value = 1.0
        if modern_compositor:
            tree.interface.new_socket(
                name="Image", in_out="OUTPUT", socket_type="NodeSocketColor")
            output_node = tree.nodes.new("NodeGroupOutput")
        else:
            output_node = tree.nodes.new("CompositorNodeComposite")
        tree.links.new(image_node.outputs["Image"], denoise_node.inputs["Image"])
        # A white-inside/black-outside feature image gives OIDN the UV-chart
        # boundaries up front. Without it, small valid charts surrounded by
        # unused black atlas space can be denoised away to exactly black.
        tree.links.new(coverage_node.outputs["Image"], denoise_node.inputs["Albedo"])
        tree.links.new(denoise_node.outputs["Image"], despeckle_node.inputs["Image"])
        tree.links.new(despeckle_node.outputs["Image"], output_node.inputs["Image"])

        if logger is not None:
            logger("Lightmap AI denoise: OpenImageDenoise high-quality HDR pass")
        with tempfile.TemporaryDirectory(prefix="cengine-lightmap-denoise-") as temporary:
            denoise_scene.render.filepath = str(Path(temporary) / "denoised.exr")
            blender.ops.render.render(scene=denoise_scene.name, write_still=True)
            result_image = blender.data.images.load(
                denoise_scene.render.filepath, check_existing=False)
            result = array("f", [0.0]) * len(pixels)
            result_image.pixels.foreach_get(result)

        # OIDN has no UV-chart topology. Use a separately baked white coverage
        # pass to distinguish genuinely unused atlas pixels from valid surface
        # texels whose irradiance happens to be exactly black.
        return _apply_coverage_mask(result, coverage)
    finally:
        if result_image is not None:
            blender.data.images.remove(result_image)
        blender.data.objects.remove(camera, do_unlink=True)
        blender.data.cameras.remove(camera_data)
        blender.data.scenes.remove(denoise_scene)
        if compositor is not None and compositor.users == 0:
            blender.data.node_groups.remove(compositor)
        blender.data.images.remove(source_image)
        blender.data.images.remove(coverage_image)


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
            "ce_light_mode", "realtime")).lower() in INDIRECT_BAKE_LIGHT_MODES
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
    denoise = bool(getattr(scene, "get", lambda *_: DEFAULT_DENOISE)(
        "ce_lightmap_denoise", DEFAULT_DENOISE))
    samples = max(1, int(getattr(scene, "get", lambda *_: DEFAULT_SAMPLES)(
        "ce_lightmap_samples", DEFAULT_SAMPLES)))
    indirect_clamp = max(0.0, float(getattr(scene, "get", lambda *_: DEFAULT_INDIRECT_CLAMP)(
        "ce_lightmap_indirect_clamp", DEFAULT_INDIRECT_CLAMP)))
    include_direct = bool(getattr(scene, "get", lambda *_: DEFAULT_INCLUDE_DIRECT)(
        "ce_lightmap_include_direct", DEFAULT_INCLUDE_DIRECT))
    tiles = plan_atlas((target.key for target in targets), resolution, padding)
    source_meshes = list({int(target.source.as_pointer()): target.source for target in targets}.values())
    ensure_lightmap_uvs(blender, source_meshes, resolution, padding)

    output_dir = output_dir_for_source(source, output_root) / "lightmaps"
    output = output_dir / f"{clean_asset_name(scene_name)}_0.dds"
    indirect_image = blender.data.images.new(
        f"CEngine_{scene_name}_Indirect", width=resolution, height=resolution,
        alpha=True, float_buffer=True)
    direct_image = (blender.data.images.new(
        f"CEngine_{scene_name}_StaticDirect", width=resolution, height=resolution,
        alpha=True, float_buffer=True) if include_direct else None)
    coverage_image = blender.data.images.new(
        f"CEngine_{scene_name}_Coverage", width=resolution, height=resolution,
        alpha=True, float_buffer=True)
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
    previous_indirect_clamp = cycles.sample_clamp_indirect
    previous_reflective_caustics = cycles.caustics_reflective
    previous_refractive_caustics = cycles.caustics_refractive
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

    def set_light_visibility(visible_modes: frozenset[str]) -> None:
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
                  visible_modes: frozenset[str], use_color: bool = False) -> None:
        target_image(image)
        set_light_visibility(visible_modes)
        scene.render.bake.use_pass_color = use_color
        scene.render.bake.use_pass_direct = use_direct
        scene.render.bake.use_pass_indirect = use_indirect
        for index, target in enumerate(temporary_objects):
            _select_only(blender, target)
            scene.render.bake.use_clear = index == 0
            blender.ops.object.bake(type="DIFFUSE", margin=padding, use_selected_to_active=False)

    try:
        scene.render.engine = "CYCLES"
        # Lightmap convergence is an offline-cook contract, independent from
        # the scene's interactive render sample count. Clamp rare, extremely
        # energetic indirect paths before OIDN so isolated firefly texels do
        # not survive in otherwise dark atlas regions.
        cycles.samples = samples
        cycles.use_adaptive_sampling = True
        cycles.adaptive_threshold = 0.01
        cycles.max_bounces = 4
        cycles.diffuse_bounces = 2
        cycles.glossy_bounces = 1
        cycles.transparent_max_bounces = 8
        cycles.sample_clamp_indirect = indirect_clamp
        cycles.caustics_reflective = False
        cycles.caustics_refractive = False
        scene.render.bake.use_clear = True
        if logger is not None:
            logger(
                f"Lightmap Cycles quality: {samples} samples, "
                f"indirect clamp={indirect_clamp:g}, direct={'on' if include_direct else 'off'}, "
                "diffuse caustics disabled")
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

        # Mixed lights contribute bounced GI here while retaining realtime
        # direct light and shadows on both static and dynamic surfaces.
        bake_pass(indirect_image, use_direct=False, use_indirect=True,
                  visible_modes=INDIRECT_BAKE_LIGHT_MODES)
        indirect_pixels = indirect_image.pixels[:]
        direct_pixels = None
        if include_direct:
            # Only fully baked lights contribute direct light and baked shadows.
            bake_pass(direct_image, use_direct=True, use_indirect=False,
                      visible_modes=DIRECT_BAKE_LIGHT_MODES)
            direct_pixels = direct_image.pixels[:]

        # Bake atlas coverage after lighting, using white diffuse color while
        # retaining each copied material's authored alpha graph. This provides
        # an exact mask for denoising without mutating source materials.
        for material in temporary_materials:
            for node in material.node_tree.nodes:
                if getattr(node, "type", "") != "BSDF_PRINCIPLED":
                    continue
                base_color = node.inputs.get("Base Color")
                if base_color is None:
                    continue
                for link in tuple(base_color.links):
                    material.node_tree.links.remove(link)
                base_color.default_value = (1.0, 1.0, 1.0, 1.0)
        bake_pass(coverage_image, use_direct=False, use_indirect=False,
                  visible_modes=frozenset(), use_color=True)
        coverage_pixels = coverage_image.pixels[:]
        combined_pixels = (_combine_light_passes(indirect_pixels, direct_pixels)
                           if direct_pixels is not None else list(indirect_pixels))
        indirect_max = _maximum_rgb(indirect_pixels)
        direct_max = _maximum_rgb(direct_pixels) if direct_pixels is not None else 0.0
        raw_combined_max = _maximum_rgb(combined_pixels)
        if denoise:
            combined_pixels = _denoise_lightmap(
                blender, combined_pixels, coverage_pixels, resolution, logger)
            combined_pixels = _clamp_rgb(combined_pixels, raw_combined_max)
        combined_max = _maximum_rgb(combined_pixels)
        if logger is not None:
            logger(
                f"Lightmap bake energy: indirect={indirect_max:.6g}, "
                f"static-direct={direct_max:.6g}, combined={raw_combined_max:.6g}, "
                f"output={combined_max:.6g}")
        if combined_max <= MINIMUM_LIGHTMAP_ENERGY:
            raise RuntimeError(
                "Cycles produced an empty lightmap; check light modes, render visibility, "
                "light linking, and overlapping prototype/instance geometry")
        _save_hdr(combined_pixels, resolution, output)
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
        if direct_image is not None:
            blender.data.images.remove(direct_image)
        blender.data.images.remove(coverage_image)
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
        cycles.sample_clamp_indirect = previous_indirect_clamp
        cycles.caustics_reflective = previous_reflective_caustics
        cycles.caustics_refractive = previous_refractive_caustics
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
