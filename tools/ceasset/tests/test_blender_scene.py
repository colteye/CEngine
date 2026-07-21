from __future__ import annotations

import sys
import types
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from ceassetlib.blender_scene import LightmapPlacement, object_transform, scene_description
from ceassetlib.scene_export import (
    CameraEntity, EmptyEntity, LightEntity, PlayerStart,
    PrefabEntity, Prop, TriggerEntity,
)


class FakeObject:
    def __init__(self, name: str, obj_type: str, matrix=None, data=None,
                 materials=(), instance_collection=None, props=None) -> None:
        self.name = name
        self.type = obj_type
        self.matrix_world = matrix
        self.data = data
        self.material_slots = [types.SimpleNamespace(material=material) for material in materials]
        self.instance_collection = instance_collection
        self.props = props or {}

    def get(self, key: str, default=None):
        return self.props.get(key, default)


class BlenderSceneTests(unittest.TestCase):
    def test_world_matrix_is_flattened_and_converted_to_engine_space(self) -> None:
        obj = FakeObject("Placed", "EMPTY", [
            [1.0, 0.0, 0.0, 2.0],
            [0.0, 1.0, 0.0, 3.0],
            [0.0, 0.0, 1.0, 4.0],
            [0.0, 0.0, 0.0, 1.0],
        ])

        transform = object_transform(obj)

        self.assertEqual(transform.position, (3.0, -2.0, 4.0))
        self.assertAlmostEqual(transform.scale[0], 1.0)
        self.assertAlmostEqual(transform.rotation[3], 1.0)

    def test_objects_become_fixed_entities_with_external_asset_references(self) -> None:
        material = types.SimpleNamespace(name="Brick")
        prefab_collection = types.SimpleNamespace(name="PREFAB_Door")
        objects = [
            FakeObject("Mesh", "MESH", materials=(material,)),
            FakeObject("Camera", "CAMERA", data=types.SimpleNamespace(type="PERSP")),
            FakeObject("Light", "LIGHT", data=types.SimpleNamespace(type="SUN", color=(1, .5, .25))),
            FakeObject("Door", "EMPTY", instance_collection=prefab_collection),
            FakeObject("Marker", "EMPTY"),
            FakeObject("Rig", "ARMATURE"),
        ]

        scene = scene_description(
            objects,
            {"Mesh": [(Path("compiled/Mesh.cmesh"), "Brick")]},
            {"Brick": Path("compiled/Brick.cmat")},
            {"PREFAB_Door": Path("compiled/Door.casset")},
            lambda path: path.as_posix(),
        )

        self.assertEqual([type(entity.data) for entity in scene.entities], [
            CameraEntity, PrefabEntity, LightEntity, EmptyEntity, Prop,
        ])
        prop = scene.entities[-1].data
        self.assertEqual(prop.mesh.path, "compiled/Mesh.cmesh")
        self.assertEqual(prop.materials[0].path, "compiled/Brick.cmat")
        prefab = scene.entities[1].data
        self.assertEqual(prefab.prefab.path, "compiled/Door.casset")

    def test_static_mesh_receives_external_lightmap_atlas_binding(self) -> None:
        obj = FakeObject("Floor", "MESH")
        scene = scene_description(
            (obj,),
            {"Floor": [(Path("compiled/Floor.cmesh"), "FloorMaterial")]},
            {"FloorMaterial": Path("compiled/Floor.cmat")},
            {},
            lambda path: path.as_posix(),
            lightmaps={"Floor": LightmapPlacement(
                Path("compiled/lightmap_0.dds"), (0.5, 0.5), (0.5, 0.0), 12.0)},
        )

        prop = scene.entities[0].data
        self.assertIsInstance(prop, Prop)
        self.assertEqual(prop.lightmap.path, "compiled/lightmap_0.dds")
        self.assertEqual(prop.lightmap_scale, (0.5, 0.5))
        self.assertEqual(prop.lightmap_offset, (0.5, 0.0))
        self.assertEqual(prop.lightmap_rgbm_range, 12.0)

    def test_missing_generated_mesh_fails_with_object_name(self) -> None:
        with self.assertRaisesRegex(ValueError, "Missing"):
            scene_description((FakeObject("Missing", "MESH"),), {}, {}, {}, lambda path: path.as_posix())

    def test_explicit_gameplay_classnames_select_fixed_entity_types(self) -> None:
        objects = (
            FakeObject("Mover", "MESH", props={
                "ce_classname": "prop", "ce_dynamic": True, "ce_collision": True}),
            FakeObject("Spawn", "EMPTY", props={"ce_classname": "info_player_start", "ce_team": 2}),
            FakeObject("Volume", "EMPTY", props={
                "ce_classname": "trigger", "ce_half_extents": (2, 3, 4), "ce_trigger_once": True}),
        )

        scene = scene_description(
            objects,
            {"Mover": [(Path("Mover.cmesh"), "MoverMaterial")]},
            {"MoverMaterial": Path("Mover.cmat")},
            {},
            lambda path: path.as_posix())

        self.assertIsInstance(scene.entities[0].data, Prop)
        self.assertTrue(scene.entities[0].data.dynamic)
        self.assertTrue(scene.entities[0].data.collision_enabled)
        self.assertIsInstance(scene.entities[1].data, PlayerStart)
        self.assertEqual(scene.entities[1].data.team, 2)
        self.assertIsInstance(scene.entities[2].data, TriggerEntity)
        self.assertEqual(scene.entities[2].data.half_extents, (2.0, 3.0, 4.0))


if __name__ == "__main__":
    unittest.main()
