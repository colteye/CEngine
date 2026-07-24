"""Blender-native physics-mesh generation for CEngine entities."""

from __future__ import annotations

import math
from dataclasses import dataclass

try:
    import bmesh
    import bpy
    from mathutils import Matrix
except ImportError:
    bmesh = None
    bpy = None
    Matrix = None


AUTO_PHYSICS_OWNER = "ce_auto_physics_owner"
COLLISION_HELPER = "ce_collision_helper"
MAX_HULL_INPUT_VERTICES = 512
MAX_HULL_OUTPUT_VERTICES = 256
MAX_CLUSTER_TRIANGLES = 100_000


@dataclass(frozen=True)
class PhysicsMeshSettings:
    """Controls native convex collision generation."""

    concave: bool = False
    accuracy: float = 0.75
    max_hulls: int = 16


@dataclass(frozen=True)
class HullMesh:
    """One generated convex hull."""

    vertices: tuple[tuple[float, float, float], ...]
    faces: tuple[tuple[int, ...], ...]
    interior_fraction: float


def _triangle_centroid(
    vertices: list[tuple[float, float, float]],
    triangle: tuple[int, int, int],
) -> tuple[float, float, float]:
    return tuple(
        sum(vertices[index][axis] for index in triangle) / 3.0
        for axis in range(3)
    )


def connected_triangle_components(
    triangles: list[tuple[int, int, int]],
) -> list[list[int]]:
    """Group triangles sharing vertices with a memory-bounded union-find."""
    parents = list(range(len(triangles)))

    def root(index: int) -> int:
        while parents[index] != index:
            parents[index] = parents[parents[index]]
            index = parents[index]
        return index

    def join(first: int, second: int) -> None:
        first_root = root(first)
        second_root = root(second)
        if first_root != second_root:
            parents[max(first_root, second_root)] = min(
                first_root, second_root)

    first_triangle_by_vertex: dict[int, int] = {}
    for triangle_index, triangle in enumerate(triangles):
        for vertex in triangle:
            previous = first_triangle_by_vertex.setdefault(
                vertex, triangle_index)
            join(triangle_index, previous)
    components: dict[int, list[int]] = {}
    for triangle_index in range(len(triangles)):
        components.setdefault(root(triangle_index), []).append(triangle_index)
    return sorted(components.values(), key=lambda component: component[0])


def coalesce_triangle_components(
    vertices: list[tuple[float, float, float]],
    triangles: list[tuple[int, int, int]],
    components: list[list[int]],
    maximum: int,
) -> list[list[int]]:
    """Spatially group loose pieces when they exceed the hull budget."""
    if len(components) <= maximum:
        return components
    centers: list[tuple[float, float, float]] = []
    for component in components:
        total = [0.0, 0.0, 0.0]
        for index in component:
            centroid = _triangle_centroid(vertices, triangles[index])
            for axis in range(3):
                total[axis] += centroid[axis]
        inverse_count = 1.0 / float(len(component))
        centers.append(tuple(
            component * inverse_count for component in total))
    bounds = [
        (
            min(center[axis] for center in centers),
            max(center[axis] for center in centers),
        )
        for axis in range(3)
    ]
    axis = max(
        range(3), key=lambda value: bounds[value][1] - bounds[value][0])
    ordered = sorted(
        zip(centers, components),
        key=lambda item: (item[0][axis], item[0], item[1][0]),
    )
    grouped = [[] for _ in range(maximum)]
    for index, (_, component) in enumerate(ordered):
        group = min(index * maximum // len(ordered), maximum - 1)
        grouped[group].extend(component)
    return [sorted(group) for group in grouped if group]


def split_triangle_cluster(
    vertices: list[tuple[float, float, float]],
    triangles: list[tuple[int, int, int]],
    cluster: list[int],
) -> tuple[list[int], list[int]] | None:
    """Split a cluster at the median centroid on its longest axis."""
    centroids = [
        (index, _triangle_centroid(vertices, triangles[index]))
        for index in cluster
    ]
    bounds = [
        (
            min(centroid[axis] for _, centroid in centroids),
            max(centroid[axis] for _, centroid in centroids),
        )
        for axis in range(3)
    ]
    axis = max(
        range(3), key=lambda value: bounds[value][1] - bounds[value][0])
    ordered = sorted(
        centroids, key=lambda item: (item[1][axis], item[0]))
    midpoint = len(ordered) // 2
    if midpoint == 0 or midpoint == len(ordered):
        return None
    left = sorted(index for index, _ in ordered[:midpoint])
    right = sorted(index for index, _ in ordered[midpoint:])
    if not left or not right:
        return None
    return left, right


def _evaluated_triangles(
    source: object, depsgraph: object
) -> tuple[
    list[tuple[float, float, float]],
    list[tuple[int, int, int]],
]:
    """Read evaluated geometry and bake object scale into local vertices."""
    scale = tuple(float(value) for value in source.scale)
    if len(scale) != 3 or any(
            not math.isfinite(value) or value <= 0.0 for value in scale):
        raise ValueError(
            "physics mesh generation requires finite, positive object scale")
    evaluated = source.evaluated_get(depsgraph)
    mesh = evaluated.to_mesh(
        preserve_all_data_layers=False, depsgraph=depsgraph)
    try:
        # Blender's loop-triangle cache triangulates concave n-gons without
        # modifying or duplicating the potentially very large source mesh.
        mesh.calc_loop_triangles()
        vertices = [
            (
                float(vertex.co.x) * scale[0],
                float(vertex.co.y) * scale[1],
                float(vertex.co.z) * scale[2],
            )
            for vertex in mesh.vertices
        ]
        triangles = [
            tuple(int(value) for value in triangle.vertices)
            for triangle in mesh.loop_triangles
        ]
    finally:
        evaluated.to_mesh_clear()
    if len(vertices) < 4 or len(triangles) < 4:
        raise ValueError(
            "physics mesh generation requires at least four vertices "
            "and four triangles")
    return vertices, triangles


def _support_directions(count: int) -> list[tuple[float, float, float]]:
    """Return deterministic, approximately uniform sphere directions."""
    directions = [
        (1.0, 0.0, 0.0), (-1.0, 0.0, 0.0),
        (0.0, 1.0, 0.0), (0.0, -1.0, 0.0),
        (0.0, 0.0, 1.0), (0.0, 0.0, -1.0),
    ]
    golden_angle = math.pi * (3.0 - math.sqrt(5.0))
    remaining = max(count - len(directions), 0)
    for index in range(remaining):
        z = 1.0 - (2.0 * (index + 0.5) / remaining)
        radius = math.sqrt(max(1.0 - z * z, 0.0))
        angle = index * golden_angle
        directions.append((
            radius * math.cos(angle),
            radius * math.sin(angle),
            z,
        ))
    return directions[:count]


def key_vertex_indices(
    vertices: list[tuple[float, float, float]],
    source_indices: list[int],
    maximum: int = MAX_HULL_INPUT_VERTICES,
) -> list[int]:
    """Select bounded interior samples and outer support points."""
    if len(source_indices) <= maximum:
        return source_indices
    ordered = sorted(source_indices)
    uniform_count = maximum // 2
    if uniform_count == 1:
        uniform = [ordered[len(ordered) // 2]]
    else:
        uniform = [
            ordered[round(index * (len(ordered) - 1) /
                          (uniform_count - 1))]
            for index in range(uniform_count)
        ]
    try:
        import numpy as np
    except ImportError:
        # Blender bundles NumPy, but retain a bounded pure-Python fallback for
        # stripped installations and converter-library unit tests.
        extrema = {
            min(ordered, key=lambda index: vertices[index][axis])
            for axis in range(3)
        } | {
            max(ordered, key=lambda index: vertices[index][axis])
            for axis in range(3)
        }
        return list(dict.fromkeys((*uniform, *sorted(extrema))))[:maximum]

    points = np.asarray(
        [vertices[index] for index in ordered], dtype=np.float64)
    support: set[int] = set()
    directions = _support_directions(maximum - uniform_count)
    # Small direction batches prevent a million-vertex source from creating
    # one giant temporary dot-product matrix.
    for first in range(0, len(directions), 8):
        batch = np.asarray(directions[first:first + 8], dtype=np.float64)
        extrema = np.argmax(points @ batch.T, axis=0)
        support.update(ordered[int(index)] for index in extrema)
    selected = list(dict.fromkeys((*uniform, *sorted(support))))
    return selected[:maximum]


def _hull_from_points(
    points: list[tuple[float, float, float]],
) -> tuple[
    tuple[tuple[float, float, float], ...],
    tuple[tuple[int, ...], ...],
    int,
]:
    """Build one Blender hull and return geometry plus used input count."""
    mesh = bmesh.new()
    try:
        input_vertices = [mesh.verts.new(point) for point in points]
        mesh.verts.ensure_lookup_table()
        bmesh.ops.convex_hull(
            mesh, input=input_vertices, use_existing_faces=False)
        mesh.verts.ensure_lookup_table()
        mesh.faces.ensure_lookup_table()
        used = {vertex for face in mesh.faces for vertex in face.verts}
        if len(used) < 4 or len(mesh.faces) < 4:
            raise ValueError(
                "a physics hull is degenerate; "
                "the source needs non-coplanar volume")
        ordered = sorted(used, key=lambda vertex: vertex.index)
        remap = {vertex: index for index, vertex in enumerate(ordered)}
        result_vertices = tuple(
            tuple(float(value) for value in vertex.co)
            for vertex in ordered
        )
        result_faces = tuple(
            tuple(remap[vertex] for vertex in face.verts)
            for face in mesh.faces
        )
        used_count = len(used)
    finally:
        mesh.free()
    return result_vertices, result_faces, used_count


def _convex_hull(
    vertices: list[tuple[float, float, float]],
    triangles: list[tuple[int, int, int]],
    cluster: list[int],
) -> HullMesh:
    """Build one hull with Blender and report discarded input vertices."""
    if bmesh is None:
        raise RuntimeError("convex hull generation must run inside Blender")
    source_indices = sorted({
        vertex for triangle_index in cluster
        for vertex in triangles[triangle_index]
    })
    key_indices = key_vertex_indices(vertices, source_indices)
    input_points = [vertices[index] for index in key_indices]
    hull_vertices, hull_faces, used_count = _hull_from_points(input_points)
    interior_fraction = 1.0 - (used_count / len(key_indices))
    if len(hull_vertices) > MAX_HULL_OUTPUT_VERTICES:
        reduced = key_vertex_indices(
            list(hull_vertices),
            list(range(len(hull_vertices))),
            MAX_HULL_OUTPUT_VERTICES,
        )
        hull_vertices, hull_faces, _ = _hull_from_points(
            [hull_vertices[index] for index in reduced])
    hull = HullMesh(
        vertices=hull_vertices,
        faces=hull_faces,
        interior_fraction=interior_fraction,
    )
    return hull


def generate_hulls(
    vertices: list[tuple[float, float, float]],
    triangles: list[tuple[int, int, int]],
    settings: PhysicsMeshSettings,
) -> list[HullMesh]:
    """Generate one hull or a bounded set of hulls for concave input."""
    if not 1 <= settings.max_hulls <= 64:
        raise ValueError("maximum hulls must be between 1 and 64")
    accuracy = min(max(float(settings.accuracy), 0.0), 1.0)
    if not settings.concave:
        all_triangles = list(range(len(triangles)))
        return [_convex_hull(vertices, triangles, all_triangles)]

    clusters = coalesce_triangle_components(
        vertices,
        triangles,
        connected_triangle_components(triangles),
        settings.max_hulls,
    )
    # Pre-split huge connected regions before asking Blender to build any
    # hull. This bounds each repeated hull workspace independently of render
    # mesh size.
    while len(clusters) < settings.max_hulls:
        candidate = max(
            range(len(clusters)), key=lambda index: len(clusters[index]))
        if len(clusters[candidate]) <= MAX_CLUSTER_TRIANGLES:
            break
        split = split_triangle_cluster(
            vertices, triangles, clusters[candidate])
        if split is None:
            break
        clusters[candidate:candidate + 1] = [split[0], split[1]]
    hulls = [_convex_hull(vertices, triangles, cluster)
             for cluster in clusters]

    # Higher accuracy tolerates fewer vertices being swallowed by a hull.
    tolerated_interior = math.pow(1.0 - accuracy, 2.0)
    while len(clusters) < settings.max_hulls:
        candidate = max(
            range(len(clusters)),
            key=lambda index: (
                hulls[index].interior_fraction,
                len(clusters[index]),
                -index,
            ),
        )
        if hulls[candidate].interior_fraction <= tolerated_interior or \
                len(clusters[candidate]) < 8:
            break
        split = split_triangle_cluster(
            vertices, triangles, clusters[candidate])
        if split is None:
            break
        replacement_clusters = [split[0], split[1]]
        replacement_hulls = [
            _convex_hull(vertices, triangles, cluster)
            for cluster in replacement_clusters
        ]
        clusters[candidate:candidate + 1] = replacement_clusters
        hulls[candidate:candidate + 1] = replacement_hulls
    return hulls


def _remove_generated_helper(source: object) -> None:
    """Remove only a previous helper generated for this source."""
    helper_name = str(source.get("ce_collision_object", "") or "")
    helper = bpy.data.objects.get(helper_name)
    if helper is None or helper.get(AUTO_PHYSICS_OWNER, "") != source.name:
        return
    for child in tuple(helper.children):
        mesh = child.data if child.type == "MESH" else None
        bpy.data.objects.remove(child, do_unlink=True)
        if mesh is not None and mesh.users == 0:
            bpy.data.meshes.remove(mesh)
    mesh = helper.data if helper.type == "MESH" else None
    bpy.data.objects.remove(helper, do_unlink=True)
    if mesh is not None and mesh.users == 0:
        bpy.data.meshes.remove(mesh)


def _parent_identity(child: object, parent: object) -> None:
    child.parent = parent
    child.matrix_parent_inverse = Matrix.Identity(4)
    child.location = (0.0, 0.0, 0.0)
    child.rotation_mode = "QUATERNION"
    child.rotation_quaternion = (1.0, 0.0, 0.0, 0.0)
    child.scale = (1.0, 1.0, 1.0)


def _helper_name(source: object) -> str:
    cleaned = "".join(
        character if character.isalnum() or character in "._-" else "_"
        for character in str(source.name)
    ).strip("._-")
    return f"COL_{cleaned or 'Mesh'}"


def _new_hull_object(
    context: object,
    name: str,
    hull: HullMesh,
    source: object,
) -> object:
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(hull.vertices, [], hull.faces)
    mesh.update()
    result = bpy.data.objects.new(name, mesh)
    collection = source.users_collection[0] \
        if source.users_collection else context.collection
    collection.objects.link(result)
    result[COLLISION_HELPER] = True
    result[AUTO_PHYSICS_OWNER] = source.name
    result["ce_collider"] = "convex_hull"
    result.display_type = "SOLID"
    result.show_wire = True
    result.show_all_edges = True
    result.show_in_front = True
    result.hide_render = True
    return result


def generate_physics_mesh(
    context: object,
    source: object,
    settings: PhysicsMeshSettings,
) -> list[object]:
    """Generate editable collision helpers and bind them to an entity."""
    if bpy is None or bmesh is None or Matrix is None:
        raise RuntimeError("physics mesh generation must run inside Blender")
    if source is None or source.type != "MESH":
        raise ValueError("select a mesh entity before generating physics")
    vertices, triangles = _evaluated_triangles(
        source, context.evaluated_depsgraph_get())
    hulls = generate_hulls(vertices, triangles, settings)
    _remove_generated_helper(source)

    base_name = _helper_name(source)
    generated: list[object] = []
    if len(hulls) == 1:
        root = _new_hull_object(
            context, base_name, hulls[0], source)
        _parent_identity(root, source)
        generated.append(root)
        source["ce_collider"] = "convex_hull"
    else:
        root_mesh = bpy.data.meshes.new(f"{base_name}_Root")
        root = bpy.data.objects.new(base_name, root_mesh)
        collection = source.users_collection[0] \
            if source.users_collection else context.collection
        collection.objects.link(root)
        _parent_identity(root, source)
        root[COLLISION_HELPER] = True
        root[AUTO_PHYSICS_OWNER] = source.name
        root["ce_collider"] = "compound"
        root.display_type = "WIRE"
        root.show_in_front = True
        root.hide_render = True
        for index, hull in enumerate(hulls):
            child = _new_hull_object(
                context, f"{root.name}_Hull_{index:02d}",
                hull, source)
            _parent_identity(child, root)
            generated.append(child)
        source["ce_collider"] = "compound"

    source["ce_collision_object"] = root.name
    if str(source.get("ce_classname", "") or "") in {"", "prop"} and \
            str(source.get("ce_physics_motion", "None")).lower() == "none":
        source["ce_physics_motion"] = "Static"
    root.hide_set(False)
    root.select_set(True)
    for hull in generated:
        hull.hide_set(False)
        hull.select_set(True)
    source.select_set(True)
    context.view_layer.objects.active = source
    context.view_layer.update()
    return generated
