from __future__ import annotations

import sys
import tempfile
import types
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ADDON_ROOT = ROOT.parent / "blender_addon"
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ADDON_ROOT))

from ceassetlib.blender_scene import LightmapPlacement
from ceassetlib.game_schema import load_bundled_game
from cengine_asset_exporter.authoring import (
    clear_lightmap_bindings,
    entity_classname,
    initialize_entity_properties,
    load_lightmap_bindings,
    native_object_type,
    store_lightmap_bindings,
    validate_entities,
)


class FakeObject(dict):
    def __init__(self, name: str, obj_type: str, data=None, **properties) -> None:
        super().__init__(properties)
        self.name = name
        self.type = obj_type
        self.data = data


class BlenderAuthoringTests(unittest.TestCase):
    def test_native_blender_types_cover_visual_entities(self) -> None:
        self.assertEqual(native_object_type("prop"), "MESH")
        self.assertEqual(native_object_type("light"), "LIGHT")
        self.assertEqual(native_object_type("player"), "CAMERA")
        self.assertEqual(native_object_type("post_process"), "EMPTY")

    def test_native_mesh_and_light_objects_are_entities_without_metadata(self) -> None:
        self.assertEqual(entity_classname(FakeObject("Mesh", "MESH")), "prop")
        self.assertEqual(entity_classname(FakeObject("Sun", "LIGHT")), "light")
        self.assertEqual(entity_classname(FakeObject("Marker", "EMPTY")), "")

    def test_schema_initialization_uses_authored_property_names_and_enum_labels(self) -> None:
        schema = load_bundled_game().entity("light")
        assert schema is not None
        obj = FakeObject("Sun", "LIGHT")

        initialize_entity_properties(obj, schema)

        self.assertEqual(obj["ce_classname"], "light")
        self.assertEqual(obj["ce_light_mode"], "Realtime")
        self.assertTrue(obj["ce_casts_shadows"])

    def test_validation_recommends_native_camera_for_player(self) -> None:
        obj = FakeObject("Player", "EMPTY", ce_classname="player")

        issues = validate_entities((obj,), load_bundled_game())

        self.assertTrue(any("camera" in issue.message for issue in issues))

    def test_lightmap_bindings_round_trip_without_rebaking(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            atlas = Path(temporary) / "Room_0.dds"
            atlas.write_bytes(b"dds")
            uv_layers = types.SimpleNamespace(
                get=lambda name: object() if name == "Lightmap" else None)
            obj = FakeObject(
                "Floor", "MESH", data=types.SimpleNamespace(uv_layers=uv_layers))
            placement = LightmapPlacement(
                atlas, (0.5, 0.5), (0.5, 0.0), 16.0)

            store_lightmap_bindings(
                (obj,), {"Floor": placement}, lambda path: path.as_posix())
            loaded, outputs = load_lightmap_bindings(
                (obj,), lambda value: Path(value))

            self.assertEqual(loaded, {"Floor": placement})
            self.assertEqual(outputs, [atlas])
            self.assertEqual(clear_lightmap_bindings((obj,)), 1)
            self.assertEqual(load_lightmap_bindings(
                (obj,), lambda value: Path(value)), ({}, []))

    def test_baked_lights_require_static_mesh_bindings(self) -> None:
        light = FakeObject(
            "Sun", "LIGHT", ce_classname="light", ce_light_mode="Mixed")
        mesh = FakeObject("Floor", "MESH")

        with self.assertRaisesRegex(RuntimeError, "Floor"):
            load_lightmap_bindings((light, mesh), Path)


if __name__ == "__main__":
    unittest.main()
