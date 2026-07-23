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

from __future__ import annotations

import struct
import sys
import tempfile
import unittest
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "blender_addon"))

from cengine_asset_exporter.physics import (  # noqa: E402
    INVALID_SHAPE,
    PHYSICS_HEADER,
    PHYSICS_MAGIC,
    PHYSICS_SHAPE,
    CollisionShape,
    ShapeType,
    collision_shape_for_object,
    physics_payload,
    physics_output_path,
    validate_shape,
    write_physics_asset,
    write_physics_assets,
)


ASSET_HEADER = struct.Struct("<4sHHI16sQ16sQQQ")


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
        header = PHYSICS_HEADER.unpack_from(payload)
        self.assertEqual(header[0], PHYSICS_MAGIC)
        self.assertEqual(header[3], 3)
        root_record = PHYSICS_SHAPE.unpack_from(payload, header[5])
        first_child = PHYSICS_SHAPE.unpack_from(
            payload, header[5] + PHYSICS_SHAPE.size)
        second_child = PHYSICS_SHAPE.unpack_from(
            payload, header[5] + PHYSICS_SHAPE.size * 2)
        self.assertEqual(root_record[0:2],
            (int(ShapeType.COMPOUND), INVALID_SHAPE))
        self.assertEqual(first_child[1], 0)
        self.assertEqual(second_child[1], 0)

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
            PHYSICS_HEADER.unpack_from(physics_payload(compound))[3], 3)

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


if __name__ == "__main__":
    unittest.main()
