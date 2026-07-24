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

import sys
import types
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from ceassetlib.blender_scene import LightmapPlacement, object_transform, scene_description
from ceassetlib.game_schema import GameSchema, SchemaEntity


class FakeObject:
    """TODO: Describe `FakeObject`."""

    def __init__(self, name: str, obj_type: str, matrix=None, data=None,
                 materials=(), instance_collection=None, props=None) -> None:
        """TODO: Describe `__init__`.

        Args:
            name: TODO: Describe this parameter.
            obj_type: TODO: Describe this parameter.
            matrix: TODO: Describe this parameter.
            data: TODO: Describe this parameter.
            materials: TODO: Describe this parameter.
            instance_collection: TODO: Describe this parameter.
            props: TODO: Describe this parameter.
        """
        self.name = name
        self.type = obj_type
        self.matrix_world = matrix
        self.data = data
        self.material_slots = [types.SimpleNamespace(material=material) for material in materials]
        self.instance_collection = instance_collection
        self.props = props or {}
        self.parent = None
        self.modifiers = ()

    def get(self, key: str, default=None):
        """TODO: Describe `get`.

        Args:
            key: TODO: Describe this parameter.
            default: TODO: Describe this parameter.

        Returns:
            TODO: Describe the produced value.
        """
        return self.props.get(key, default)


class BlenderSceneTests(unittest.TestCase):
    """TODO: Describe `BlenderSceneTests`."""

    def test_world_matrix_is_flattened_and_converted_to_engine_space(self) -> None:
        """TODO: Describe `test_world_matrix_is_flattened_and_converted_to_engine_space`."""
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
        """TODO: Describe `test_objects_become_fixed_entities_with_external_asset_references`."""
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

        self.assertTrue(all(
            isinstance(entity.data, SchemaEntity) for entity in scene.entities))
        self.assertEqual(
            [entity.data.classname for entity in scene.entities],
            ["light", "prop", "player"])
        prop = scene.entities[1].data
        self.assertEqual(prop.values["mesh"].path, "compiled/Mesh.cmesh")
        self.assertEqual(
            prop.values["materials"][0].path, "compiled/Brick.cmat")
        light = scene.entities[0].data
        self.assertTrue(light.values["casts_shadows"])

    def test_static_mesh_receives_external_lightmap_atlas_binding(self) -> None:
        """TODO: Describe `test_static_mesh_receives_external_lightmap_atlas_binding`."""
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
        self.assertIsInstance(prop, SchemaEntity)
        self.assertEqual(
            prop.values["lightmap"].path, "compiled/lightmap_0.dds")
        self.assertEqual(prop.values["lightmap_scale"], (0.5, 0.5))
        self.assertEqual(prop.values["lightmap_offset"], (0.5, 0.0))
        self.assertEqual(prop.values["lightmap_rgbm_range"], 12.0)

    def test_skinned_mesh_receives_skeleton_and_default_animation(self) -> None:
        armature = FakeObject("Rig", "ARMATURE")
        obj = FakeObject("Character", "MESH")
        obj.parent = armature

        scene = scene_description(
            (obj, armature),
            {"Character": [(Path("Character.cmesh"), "Character")]},
            {"Character": Path("Character.cmat")},
            lambda path: path.as_posix(),
            skeleton_outputs={"Rig": Path("Rig.cskel")},
            animation_outputs={
                "Rig": [Path("Rig_Walk.canim"), Path("Rig_Idle.canim")]},
        )

        prop = scene.entities[0].data
        self.assertEqual(prop.values["skeleton"].path, "Rig.cskel")
        self.assertEqual(prop.values["animation"].path, "Rig_Idle.canim")
        self.assertEqual(prop.values["animation_playback_rate"], 1.0)
        self.assertTrue(prop.values["animation_looping"])

    def test_skinned_mesh_without_actions_exports_reference_pose(self) -> None:
        armature = FakeObject("Rig", "ARMATURE")
        obj = FakeObject("Character", "MESH")
        obj.parent = armature

        scene = scene_description(
            (obj, armature),
            {"Character": [(Path("Character.cmesh"), "Character")]},
            {"Character": Path("Character.cmat")},
            lambda path: path.as_posix(),
            skeleton_outputs={"Rig": Path("Rig.cskel")},
        )

        prop = scene.entities[0].data
        self.assertEqual(prop.values["skeleton"].path, "Rig.cskel")
        self.assertIsNone(prop.values["animation"])

    def test_scene_occluder_becomes_shadow_only_prop_without_lightmap(self) -> None:
        """TODO: Describe `test_scene_occluder_becomes_shadow_only_prop_without_lightmap`."""
        obj = FakeObject("OCC_SunBlocker", "MESH", props={"ce_role": "occluder"})
        scene = scene_description(
            (obj,),
            {"OCC_SunBlocker": [(Path("compiled/OCC_SunBlocker.cmesh"), "BlockerMaterial")]},
            {"BlockerMaterial": Path("compiled/BlockerMaterial.cmat")},
            lambda path: path.as_posix(),
        )

        prop = scene.entities[0].data
        self.assertIsInstance(prop, SchemaEntity)
        self.assertTrue(prop.values["shadow_only"])
        self.assertIsNone(prop.values["lightmap"])

    def test_missing_generated_mesh_fails_with_object_name(self) -> None:
        """TODO: Describe `test_missing_generated_mesh_fails_with_object_name`."""
        with self.assertRaisesRegex(ValueError, "Missing"):
            scene_description((FakeObject("Missing", "MESH"),), {}, {}, lambda path: path.as_posix())

    def test_explicit_gameplay_classnames_select_fixed_entity_types(self) -> None:
        """TODO: Describe `test_explicit_gameplay_classnames_select_fixed_entity_types`."""
        objects = (
            FakeObject("Mover", "MESH", props={
                "ce_classname": "prop",
                "ce_physics_motion": "Dynamic",
                "ce_collider": "box"}),
        )

        scene = scene_description(
            objects,
            {"Mover": [(Path("Mover.cmesh"), "MoverMaterial")]},
            {"MoverMaterial": Path("Mover.cmat")},
            lambda path: path.as_posix(),
            physics_outputs={"Mover": Path("Mover.cphys")})

        prop = scene.entities[0].data
        self.assertIsInstance(prop, SchemaEntity)
        self.assertEqual(prop.values["motion"], 2)
        self.assertEqual(prop.values["collision"].path, "Mover.cphys")

    def test_constraints_resolve_physics_props_and_export_last(self) -> None:
        """TODO: Describe `test_constraints_resolve_physics_props_and_export_last`."""
        objects = (
            FakeObject("Joint", "EMPTY", props={
                "ce_classname": "physics_constraint",
                "ce_first_entity": "BodyA",
                "ce_second_entity": "BodyB",
                "ce_type": "Hinge",
                "ce_axis": (0.0, 0.0, 1.0),
                "ce_normal": (1.0, 0.0, 0.0),
            }),
            FakeObject("BodyB", "MESH", props={
                "ce_physics_motion": "Static"}),
            FakeObject("BodyA", "MESH", props={
                "ce_physics_motion": "Dynamic"}),
        )
        scene = scene_description(
            objects,
            {
                "BodyA": [(Path("BodyA.cmesh"), "BodyA")],
                "BodyB": [(Path("BodyB.cmesh"), "BodyB")],
            },
            {
                "BodyA": Path("BodyA.cmat"),
                "BodyB": Path("BodyB.cmat"),
            },
            lambda path: path.as_posix(),
            physics_outputs={
                "BodyA": Path("BodyA.cphys"),
                "BodyB": Path("BodyB.cphys"),
            })

        self.assertEqual(
            [entity.data.classname for entity in scene.entities],
            ["prop", "prop", "physics_constraint"])
        constraint = scene.entities[-1].data
        self.assertEqual(constraint.values["first_entity"], 0)
        self.assertEqual(constraint.values["second_entity"], 1)
        self.assertEqual(constraint.values["type"], 2)

    def test_constraint_rejects_non_physics_reference(self) -> None:
        """TODO: Describe `test_constraint_rejects_non_physics_reference`."""
        objects = (
            FakeObject("Body", "MESH"),
            FakeObject("Joint", "EMPTY", props={
                "ce_classname": "physics_constraint",
                "ce_first_entity": "Body",
                "ce_second_entity": "Missing",
            }),
        )
        with self.assertRaisesRegex(ValueError, "references no physics entity"):
            scene_description(
                objects,
                {"Body": [(Path("Body.cmesh"), "Body")]},
                {"Body": Path("Body.cmat")},
                lambda path: path.as_posix())

    def test_player_uses_native_blender_camera_settings(self) -> None:
        """TODO: Describe `test_player_uses_native_blender_camera_settings`."""
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

    def test_player_spawn_exports_game_owned_selection_metadata(self) -> None:
        spawn = FakeObject("RedHigh", "EMPTY", props={
            "ce_classname": "player_spawn",
            "ce_team": "Red",
            "ce_spawn_group": 7,
            "ce_spawn_priority": 10,
            "ce_spawn_clearance_radius": 1.5,
        })

        scene = scene_description(
            (spawn,), {}, {}, lambda path: path.as_posix())

        entity = scene.entities[0].data
        self.assertEqual(entity.classname, "player_spawn")
        self.assertEqual(entity.values["team"], 1)
        self.assertEqual(entity.values["spawn_group"], 7)
        self.assertEqual(entity.values["priority"], 10)
        self.assertEqual(entity.values["clearance_radius"], 1.5)

    def test_base_camera_uses_native_blender_camera_settings(self) -> None:
        camera = types.SimpleNamespace(
            angle_y=0.9, clip_start=0.2, clip_end=600.0)
        scene = scene_description(
            (FakeObject("Camera", "CAMERA", data=camera),),
            {}, {}, lambda path: path.as_posix())

        entity = scene.entities[0].data
        self.assertEqual(entity.classname, "camera")
        self.assertEqual(entity.values["vertical_fov_radians"], 0.9)
        self.assertEqual(entity.values["near_clip"], 0.2)
        self.assertEqual(entity.values["far_clip"], 600.0)

    def test_collider_and_trigger_are_physics_only_mesh_entities(self) -> None:
        objects = (
            FakeObject("Blocker", "MESH", props={
                "ce_classname": "collider",
                "ce_physics_motion": "Dynamic",
                "ce_mass": 3.0,
            }),
            FakeObject("Trigger", "MESH", props={
                "ce_classname": "trigger_volume",
                "ce_kinematic": True,
                "ce_fire_once": True,
            }),
        )
        scene = scene_description(
            objects, {}, {}, lambda path: path.as_posix(),
            physics_outputs={
                "Blocker": Path("Blocker.cphys"),
                "Trigger": Path("Trigger.cphys"),
            })

        self.assertEqual(
            [entity.data.classname for entity in scene.entities],
            ["collider", "trigger_volume"])
        self.assertEqual(scene.entities[0].data.values["motion"], 1)
        self.assertEqual(scene.entities[0].data.values["mass"], 3.0)
        self.assertTrue(scene.entities[1].data.values["kinematic"])
        self.assertTrue(scene.entities[1].data.values["fire_once"])

    def test_speaker_exports_native_audio_source_fields(self) -> None:
        speaker = types.SimpleNamespace(
            volume=0.7,
            pitch=1.2,
            distance_reference=2.0,
            distance_max=40.0,
            cone_angle_inner=1.0,
            cone_angle_outer=2.0,
            cone_volume_outer=0.25,
        )
        scene = scene_description(
            (FakeObject("Alarm", "SPEAKER", data=speaker),),
            {}, {}, lambda path: path.as_posix(),
            audio_outputs={"Alarm": Path("audio/Alarm.ogg")})

        audio = scene.entities[0].data
        self.assertEqual(audio.classname, "audio_source")
        self.assertEqual(audio.values["audio"].path, "audio/Alarm.ogg")
        self.assertEqual(audio.values["gain"], 0.7)
        self.assertEqual(audio.values["pitch"], 1.2)
        self.assertEqual(audio.values["min_distance"], 2.0)
        self.assertEqual(audio.values["max_distance"], 40.0)

    def test_blender_connections_export_schema_checked_io_edges(self) -> None:
        relay = FakeObject("Relay", "EMPTY", props={
            "ce_classname": "logic_relay",
            "ce_connections": ({
                "event": "OnTrigger",
                "target": "Timer",
                "action": "Fire",
                "parameter": "door",
                "delay_seconds": 0.25,
                "times_to_fire": 1,
            },),
        })
        timer = FakeObject("Timer", "EMPTY", props={
            "ce_classname": "logic_timer",
        })
        scene = scene_description(
            (timer, relay), {}, {}, lambda path: path.as_posix())

        self.assertEqual(len(scene.connections), 1)
        connection = scene.connections[0]
        self.assertEqual(
            scene.entities[connection.source_entity].name, "Relay")
        self.assertEqual(
            scene.entities[connection.target_entity].name, "Timer")
        self.assertEqual(connection.event, "OnTrigger")
        self.assertEqual(connection.action, "Fire")
        self.assertEqual(connection.parameter, "door")
        self.assertEqual(connection.delay_seconds, 0.25)
        self.assertEqual(connection.times_to_fire, 1)

    def test_custom_entity_asset_fields_use_the_packaged_schema(self) -> None:
        """TODO: Describe `test_custom_entity_asset_fields_use_the_packaged_schema`."""
        game = GameSchema(
            Path("game.json"),
            {"id": "test"},
            {},
            ({"name": "mesh", "extension": ".cmesh"},),
            (),
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
        """TODO: Describe `test_environment_entities_export_authored_settings`."""
        objects = (
            FakeObject("Sky", "EMPTY", props={
                "ce_classname": "skybox", "ce_intensity": 1.5, "ce_rotation_radians": 0.25}),
            FakeObject("Fog", "EMPTY", props={
                "ce_classname": "fog", "ce_fog_density": 0.04,
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
        self.assertIsInstance(sky, SchemaEntity)
        self.assertEqual(
            sky.values["panorama"].path, "environments/test.dds")
        self.assertEqual(sky.values["intensity"], 1.5)

    def test_native_environment_probe_exports_baked_dynamic_lighting(self) -> None:
        """A native sphere probe binds one baked GI/reflection panorama."""
        probe_data = types.SimpleNamespace(
            type="SPHERE", influence_type="BOX", falloff=0.25)
        probe = FakeObject(
            "Interior Probe", "LIGHT_PROBE",
            matrix=[
                [4.0, 0.0, 0.0, 1.0],
                [0.0, 6.0, 0.0, 2.0],
                [0.0, 0.0, 3.0, 3.0],
                [0.0, 0.0, 0.0, 1.0],
            ],
            data=probe_data,
            props={
                "ce_classname": "environment_probe",
                "ce_intensity": 1.25,
                "ce_enabled": True,
            })
        scene = scene_description(
            (probe,), {}, {}, lambda path: path.as_posix(),
            environment_probe_outputs={
                "Interior Probe": Path("probes/interior.dds")})
        environment = scene.entities[0].data
        self.assertEqual(environment.classname, "environment_probe")
        self.assertEqual(
            environment.values["panorama"].path, "probes/interior.dds")
        self.assertEqual(environment.values["intensity"], 1.25)
        self.assertAlmostEqual(environment.values["blend_distance"], 0.75)


if __name__ == "__main__":
    unittest.main()
