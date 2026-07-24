"""Headless Blender smoke test for native physics-mesh generation."""

from __future__ import annotations

import sys
import tempfile
from pathlib import Path

import bpy


ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "tools" / "ceasset"))
sys.path.insert(0, str(ROOT / "tools" / "blender_addon"))

from cengine_asset_exporter import register, unregister  # noqa: E402
from cengine_asset_exporter.physics import (  # noqa: E402
    ShapeType,
    collision_shape_for_object,
    write_physics_asset,
)
from cengine_asset_exporter.physics_mesh import (  # noqa: E402
    PhysicsMeshSettings,
    generate_physics_mesh,
    key_vertex_indices,
)


def concave_prism() -> object:
    """Create one connected L-shaped closed mesh with concave n-gons."""
    profile = (
        (0.0, 0.0), (3.0, 0.0), (3.0, 1.0),
        (1.0, 1.0), (1.0, 3.0), (0.0, 3.0),
    )
    vertices = [
        (x, y, z)
        for z in (-0.5, 0.5)
        for x, y in profile
    ]
    faces = [
        tuple(reversed(range(6))),
        tuple(range(6, 12)),
    ]
    for index in range(6):
        following = (index + 1) % 6
        faces.append((
            index, following, following + 6, index + 6))
    mesh = bpy.data.meshes.new("PhysicsSmokeSource")
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    source = bpy.data.objects.new("PhysicsSmokeSource", mesh)
    bpy.context.collection.objects.link(source)
    source["ce_classname"] = "prop"
    source["ce_physics_motion"] = "Dynamic"
    source.scale = (2.0, 3.0, 4.0)
    return source


def main() -> None:
    register()
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    source = concave_prism()
    bpy.context.view_layer.objects.active = source
    source.select_set(True)

    single = generate_physics_mesh(
        bpy.context, source, PhysicsMeshSettings(concave=False))
    assert len(single) == 1
    assert source["ce_collider"] == "convex_hull"
    shape = collision_shape_for_object(single[0])
    assert shape.shape_type == ShapeType.CONVEX_HULL
    assert 4 <= len(shape.vertices) <= 256

    compound_hulls = generate_physics_mesh(
        bpy.context,
        source,
        PhysicsMeshSettings(concave=True, accuracy=1.0, max_hulls=4),
    )
    root = bpy.data.objects[source["ce_collision_object"]]
    assert source["ce_collider"] == "compound"
    assert root["ce_collider"] == "compound"
    assert root.select_get()
    assert 2 <= len(compound_hulls) <= 4
    assert all(hull.select_get() for hull in compound_hulls)
    assert all(hull.display_type == "SOLID" for hull in compound_hulls)
    assert all(hull.show_wire for hull in compound_hulls)
    assert bpy.context.view_layer.objects.active is source
    compound = collision_shape_for_object(root)
    assert compound.shape_type == ShapeType.COMPOUND
    assert len(compound.children) == len(compound_hulls)
    assert all(
        4 <= len(child.vertices) <= 256
        for child in compound.children
    )
    with tempfile.TemporaryDirectory() as temporary:
        exported = write_physics_asset(
            Path(temporary) / "smoke.blend",
            Path(temporary),
            source,
            collision_source=root,
        )
        assert exported.output.is_file()

    huge_vertices = [
        (
            float(index),
            float((index * 17) % 1009),
            float((index * 31) % 1013),
        )
        for index in range(250_000)
    ]
    selected = key_vertex_indices(
        huge_vertices, list(range(len(huge_vertices))))
    assert len(selected) <= 512
    assert len(selected) == len(set(selected))
    print(
        "physics_mesh_smoke: "
        f"{len(compound_hulls)} hulls, "
        f"{len(selected)}/250000 key vertices")
    unregister()


if __name__ == "__main__":
    main()
