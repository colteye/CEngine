#   _____ ______             _
#  / ____|  ____|           (_)
# | |    | |__   _ __   __ _ _ _ __   ___
# | |    |  __| | '_ \ / _` | | '_ \ / _ \
# | |____| |____| | | | (_| | | | | |  __/
#  \_____|______|_| |_|\__, |_|_| |_|\___|
#                       __/ |
#                      |___/

"""TODO: Briefly describe this module.

Author:
    Erik Coltey
"""

import bpy
import math
import os


# =====================================================
# SETTINGS
# =====================================================

SIZE = 4096
SAMPLES = 256
BAKE_MARGIN = 64
LIGHTMAP_UV_NAME = "Lightmap"
LIGHTMAP_UV_MARGIN = 0.001
NORMAL_SEAM_DOT_THRESHOLD = 0.9999
GEOMETRY_SEAM_ANGLE_DEGREES = 140.0
ENABLE_WARP_FALLBACK = True
# Local triangle-shape distortion. Keep this permissive to avoid replacing
# otherwise valid islands merely because they contain elongated geometry.
MAX_UV_STRETCH_RATIO = 64.0
# Per-island texel-density variation. This catches charts such as a vase where
# individual faces look valid but different sections are scaled inconsistently.
MAX_TEXEL_DENSITY_RATIO = 4.0
TEXEL_DENSITY_TRIM_FRACTION = 0.10
MIN_TEXEL_DENSITY_SAMPLES = 10
FALLBACK_SMART_PROJECT_ANGLE_DEGREES = 66.0
UV_EPSILON = 1.0e-7

# Bake indirect light by default. Set True when the lightmap should also
# contain direct light and shadows.
INCLUDE_DIRECT_LIGHTING = False


RAW_NAME = "LightBake_Raw"
COMPOSITOR_NAME = "Lightmap_Compositor"

FINAL_PNG_NAME = "LightBake_Final.png"


# =====================================================
# VALIDATE
# =====================================================

obj = bpy.context.active_object

if obj is None or obj.type != 'MESH':
    raise RuntimeError("Select a mesh object first.")

if not bpy.data.filepath:
    raise RuntimeError("Save the .blend file before baking.")

scene = bpy.context.scene
scene.render.engine = 'CYCLES'

folder = os.path.dirname(bpy.data.filepath)


# =====================================================
# CYCLES SETTINGS
# =====================================================

scene.cycles.samples = SAMPLES
scene.cycles.use_adaptive_sampling = True
scene.cycles.adaptive_threshold = 0.01

scene.cycles.max_bounces = 4
scene.cycles.diffuse_bounces = 2
scene.cycles.glossy_bounces = 1
scene.cycles.transparent_max_bounces = 8


# =====================================================
# LIGHTMAP SEAM HELPERS
# =====================================================

def edge_has_split_normals(mesh, edge_index, edge_face_loops):
    """Return True when an edge is a visible hard-normal boundary."""
    face_loops = edge_face_loops[edge_index]

    # Boundary and non-manifold edges should always become UV seams.
    if len(face_loops) != 2:
        return True

    first_face_index, first_loops = face_loops[0]
    second_face_index, second_loops = face_loops[1]

    # Also split at extremely sharp geometric folds. This mirrors a 140-degree
    # Select Sharp Edges test without relying on Blender's selection operator.
    geometry_seam_dot = math.cos(
        math.radians(GEOMETRY_SEAM_ANGLE_DEGREES)
    )
    if (
        mesh.polygons[first_face_index].normal.dot(
            mesh.polygons[second_face_index].normal
        ) < geometry_seam_dot
    ):
        return True

    first_by_vertex = {
        mesh.loops[loop_index].vertex_index: loop_index
        for loop_index in first_loops
    }
    second_by_vertex = {
        mesh.loops[loop_index].vertex_index: loop_index
        for loop_index in second_loops
    }

    for vertex_index in first_by_vertex.keys() & second_by_vertex.keys():
        first_normal = mesh.corner_normals[first_by_vertex[vertex_index]].vector
        second_normal = mesh.corner_normals[second_by_vertex[vertex_index]].vector

        if first_normal.dot(second_normal) < NORMAL_SEAM_DOT_THRESHOLD:
            return True

    return False


def mark_visual_seams(mesh):
    """Temporarily add seams at authored sharp and split-normal boundaries."""
    original_seams = [edge.use_seam for edge in mesh.edges]
    sharp_edges = mesh.attributes.get("sharp_edge")
    edge_face_loops = [[] for _ in mesh.edges]

    for polygon in mesh.polygons:
        loop_indices = list(polygon.loop_indices)

        for offset, loop_index in enumerate(loop_indices):
            next_loop_index = loop_indices[(offset + 1) % len(loop_indices)]
            edge_index = mesh.loops[loop_index].edge_index
            edge_face_loops[edge_index].append(
                (polygon.index, (loop_index, next_loop_index))
            )

    for edge in mesh.edges:
        explicitly_sharp = (
            sharp_edges is not None
            and sharp_edges.domain == 'EDGE'
            and sharp_edges.data[edge.index].value
        )
        edge.use_seam = (
            original_seams[edge.index]
            or explicitly_sharp
            or edge_has_split_normals(mesh, edge.index, edge_face_loops)
        )

    return original_seams


def restore_seams(mesh, original_seams):
    """TODO: Describe `restore_seams`.

    Args:
        mesh: TODO: Describe this parameter.
        original_seams: TODO: Describe this parameter.
    """
    for edge, use_seam in zip(mesh.edges, original_seams):
        edge.use_seam = use_seam


def uv_coordinates_match(first_uv, second_uv):
    """TODO: Describe `uv_coordinates_match`.

    Args:
        first_uv: TODO: Describe this parameter.
        second_uv: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    return (
        abs(first_uv.x - second_uv.x) <= UV_EPSILON
        and abs(first_uv.y - second_uv.y) <= UV_EPSILON
    )


def get_uv_islands(mesh, uv_layer):
    """Return polygon-index sets joined only across continuous UV edges."""
    parents = list(range(len(mesh.polygons)))
    edge_faces = [[] for _ in mesh.edges]

    def find(index):
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

    def union(first, second):
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
            edge_index = mesh.loops[loop_index].edge_index
            edge_faces[edge_index].append(
                (polygon.index, (loop_index, next_loop_index))
            )

    for edge_entries in edge_faces:
        if len(edge_entries) != 2:
            continue

        first_polygon, first_loops = edge_entries[0]
        second_polygon, second_loops = edge_entries[1]
        first_by_vertex = {
            mesh.loops[loop_index].vertex_index: uv_layer.data[loop_index].uv
            for loop_index in first_loops
        }
        second_by_vertex = {
            mesh.loops[loop_index].vertex_index: uv_layer.data[loop_index].uv
            for loop_index in second_loops
        }

        if first_by_vertex.keys() != second_by_vertex.keys():
            continue

        if all(
            uv_coordinates_match(first_by_vertex[vertex], second_by_vertex[vertex])
            for vertex in first_by_vertex
        ):
            union(first_polygon, second_polygon)

    islands = {}

    for polygon in mesh.polygons:
        islands.setdefault(find(polygon.index), []).append(polygon.index)

    return list(islands.values())


def triangle_aspect_ratio(points):
    """TODO: Describe `triangle_aspect_ratio`.

    Args:
        points: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    lengths = [
        (points[1] - points[0]).length,
        (points[2] - points[1]).length,
        (points[0] - points[2]).length,
    ]
    shortest = min(lengths)

    if shortest <= UV_EPSILON:
        return float("inf")

    return max(lengths) / shortest


def island_needs_fallback(mesh, uv_layer, polygon_indices):
    """Detect degenerate or disproportionately stretched UV triangles."""
    texel_densities = []

    for polygon_index in polygon_indices:
        loop_indices = list(mesh.polygons[polygon_index].loop_indices)

        for offset in range(1, len(loop_indices) - 1):
            triangle_loops = (
                loop_indices[0],
                loop_indices[offset],
                loop_indices[offset + 1],
            )
            world_points = [
                mesh.vertices[mesh.loops[loop_index].vertex_index].co
                for loop_index in triangle_loops
            ]
            uv_points = [uv_layer.data[loop_index].uv for loop_index in triangle_loops]

            world_area = (
                (world_points[1] - world_points[0]).cross(
                    world_points[2] - world_points[0]
                ).length
                * 0.5
            )
            uv_area = abs(
                (uv_points[1].x - uv_points[0].x)
                * (uv_points[2].y - uv_points[0].y)
                - (uv_points[1].y - uv_points[0].y)
                * (uv_points[2].x - uv_points[0].x)
            ) * 0.5

            # Ignore collapsed triangles here. They can occur in imported UVs
            # and are handled by the later unwrap/pack stage; this fallback is
            # intentionally driven only by measurable stretch.
            if world_area <= UV_EPSILON or uv_area <= UV_EPSILON:
                continue

            world_aspect = triangle_aspect_ratio(world_points)
            uv_aspect = triangle_aspect_ratio(uv_points)
            stretch = max(
                uv_aspect / world_aspect,
                world_aspect / uv_aspect,
            )

            if stretch > MAX_UV_STRETCH_RATIO:
                return True

            texel_densities.append(uv_area / world_area)

    # Raw min/max is dominated by isolated tiny or highly curved triangles.
    # Require enough samples, then discard both tails before comparing density.
    if len(texel_densities) < MIN_TEXEL_DENSITY_SAMPLES:
        return False

    texel_densities.sort()
    trim_count = max(
        1,
        int(len(texel_densities) * TEXEL_DENSITY_TRIM_FRACTION),
    )
    trimmed_densities = texel_densities[
        trim_count:len(texel_densities) - trim_count
    ]

    if len(trimmed_densities) < 2:
        return False

    return (
        trimmed_densities[-1] / trimmed_densities[0]
        > MAX_TEXEL_DENSITY_RATIO
    )


def smart_project_bad_islands(mesh):
    # Reacquire this RNA collection after leaving Edit Mode. UV-layer objects
    # captured before an unwrap can become invalid even though their names stay
    # visible in Blender's UI.
    """TODO: Describe `smart_project_bad_islands`.

    Args:
        mesh: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    uv_layer = mesh.uv_layers.get(LIGHTMAP_UV_NAME)

    if uv_layer is None:
        raise RuntimeError("The generated Lightmap UV layer is missing.")

    bad_polygons = set()

    for island in get_uv_islands(mesh, uv_layer):
        if island_needs_fallback(mesh, uv_layer, island):
            bad_polygons.update(island)

    if not bad_polygons:
        return 0

    for polygon in mesh.polygons:
        polygon.select = polygon.index in bad_polygons

    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.uv.smart_project(
        angle_limit=math.radians(FALLBACK_SMART_PROJECT_ANGLE_DEGREES),
        island_margin=0.0,
        area_weight=0.0,
        correct_aspect=True,
        scale_to_bounds=False,
    )
    bpy.ops.object.mode_set(mode='OBJECT')

    return len(bad_polygons)


# =====================================================
# COPY UV0, UNWRAP, AND PACK A LIGHTMAP UV
# =====================================================

mesh = obj.data

if not mesh.uv_layers:
    raise RuntimeError("The selected mesh has no UV0 layer to repack.")

# Capture UV0 before creating or replacing the dedicated lightmap layer. This
# keeps the artist-authored seam layout while ensuring UV0 is never modified.
uv0 = mesh.uv_layers[0]
uv0_coordinates = [0.0] * (len(uv0.data) * 2)
uv0.data.foreach_get("uv", uv0_coordinates)

lightmap_uv = mesh.uv_layers.get(LIGHTMAP_UV_NAME)

# If UV0 itself is called "Lightmap", recreate that layer after copying its
# coordinates so packing cannot alter the source layout.
if lightmap_uv is uv0:
    mesh.uv_layers.remove(lightmap_uv)
    lightmap_uv = None

if lightmap_uv is None:
    lightmap_uv = mesh.uv_layers.new(name=LIGHTMAP_UV_NAME)

lightmap_uv.data.foreach_set("uv", uv0_coordinates)
mesh.update()

mesh.uv_layers.active = lightmap_uv
bpy.context.view_layer.objects.active = obj

if obj.mode != 'OBJECT':
    bpy.ops.object.mode_set(mode='OBJECT')

original_seams = mark_visual_seams(mesh)

bpy.ops.object.mode_set(mode='EDIT')
bpy.ops.mesh.select_all(action='SELECT')

# Unwrap using authored sharp edges and the actual split-normal boundaries that
# determine the mesh's visible hard shading.
bpy.ops.uv.unwrap(method='ANGLE_BASED')

bpy.ops.object.mode_set(mode='OBJECT')
restore_seams(mesh, original_seams)

if ENABLE_WARP_FALLBACK:
    fallback_face_count = smart_project_bad_islands(mesh)
    print(f"Smart-projected {fallback_face_count} warped lightmap faces.")

bpy.ops.object.mode_set(mode='EDIT')
bpy.ops.mesh.select_all(action='SELECT')

# Normalize texel density before tightly packing unique lightmap islands.
bpy.ops.uv.average_islands_scale()

bpy.ops.uv.pack_islands(
    rotate=True,
    scale=True,
    merge_overlap=False,
    margin=LIGHTMAP_UV_MARGIN
)

bpy.ops.object.mode_set(mode='OBJECT')


# =====================================================
# IMAGE HELPERS
# =====================================================

def remove_image(name):
    """TODO: Describe `remove_image`.

    Args:
        name: TODO: Describe this parameter.
    """
    image = bpy.data.images.get(name)

    if image:
        bpy.data.images.remove(image)


def make_float_image(name):
    """TODO: Describe `make_float_image`.

    Args:
        name: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    remove_image(name)

    return bpy.data.images.new(
        name=name,
        width=SIZE,
        height=SIZE,
        alpha=False,
        float_buffer=True
    )


raw_img = make_float_image(RAW_NAME)


# =====================================================
# ASSIGN BAKE TARGET
# =====================================================

def set_bake_target(image):
    """TODO: Describe `set_bake_target`.

    Args:
        image: TODO: Describe this parameter.
    """
    for slot in obj.material_slots:
        material = slot.material

        if material is None:
            continue

        if not material.use_nodes:
            material.use_nodes = True

        material_tree = material.node_tree
        old_node = material_tree.nodes.get("LightmapBakeTarget")

        if old_node:
            material_tree.nodes.remove(old_node)

        bake_node = material_tree.nodes.new("ShaderNodeTexImage")
        bake_node.name = "LightmapBakeTarget"
        bake_node.label = "Lightmap Bake Target"
        bake_node.image = image

        for node in material_tree.nodes:
            node.select = False

        bake_node.select = True
        material_tree.nodes.active = bake_node


def set_lightmap_target_image(image):
    """Point every generated bake target at the final lightmap image."""
    for slot in obj.material_slots:
        material = slot.material

        if material is None or not material.use_nodes:
            continue

        bake_node = material.node_tree.nodes.get("LightmapBakeTarget")

        if bake_node is not None:
            bake_node.image = image


set_bake_target(raw_img)


# =====================================================
# BAKE COMBINED DIFFUSE LIGHTING
# =====================================================

scene.cycles.bake_type = 'DIFFUSE'

bake = scene.render.bake
bake.use_selected_to_active = False
bake.use_clear = True
bake.margin = BAKE_MARGIN

# Lighting-only lightmap.
bake.use_pass_color = False
bake.use_pass_direct = INCLUDE_DIRECT_LIGHTING
bake.use_pass_indirect = True

bpy.ops.object.select_all(action='DESELECT')
obj.select_set(True)
bpy.context.view_layer.objects.active = obj

print(
    "Baking diffuse lighting "
    f"(direct={'on' if INCLUDE_DIRECT_LIGHTING else 'off'}, indirect=on)..."
)

bpy.ops.object.bake(type='DIFFUSE')

# =====================================================
# DENOISE: BLENDER 5.x COMPOSITOR
# =====================================================

print("Denoising lightmap...")

denoised_exr_path = os.path.join(folder, "LightBake_Denoised.exr")

tree = scene.compositing_node_group

if tree is None:
    tree = bpy.data.node_groups.new(
        name=COMPOSITOR_NAME,
        type="CompositorNodeTree"
    )
    scene.compositing_node_group = tree

tree.nodes.clear()
tree.interface.clear()

tree.interface.new_socket(
    name="Image",
    in_out='OUTPUT',
    socket_type="NodeSocketColor"
)

image_node = tree.nodes.new("CompositorNodeImage")
image_node.image = raw_img

denoise_node = tree.nodes.new("CompositorNodeDenoise")

group_output = tree.nodes.new("NodeGroupOutput")

despeckle_node = tree.nodes.new("CompositorNodeDespeckle")

# 0.0 disables the effect; 1.0 is the strongest setting.
despeckle_node.inputs["Factor"].default_value = 1.0

tree.links.new(
    image_node.outputs["Image"],
    denoise_node.inputs["Image"]
)

tree.links.new(
    denoise_node.outputs["Image"],
    despeckle_node.inputs["Image"]
)

tree.links.new(
    despeckle_node.outputs["Image"],
    group_output.inputs["Image"]
)


# Write the compositor's final Group Output as a 32-bit EXR.
scene.render.image_settings.file_format = 'OPEN_EXR'
scene.render.image_settings.color_mode = 'RGB'
scene.render.image_settings.color_depth = '32'
scene.render.filepath = denoised_exr_path

scene.render.resolution_x = SIZE
scene.render.resolution_y = SIZE
scene.render.resolution_percentage = 100

bpy.ops.render.render(write_still=True)

if not os.path.exists(denoised_exr_path):
    raise RuntimeError(
        "Denoised EXR was not created:\n" + denoised_exr_path
    )


# =====================================================
# EXPORT 16-BIT PNG
# =====================================================

denoised_img = bpy.data.images.load(
    denoised_exr_path,
    check_existing=False
)

# Keep the generated Image Texture nodes useful after the bake: they now show
# the final, denoised lightmap instead of the raw intermediate image.
set_lightmap_target_image(denoised_img)

png_path = os.path.join(folder, FINAL_PNG_NAME)

scene.render.image_settings.file_format = 'PNG'
scene.render.image_settings.color_mode = 'RGB'
scene.render.image_settings.color_depth = '16'

denoised_img.save_render(png_path, scene=scene)

print("--------------------------------")
print("DONE")
print("Denoised EXR: ", denoised_exr_path)
print("Final PNG:    ", png_path)
print("--------------------------------")
