from __future__ import annotations

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from ceassetlib.formats import AssetType
from ceassetlib.scene_export import (
    AssetReference,
    CameraEntity,
    EntityConnection,
    EntityDescription,
    LightEntity,
    PrefabEntity, PrefabLightmap,
    SceneDescription,
    SceneSettings,
    Prop,
    Transform,
    build_scene_payload,
)
from ceassetlib.scene_format import (
    ASSET_REFERENCE,
    ENTITY_CLASS_BLOCK,
    SCENE_ENTITY,
    SCENE_HEADER,
)


def guid(value: int) -> bytes:
    return bytes([value]) + bytes(15)


class SceneExportTests(unittest.TestCase):
    def make_scene(self) -> SceneDescription:
        mesh = AssetReference(AssetType.MESH, "assets/compiled/props/crate.cmesh", guid(1))
        material = AssetReference(AssetType.MATERIAL, "assets/compiled/props/crate.cmat", guid(2))
        lightmap = AssetReference(AssetType.TEXTURE, "assets/compiled/maps/test/lightmap_0.dds", guid(3))
        prefab = AssetReference(AssetType.ASSET, "assets/compiled/props/statue.casset", guid(4))
        return SceneDescription(
            entities=(
                EntityDescription(
                    data=LightEntity(Transform(position=(1.0, 2.0, 3.0)), intensity=4.0),
                    name="KeyLight",
                ),
                EntityDescription(
                    data=Prop(mesh, materials=(material,), lightmap=lightmap),
                    name="CrateA",
                ),
                EntityDescription(
                    data=CameraEntity(),
                    name="Camera",
                ),
                EntityDescription(
                    data=PrefabEntity(prefab, lightmaps=(PrefabLightmap(2, lightmap),)),
                ),
            ),
            settings=SceneSettings(active_camera_entity=2),
            connections=(EntityConnection(0, "OnEnabled", 1, "Show", 0.25),),
        )

    def test_writer_is_deterministic_and_type_grouped(self) -> None:
        scene = self.make_scene()
        first = build_scene_payload(scene)
        second = build_scene_payload(scene)
        self.assertEqual(first, second)

        header = SCENE_HEADER.unpack_from(first)
        asset_offset, asset_count, asset_stride = header[4:7]
        entity_offset, entity_count, entity_stride = header[7:10]
        class_offset, class_count, class_stride = header[10:13]
        connection_offset, connection_count, connection_stride = header[13:16]
        self.assertEqual(asset_count, 4)
        self.assertEqual(asset_stride, ASSET_REFERENCE.size)
        self.assertEqual(entity_count, 4)
        self.assertEqual(entity_stride, SCENE_ENTITY.size)
        self.assertEqual(class_count, 4)
        self.assertEqual(class_stride, ENTITY_CLASS_BLOCK.size)
        self.assertNotEqual(connection_offset, 0)
        self.assertEqual(connection_count, 1)
        self.assertEqual(connection_stride, 28)

        blocks = [ENTITY_CLASS_BLOCK.unpack_from(first, class_offset + i * class_stride)
                  for i in range(class_count)]
        self.assertEqual(sum(block[4] for block in blocks), entity_count)

    def test_invalid_descriptions_fail_before_writing(self) -> None:
        with self.assertRaisesRegex(ValueError, "project-relative"):
            build_scene_payload(SceneDescription((EntityDescription(
                Prop(AssetReference(
                    AssetType.MESH, "../bad.cmesh", guid(9)))),)))
        with self.assertRaisesRegex(ValueError, "active camera entity index"):
            build_scene_payload(SceneDescription(
                (EntityDescription(CameraEntity()),),
                SceneSettings(active_camera_entity=4)))
        with self.assertRaisesRegex(ValueError, "must reference a camera"):
            build_scene_payload(SceneDescription(
                (EntityDescription(LightEntity()),),
                SceneSettings(active_camera_entity=0)))
        with self.assertRaisesRegex(ValueError, "connection entity index"):
            build_scene_payload(SceneDescription(
                (EntityDescription(CameraEntity()),),
                connections=(EntityConnection(0, "OnReady", 2, "Enable"),)))
        mesh = AssetReference(AssetType.MESH, "assets/compiled/prop.cmesh", guid(10))
        lightmap = AssetReference(AssetType.TEXTURE, "assets/compiled/lightmap.dds", guid(11))
        with self.assertRaisesRegex(ValueError, "only a static prop"):
            build_scene_payload(SceneDescription((EntityDescription(
                Prop(mesh, lightmap=lightmap, dynamic=True)),)))
        with self.assertRaisesRegex(ValueError, "dynamic prop mass"):
            build_scene_payload(SceneDescription((EntityDescription(
                Prop(mesh, dynamic=True, collision_enabled=True, mass=0.0)),)))
        prefab = AssetReference(AssetType.ASSET, "assets/compiled/test.casset", guid(12))
        with self.assertRaisesRegex(ValueError, "duplicated"):
            build_scene_payload(SceneDescription((EntityDescription(PrefabEntity(
                prefab, lightmaps=(PrefabLightmap(1, lightmap), PrefabLightmap(1, lightmap)))),)))


if __name__ == "__main__":
    unittest.main()
