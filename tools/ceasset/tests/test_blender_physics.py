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

import struct
import sys
import tempfile
import unittest
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "blender_addon"))

from cengine_asset_exporter.physics import (  # noqa: E402
    CollisionShape,
    ShapeType,
    collision_shape_for_object,
    physics_payload,
    physics_output_path,
    validate_shape,
    write_physics_asset,
    write_physics_assets,
)
from cengine_asset_exporter.physics_mesh import (  # noqa: E402
    coalesce_triangle_components,
    connected_triangle_components,
    key_vertex_indices,
    split_triangle_cluster,
)
from ceassetlib.game_schema import load_bundled_game
from ceassetlib.wire import unpack_record


ASSET_HEADER = struct.Struct("<4sHHI16sQ16sQQQ")
GAME_SCHEMA = load_bundled_game()


class FakeVertex:
    """TODO: Describe `FakeVertex`."""

    def __init__(self, co):
        """TODO: Describe `__init__`.

        Args:
            co: TODO: Describe this parameter.
        """
        self.co = co


class FakePolygon:
    """TODO: Describe `FakePolygon`."""

    def __init__(self, vertices):
        """TODO: Describe `__init__`.

        Args:
            vertices: TODO: Describe this parameter.
        """
        self.vertices = vertices


class FakeObject(dict):
    """TODO: Describe `FakeObject`."""

    def __init__(self, name: str, **properties):
        """TODO: Describe `__init__`.

        Args:
            name: TODO: Describe this parameter.
            **properties: TODO: Describe this parameter.
        """
        super().__init__(properties)
        self.name = name
        self.type = "MESH"
        self.scale = (1.0, 1.0, 1.0)
        self.dimensions = (2.0, 4.0, 6.0)
        self.data = type("Mesh", (), {
            "vertices": [
                FakeVertex((0.0, 0.0, 0.0)),
                FakeVertex((1.0, 0.0, 0.0)),
                FakeVertex((0.0, 1.0, 0.0)),
                FakeVertex((0.0, 0.0, 1.0)),
            ],
            "polygons": [
                FakePolygon((0, 1, 2)),
                FakePolygon((0, 3, 1)),
                FakePolygon((0, 2, 3)),
                FakePolygon((1, 3, 2)),
            ],
        })()
        self.children = []
        self.matrix_local = None
        self.parent = None


class BlenderPhysicsTests(unittest.TestCase):
    """TODO: Describe `BlenderPhysicsTests`."""

    def test_compound_payload_is_preorder_and_engine_neutral(self) -> None:
        """TODO: Describe `test_compound_payload_is_preorder_and_engine_neutral`."""
        root = CollisionShape(
            shape_type=ShapeType.COMPOUND,
            children=[
                CollisionShape(
                    shape_type=ShapeType.BOX,
                    half_extents=(1.0, 2.0, 3.0),
                ),
                CollisionShape(
                    shape_type=ShapeType.SPHERE,
                    radius=2.0,
                    local_position=(4.0, 0.0, 0.0),
                ),
            ],
        )
        payload = physics_payload(root)
        decoded = unpack_record(GAME_SCHEMA, "physics", payload)
        self.assertEqual(len(decoded["nodes"]), 3)
        self.assertEqual(
            (decoded["nodes"][0]["type"], decoded["nodes"][0]["parent"]),
            (int(ShapeType.COMPOUND), -1),
        )
        self.assertEqual(decoded["nodes"][1]["parent"], 0)
        self.assertEqual(decoded["nodes"][2]["parent"], 0)

    def test_invalid_triangle_index_is_rejected(self) -> None:
        """TODO: Describe `test_invalid_triangle_index_is_rejected`."""
        shape = CollisionShape(
            shape_type=ShapeType.TRIANGLE_MESH,
            vertices=[(0.0, 0.0, 0.0)] * 3,
            indices=[0, 1, 3],
        )
        with self.assertRaisesRegex(ValueError, "invalid"):
            validate_shape(shape)

    def test_dynamic_triangle_mesh_has_actionable_error(self) -> None:
        """TODO: Describe `test_dynamic_triangle_mesh_has_actionable_error`."""
        with tempfile.TemporaryDirectory() as tmp:
            obj = FakeObject(
                "Moving", ce_physics_motion="dynamic",
                ce_collider="triangle_mesh")
            with self.assertRaisesRegex(
                    ValueError, "choose convex_hull or compound"):
                write_physics_asset(
                    Path(tmp) / "scene.blend", Path(tmp), obj)

    def test_plane_cooks_and_is_static_only(self) -> None:
        """TODO: Describe `test_plane_cooks_and_is_static_only`."""
        plane = FakeObject("Ground", ce_collider="plane")
        shape = collision_shape_for_object(plane)
        self.assertEqual(shape.shape_type, ShapeType.PLANE)
        self.assertEqual(shape.half_extents, (2.0, 1.0, 0.0))
        validate_shape(shape)

        with tempfile.TemporaryDirectory() as tmp:
            plane["ce_physics_motion"] = "dynamic"
            with self.assertRaisesRegex(
                    ValueError, "convex_hull or compound"):
                write_physics_asset(
                    Path(tmp) / "scene.blend", Path(tmp), plane)

    def test_writes_cphys_asset_envelope(self) -> None:
        """TODO: Describe `test_writes_cphys_asset_envelope`."""
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            blend = root / "scene.blend"
            blend.write_bytes(b"blend")
            obj = FakeObject(
                "Crate", ce_physics_motion="static",
                ce_collider="convex_hull")
            exported = write_physics_asset(blend, root, obj)
            envelope = ASSET_HEADER.unpack_from(
                exported.output.read_bytes())
            self.assertEqual(envelope[0], b"CEAF")
            self.assertEqual(envelope[3], 6)
            self.assertEqual(
                physics_output_path(blend, root, "Crate"),
                exported.output)

    def test_height_field_grid_and_compound_helpers_cook(self) -> None:
        """TODO: Describe `test_height_field_grid_and_compound_helpers_cook`."""
        height_field = FakeObject(
            "Terrain", ce_collider="height_field")
        height_field.data = type("Mesh", (), {
            "vertices": [
                FakeVertex((0.0, 0.0, 0.0)),
                FakeVertex((1.0, 0.0, 1.0)),
                FakeVertex((0.0, 1.0, 2.0)),
                FakeVertex((1.0, 1.0, 3.0)),
            ],
            "polygons": (),
        })()
        terrain_shape = collision_shape_for_object(height_field)
        self.assertEqual(terrain_shape.shape_type, ShapeType.HEIGHT_FIELD)
        self.assertEqual(terrain_shape.samples_per_side, 2)
        self.assertEqual(len(terrain_shape.heights), 4)

        root = FakeObject("Compound", ce_collider="compound")
        root.children = [
            FakeObject("BoxChild", ce_collider="box"),
            FakeObject("SphereChild", ce_collider="sphere"),
        ]
        compound = collision_shape_for_object(root)
        self.assertEqual(compound.shape_type, ShapeType.COMPOUND)
        self.assertEqual(
            [child.shape_type for child in compound.children],
            [ShapeType.BOX, ShapeType.SPHERE])
        self.assertEqual(
            len(unpack_record(
                GAME_SCHEMA, "physics", physics_payload(compound)
            )["nodes"]),
            3,
        )

    def test_explicit_collision_mesh_is_cooked_for_parent_body(self) -> None:
        """TODO: Describe `test_explicit_collision_mesh_is_cooked_for_parent_body`."""
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            body = FakeObject(
                "Body", ce_physics_motion="dynamic",
                ce_collider="convex_hull",
                ce_collision_object="BodyCollider")
            collider = FakeObject(
                "BodyCollider", ce_collider="convex_hull",
                ce_collision_helper=True)
            collider.parent = body

            outputs = write_physics_assets(
                root / "scene.blend", root, (body, collider))

            self.assertEqual(len(outputs), 1)
            self.assertIs(outputs[0].source, body)
            self.assertEqual(outputs[0].output.name, "Body.cphys")

    def test_physics_mesh_groups_connected_triangles(self) -> None:
        """Connected-component discovery handles multiple loose pieces."""
        triangles = [
            (0, 1, 2), (0, 3, 1), (0, 2, 3), (1, 3, 2),
            (4, 5, 6), (4, 7, 5), (4, 6, 7), (5, 7, 6),
        ]

        self.assertEqual(
            connected_triangle_components(triangles),
            [list(range(4)), list(range(4, 8))],
        )

    def test_physics_mesh_coalesces_excess_loose_pieces(self) -> None:
        """Loose geometry stays under the hard compound hull limit."""
        vertices = [
            (float(index), float(y), float(z))
            for index in range(6)
            for y, z in ((0, 0), (1, 0), (0, 1))
        ]
        triangles = [
            (index * 3, index * 3 + 1, index * 3 + 2)
            for index in range(6)
        ]
        components = [[index] for index in range(6)]

        grouped = coalesce_triangle_components(
            vertices, triangles, components, 2)

        self.assertEqual(len(grouped), 2)
        self.assertEqual(
            sorted(index for group in grouped for index in group),
            list(range(6)),
        )

    def test_physics_mesh_cluster_split_is_balanced(self) -> None:
        """Median spatial splitting does not duplicate or lose triangles."""
        vertices = [
            (float(index), float(y), float(z))
            for index in range(8)
            for y, z in ((0, 0), (1, 0), (0, 1))
        ]
        triangles = [
            (index * 3, index * 3 + 1, index * 3 + 2)
            for index in range(8)
        ]

        split = split_triangle_cluster(
            vertices, triangles, list(range(8)))

        self.assertIsNotNone(split)
        assert split is not None
        self.assertEqual((len(split[0]), len(split[1])), (4, 4))
        self.assertEqual(sorted((*split[0], *split[1])), list(range(8)))

    def test_physics_mesh_key_vertices_are_bounded_for_huge_sources(
            self) -> None:
        """Hull construction receives a fixed-size representative sample."""
        vertices = [
            (float(index), float(index % 17), float(index % 31))
            for index in range(100_000)
        ]

        selected = key_vertex_indices(
            vertices, list(range(len(vertices))), maximum=256)

        self.assertLessEqual(len(selected), 256)
        self.assertEqual(len(selected), len(set(selected)))
        self.assertIn(0, selected)
        self.assertIn(len(vertices) - 1, selected)


if __name__ == "__main__":
    unittest.main()
