from __future__ import annotations

import struct
import sys
import tempfile
import unittest
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from ceassetlib.collection_export import (
    CASSET_COMPONENT,
    CASSET_HEADER,
    CASSET_MAGIC,
    CASSET_OBJECT,
    blender_to_engine_matrix_rows,
    bundle_relative_path,
    collection_export_spec,
    collection_payload,
    object_role,
    write_collection_assets,
)
from ceassetlib.formats import AssetType


ASSET_HEADER = struct.Struct("<4sHHI16sQ16sQQQ")


class FakeObject:
    def __init__(
        self,
        name: str,
        obj_type: str,
        parent: "FakeObject | None" = None,
        props: dict[str, str] | None = None,
        matrix_world: list[list[float]] | None = None,
    ) -> None:
        self.name = name
        self.type = obj_type
        self.parent = parent
        self.props = props or {}
        self.matrix_world = matrix_world
        self.material_slots: list[object] = []

    def get(self, key: str, default: object = None) -> object:
        return self.props.get(key, default)


class FakeCollection:
    def __init__(self, name: str, objects: list[FakeObject], props: dict[str, str] | None = None) -> None:
        self.name = name
        self.objects = objects
        self.props = props or {}

    def get(self, key: str, default: object = None) -> object:
        return self.props.get(key, default)


class CollectionExportTests(unittest.TestCase):
    def test_occluder_prefix_maps_to_existing_shadow_blocker_role(self) -> None:
        self.assertEqual(object_role(FakeObject("OCC_SunBlocker", "MESH")), "occluder")

    def test_collection_prefix_selects_prefab_asset(self) -> None:
        spec = collection_export_spec(FakeCollection("PREFAB_Hero", []))

        self.assertIsNotNone(spec)
        assert spec is not None
        self.assertEqual(spec.asset_type, AssetType.PREFAB)
        self.assertEqual(spec.asset_name, "Hero")

    def test_collection_custom_properties_select_scene_asset(self) -> None:
        collection = FakeCollection(
            "Lighting Blockout",
            [],
            {"ce_asset_type": "scene", "ce_asset_name": "lighting_blockout"},
        )

        spec = collection_export_spec(collection)

        self.assertIsNotNone(spec)
        assert spec is not None
        self.assertEqual(spec.asset_type, AssetType.SCENE)
        self.assertEqual(spec.asset_name, "lighting_blockout")

    def test_collection_payload_records_roles_parents_and_transforms(self) -> None:
        root = FakeObject(
            "ARM_Hero",
            "ARMATURE",
            matrix_world=[
                [1.0, 0.0, 0.0, 10.0],
                [0.0, 1.0, 0.0, 20.0],
                [0.0, 0.0, 1.0, 30.0],
                [0.0, 0.0, 0.0, 1.0],
            ],
        )
        child = FakeObject("SM_Body", "MESH", parent=root)
        child.material_slots = [type("Slot", (), {"material": type("Material", (), {"name": "HeroSkin"})()})()]
        custom = FakeObject("FaceTarget", "MESH", props={"ce_role": "Morph Target"})
        spec = collection_export_spec(FakeCollection("PREFAB_Hero", []))
        assert spec is not None

        payload = collection_payload(
            Path("hero.blend"),
            spec,
            [child, custom, root],
            lambda obj: {"mesh": "compiled/hero/meshes/SM_Body.cmesh"} if obj.name == "SM_Body" else {},
        )

        header = CASSET_HEADER.unpack_from(payload)
        self.assertEqual(header[0], CASSET_MAGIC)
        self.assertEqual(header[4], 3)
        strings = payload[header[8] : header[8] + header[9]]
        first_object = CASSET_OBJECT.unpack_from(payload, header[5])
        third_object = CASSET_OBJECT.unpack_from(payload, header[5] + 2 * CASSET_OBJECT.size)
        component = CASSET_COMPONENT.unpack_from(payload, header[7])
        self.assertEqual(strings[first_object[0] : first_object[0] + first_object[1]], b"ARM_Hero")
        self.assertEqual(first_object[2], 3)
        self.assertEqual(first_object[11], 20.0)
        self.assertEqual(first_object[15], -10.0)
        self.assertEqual(first_object[19], 30.0)
        self.assertEqual(strings[third_object[4] : third_object[4] + third_object[5]], b"ARM_Hero")
        self.assertEqual(component[0], 1)
        self.assertEqual(strings[component[1] : component[1] + component[2]], b"compiled/hero/meshes/SM_Body.cmesh")

    def test_blender_to_engine_matrix_rows_maps_to_x_forward_y_left_z_up(self) -> None:
        rows = blender_to_engine_matrix_rows(
            [
                [1.0, 0.0, 0.0, 10.0],
                [0.0, 1.0, 0.0, 20.0],
                [0.0, 0.0, 1.0, 30.0],
                [0.0, 0.0, 0.0, 1.0],
            ]
        )

        self.assertEqual(rows[3], 20.0)
        self.assertEqual(rows[7], -10.0)
        self.assertEqual(rows[11], 30.0)

    def test_bundle_relative_path_removes_casset_directory_prefix(self) -> None:
        bundle = Path("/repo/assets/compiled/statue")

        self.assertEqual(
            bundle_relative_path("/repo/assets/compiled/statue/meshes/Statue.cmesh", bundle),
            "meshes/Statue.cmesh",
        )
        self.assertEqual(
            bundle_relative_path("assets/compiled/statue/materials/stone.cmat", bundle),
            "materials/stone.cmat",
        )
        self.assertEqual(bundle_relative_path("meshes/Statue.cmesh", bundle), "meshes/Statue.cmesh")

    def test_collection_export_writes_common_prefab_asset_directly(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "hero.blend"
            output_root = root / "compiled"
            source.write_bytes(b"blend")
            collection = FakeCollection(
                "PREFAB_Hero",
                [FakeObject("SM_Body", "MESH"), FakeObject("ARM_Hero", "ARMATURE")],
            )

            outputs = write_collection_assets(source, output_root, [collection])

            self.assertEqual(outputs, [output_root / "hero" / "Hero.casset"])
            data = outputs[0].read_bytes()
            header = ASSET_HEADER.unpack_from(data)
            self.assertEqual(header[0], b"CEAF")
            self.assertEqual(header[3], int(AssetType.ASSET))

            payload = data[header[7] : header[7] + header[8]]
            casset = CASSET_HEADER.unpack_from(payload)
            strings = payload[casset[8] : casset[8] + casset[9]]
            self.assertEqual(strings[casset[12] : casset[12] + casset[13]], b"PREFAB_Hero")
            second_object = CASSET_OBJECT.unpack_from(payload, casset[5] + CASSET_OBJECT.size)
            self.assertEqual(second_object[2], 1)

    def test_collection_export_writes_component_paths(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "hero.blend"
            output_root = root / "compiled"
            mesh = output_root / "hero" / "meshes" / "SM_Body.cmesh"
            source.write_bytes(b"blend")
            mesh.parent.mkdir(parents=True)
            mesh.write_bytes(b"mesh")
            body = FakeObject("SM_Body", "MESH")
            collection = FakeCollection("PREFAB_Hero", [body])

            outputs = write_collection_assets(
                source,
                output_root,
                [collection],
                lambda obj: {"mesh": mesh.as_posix()} if obj.name == "SM_Body" else {},
            )

            data = outputs[0].read_bytes()
            header = ASSET_HEADER.unpack_from(data)
            payload = data[header[7] : header[7] + header[8]]
            casset = CASSET_HEADER.unpack_from(payload)
            strings = payload[casset[8] : casset[8] + casset[9]]
            component = CASSET_COMPONENT.unpack_from(payload, casset[7])
            self.assertEqual(strings[component[1] : component[1] + component[2]], b"meshes/SM_Body.cmesh")


if __name__ == "__main__":
    unittest.main()
