from __future__ import annotations

import sys
import types
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from ceassetlib.blender_scene import LightmapPlacement, object_transform, scene_description
from ceassetlib.game_schema import GameSchema, SchemaEntity
from ceassetlib.scene_export import (
    LightEntity, Prop,
    SkyboxEntity,
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
        objects = [
            FakeObject("Mesh", "MESH", materials=(material,)),
            FakeObject("Player", "EMPTY", props={"ce_classname": "player"}),
            FakeObject("Light", "LIGHT", data=types.SimpleNamespace(type="SUN", color=(1, .5, .25))),
            FakeObject("Marker", "EMPTY"),
            FakeObject("Rig", "ARMATURE"),
        ]

        scene = scene_description(
            objects,
            {"Mesh": [(Path("compiled/Mesh.cmesh"), "Brick")]},
            {"Brick": Path("compiled/Brick.cmat")},
            lambda path: path.as_posix(),
        )

        self.assertEqual([type(entity.data) for entity in scene.entities], [
            LightEntity, Prop, SchemaEntity,
        ])
        prop = scene.entities[1].data
        self.assertEqual(prop.mesh.path, "compiled/Mesh.cmesh")
        self.assertEqual(prop.materials[0].path, "compiled/Brick.cmat")
        light = scene.entities[0].data
        self.assertTrue(light.casts_shadows)

    def test_static_mesh_receives_external_lightmap_atlas_binding(self) -> None:
        obj = FakeObject("Floor", "MESH")
        scene = scene_description(
            (obj,),
            {"Floor": [(Path("compiled/Floor.cmesh"), "FloorMaterial")]},
            {"FloorMaterial": Path("compiled/Floor.cmat")},
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

    def test_scene_occluder_becomes_shadow_only_prop_without_lightmap(self) -> None:
        obj = FakeObject("OCC_SunBlocker", "MESH", props={"ce_role": "occluder"})
        scene = scene_description(
            (obj,),
            {"OCC_SunBlocker": [(Path("compiled/OCC_SunBlocker.cmesh"), "BlockerMaterial")]},
            {"BlockerMaterial": Path("compiled/BlockerMaterial.cmat")},
            lambda path: path.as_posix(),
        )

        prop = scene.entities[0].data
        self.assertIsInstance(prop, Prop)
        self.assertTrue(prop.shadow_only)
        self.assertIsNone(prop.lightmap)

    def test_missing_generated_mesh_fails_with_object_name(self) -> None:
        with self.assertRaisesRegex(ValueError, "Missing"):
            scene_description((FakeObject("Missing", "MESH"),), {}, {}, lambda path: path.as_posix())

    def test_explicit_gameplay_classnames_select_fixed_entity_types(self) -> None:
        objects = (
            FakeObject("Mover", "MESH", props={
                "ce_classname": "prop", "ce_dynamic": True, "ce_collision": True}),
        )

        scene = scene_description(
            objects,
            {"Mover": [(Path("Mover.cmesh"), "MoverMaterial")]},
            {"MoverMaterial": Path("Mover.cmat")},
            lambda path: path.as_posix())

        self.assertIsInstance(scene.entities[0].data, Prop)
        self.assertTrue(scene.entities[0].data.dynamic)
        self.assertTrue(scene.entities[0].data.collision_enabled)

    def test_player_uses_native_blender_camera_settings(self) -> None:
        camera = types.SimpleNamespace(
            angle_y=1.2, clip_start=0.25, clip_end=750.0)
        obj = FakeObject(
            "Player", "CAMERA", data=camera,
            props={"ce_classname": "player", "ce_view_mode": "FirstPerson"})

        scene = scene_description((obj,), {}, {}, lambda path: path.as_posix())

        player = scene.entities[0].data
        self.assertIsInstance(player, SchemaEntity)
        self.assertEqual(player.values["vertical_fov_radians"], 1.2)
        self.assertEqual(player.values["near_clip"], 0.25)
        self.assertEqual(player.values["far_clip"], 750.0)

    def test_custom_entity_asset_fields_use_the_packaged_schema(self) -> None:
        game = GameSchema(
            Path("game.json"),
            {"id": "test"},
            {},
            ({"name": "mesh", "extension": ".cmesh"},),
            ({
                "classname": "pickup",
                "version": 1,
                "fields": (
                    {"name": "transform", "type": "transform"},
                    {"name": "model", "type": "asset", "asset_type": "mesh"},
                ),
            },),
        )
        obj = FakeObject(
            "Pickup", "EMPTY",
            props={
                "ce_classname": "pickup",
                "ce_model": "//compiled/Pickup.cmesh",
            })

        scene = scene_description(
            (obj,), {}, {}, lambda path: path.as_posix(),
            game_schema=game,
            resolve_asset_path=lambda value: Path(
                value.removeprefix("//")))

        pickup = scene.entities[0].data
        self.assertIsInstance(pickup, SchemaEntity)
        self.assertEqual(
            pickup.values["model"].path, "compiled/Pickup.cmesh")

    def test_environment_entities_export_authored_settings(self) -> None:
        objects = (
            FakeObject("Sky", "EMPTY", props={
                "ce_classname": "skybox", "ce_intensity": 1.5, "ce_rotation_radians": 0.25}),
            FakeObject("Fog", "EMPTY", props={
                "ce_classname": "exponential_height_fog", "ce_fog_density": 0.04,
                "ce_height_falloff": 0.3, "ce_start_distance": 2.0,
                "ce_max_opacity": 0.8, "ce_cutoff_distance": 500.0,
                "ce_inscattering_color": (0.4, 0.5, 0.6)}),
            FakeObject("Post", "EMPTY", props={
                "ce_classname": "post_process", "ce_exposure": 1.25,
                "ce_bloom_threshold": 1.75, "ce_ssao_radius": 0.8}),
        )
        scene = scene_description(objects, {}, {}, lambda path: path.as_posix(),
            skybox_outputs={"Sky": Path("environments/test.dds")})
        fog = scene.entities[0].data
        post_process = scene.entities[1].data
        sky = scene.entities[2].data
        self.assertIsInstance(fog, SchemaEntity)
        self.assertEqual(fog.values["density"], 0.04)
        self.assertIsInstance(post_process, SchemaEntity)
        self.assertEqual(post_process.values["exposure"], 1.25)
        self.assertEqual(post_process.values["ssao_radius"], 0.8)
        self.assertIsInstance(sky, SkyboxEntity)
        self.assertEqual(sky.panorama.path, "environments/test.dds")
        self.assertEqual(sky.intensity, 1.5)


if __name__ == "__main__":
    unittest.main()
