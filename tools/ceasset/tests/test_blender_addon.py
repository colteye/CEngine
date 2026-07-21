from __future__ import annotations

import importlib.util
import ast
import sys
import tempfile
import types
import unittest
import zipfile
from pathlib import Path
from unittest.mock import patch


ADDON = Path(__file__).resolve().parents[2] / "blender_addon" / "cengine_asset_exporter" / "__init__.py"
BLENDER_ADDON_ROOT = ADDON.parents[1]
PACKAGE_ADDON = ADDON.parents[1] / "package_addon.py"
CEASSET_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(CEASSET_ROOT))
sys.path.insert(0, str(BLENDER_ADDON_ROOT))
SPEC = importlib.util.spec_from_file_location(
    "cengine_asset_exporter",
    ADDON,
    submodule_search_locations=[str(ADDON.parent)],
)
assert SPEC is not None and SPEC.loader is not None
addon = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = addon
SPEC.loader.exec_module(addon)

PACKAGE_SPEC = importlib.util.spec_from_file_location("package_addon", PACKAGE_ADDON)
assert PACKAGE_SPEC is not None and PACKAGE_SPEC.loader is not None
package_addon = importlib.util.module_from_spec(PACKAGE_SPEC)
PACKAGE_SPEC.loader.exec_module(package_addon)


class FakeImage:
    def __init__(self, filepath: Path | str, packed_data: bytes | None = None, name: str = "image.png") -> None:
        self.filepath = str(filepath)
        self.name = name
        self.file_format = "PNG"
        self.packed_file = types.SimpleNamespace(data=packed_data) if packed_data is not None else None


class FakeNode:
    def __init__(self, node_type: str, image: FakeImage | None = None) -> None:
        self.type = node_type
        self.image = image


class FakeSocket:
    def __init__(self, name: str) -> None:
        self.name = name


class FakeLink:
    def __init__(self, image: FakeImage, socket: str = "Base Color") -> None:
        self.from_node = FakeNode("TEX_IMAGE", image)
        self.to_node = FakeNode("BSDF_PRINCIPLED")
        self.to_socket = FakeSocket(socket)


class FakeNodeTree:
    def __init__(self, links: list[FakeLink]) -> None:
        self.links = links


class FakeMaterial:
    def __init__(self, name: str, images: list[FakeImage] | None = None) -> None:
        self.name = name
        self.node_tree = FakeNodeTree([FakeLink(image) for image in images or []])

    def get(self, _key: str, default: object = None) -> object:
        return default


class FakeMaterialSlot:
    def __init__(self, material: FakeMaterial | None) -> None:
        self.material = material


class FakeObject:
    def __init__(
        self,
        name: str,
        material_names: list[str] | None = None,
        materials: list[FakeMaterial] | None = None,
        obj_type: str = "MESH",
        hide_render: bool = False,
        hide_viewport: bool = False,
        visible: bool = True,
        children: list["FakeObject"] | None = None,
        instance_collection: "FakeCollection | None" = None,
    ) -> None:
        self.name = name
        self.type = obj_type
        if materials is None:
            materials = [FakeMaterial(name) for name in material_names or []]
        self.material_slots = [FakeMaterialSlot(material) for material in materials]
        self.users_collection: list[FakeCollection] = []
        self.hide_render = hide_render
        self.hide_viewport = hide_viewport
        self.visible = visible
        self.children = children or []
        self.instance_collection = instance_collection
        self.instance_type = "COLLECTION" if instance_collection is not None else "NONE"

    def get(self, _key: str, default: object = None) -> object:
        return default

    def visible_get(self) -> bool:
        return self.visible


class FakeCollection:
    def __init__(
        self,
        name: str,
        objects: list[FakeObject] | None = None,
        children: list["FakeCollection"] | None = None,
        hide_render: bool = False,
        hide_viewport: bool = False,
    ) -> None:
        self.name = name
        self.objects = objects or []
        self.children = children or []
        self.hide_render = hide_render
        self.hide_viewport = hide_viewport
        self.all_objects = self.objects + [obj for child in self.children for obj in child.all_objects]
        for obj in self.objects:
            obj.users_collection.append(self)

    def get(self, _key: str, default: object = None) -> object:
        return default


class BlenderAddonTests(unittest.TestCase):
    def tearDown(self) -> None:
        addon.bpy = None

    def test_addon_requires_blender_for_bpy_operations(self) -> None:
        with self.assertRaises(RuntimeError):
            addon.require_bpy()

    def test_project_root_is_found_from_assets_source_tree(self) -> None:
        source = Path("/repo/assets/source/characters/hero.blend")
        self.assertEqual(addon.project_root_for(source), Path("/repo"))

    def test_default_output_root_uses_compiled_asset_tree(self) -> None:
        source = Path("/repo/assets/source/characters/hero.blend")
        self.assertEqual(addon.default_output_root_for_source(source), Path("/repo/assets/compiled"))
        self.assertIsNone(addon.default_output_root_for_source(None))

    def test_operator_invoke_accepts_blender_event_argument(self) -> None:
        tree = ast.parse(ADDON.read_text())
        invoke = next(
            node
            for node in ast.walk(tree)
            if isinstance(node, ast.FunctionDef) and node.name == "invoke"
        )

        self.assertEqual([arg.arg for arg in invoke.args.args], ["self", "context", "event"])

    def test_blend_source_path_rejects_unsaved_file(self) -> None:
        addon.bpy = types.SimpleNamespace(data=types.SimpleNamespace(filepath=""))

        with self.assertRaisesRegex(RuntimeError, "save the Blender file"):
            addon.blend_source_path()

    def test_unsaved_file_can_export_selected_collection_casset(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            body = FakeObject("SM_Body")
            collection = FakeCollection("PREFAB_Statue", [body])
            addon.bpy = types.SimpleNamespace(
                data=types.SimpleNamespace(filepath="", collections=[collection]),
                context=types.SimpleNamespace(collection=collection, selected_objects=[]),
            )

            result = addon.export_current_file(Path(tmp))

            self.assertEqual(result.collections, 1)
            self.assertTrue((Path(tmp) / "Statue.casset").exists())

    def test_collection_only_export_does_not_require_saved_file(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            body = FakeObject("SM_Body")
            collection = FakeCollection("PREFAB_Statue", [body])
            addon.bpy = types.SimpleNamespace(
                data=types.SimpleNamespace(filepath="", collections=[collection]),
                context=types.SimpleNamespace(collection=collection, selected_objects=[]),
            )

            result = addon.run_export(Path(tmp), collection_only=True)

            self.assertEqual(result.collections, 1)
            self.assertTrue((Path(tmp) / "Statue.casset").exists())

    def test_collection_only_export_uses_default_compiled_root_for_saved_files(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "assets" / "source" / "statue" / "Statue.blend"
            source.parent.mkdir(parents=True)
            source.write_bytes(b"blend")
            body = FakeObject("SM_Body")
            collection = FakeCollection("PREFAB_Statue", [body])
            addon.bpy = types.SimpleNamespace(
                data=types.SimpleNamespace(filepath=str(source), collections=[collection]),
                context=types.SimpleNamespace(collection=collection, selected_objects=[]),
            )

            result = addon.run_export(None, collection_only=True)

            self.assertEqual(result.collections, 1)
            self.assertTrue((root / "assets" / "compiled" / "statue" / "Statue.casset").exists())

    def test_selected_plain_collection_exports_as_prefab_asset(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            body = FakeObject("SM_Body")
            collection = FakeCollection("Collection", [body])
            addon.bpy = types.SimpleNamespace(
                data=types.SimpleNamespace(filepath="", collections=[collection]),
                context=types.SimpleNamespace(collection=collection, selected_objects=[]),
            )

            result = addon.run_export(Path(tmp), collection_only=True)

            self.assertEqual(result.collections, 1)
            self.assertTrue((Path(tmp) / "Collection.casset").exists())

    def test_exported_collection_objects_skips_hidden_and_unrendered_data(self) -> None:
        visible = FakeObject("SM_Visible")
        hidden_render = FakeObject("SM_HiddenRender", hide_render=True)
        hidden_viewport = FakeObject("SM_HiddenViewport", hide_viewport=True)
        hidden_child = FakeCollection("Hidden Child", [FakeObject("SM_HiddenChild")], hide_render=True)
        visible_child = FakeCollection("Visible Child", [FakeObject("SM_VisibleChild")])
        root = FakeCollection(
            "PREFAB_Root",
            [visible, hidden_render, hidden_viewport],
            [hidden_child, visible_child],
        )

        objects = addon.exported_collection_objects([root])

        self.assertEqual([obj.name for obj in objects], ["SM_Visible", "SM_VisibleChild"])

    def test_exported_collection_objects_walks_nested_child_collections(self) -> None:
        deep_mesh = FakeObject("SM_Deep")
        mid = FakeCollection("Mid", children=[FakeCollection("Deep", [deep_mesh])])
        root = FakeCollection("PREFAB_Root", children=[mid])

        objects = addon.exported_collection_objects([root])

        self.assertEqual([obj.name for obj in objects], ["SM_Deep"])

    def test_exported_collection_objects_expands_collection_instance_containers(self) -> None:
        instanced_mesh = FakeObject("SM_Instanced")
        instanced_collection = FakeCollection("ContainerContents", [instanced_mesh])
        container = FakeObject("EMPTY_Container", obj_type="EMPTY", instance_collection=instanced_collection)
        root = FakeCollection("PREFAB_Root", [container])

        objects = addon.exported_collection_objects([root])

        self.assertEqual([obj.name for obj in objects], ["EMPTY_Container", "SM_Instanced"])

    def test_collection_only_export_needs_output_root_for_unsaved_files(self) -> None:
        collection = FakeCollection("PREFAB_Statue", [FakeObject("SM_Body")])
        addon.bpy = types.SimpleNamespace(
            data=types.SimpleNamespace(filepath="", collections=[collection]),
            context=types.SimpleNamespace(collection=collection, selected_objects=[]),
        )

        with self.assertRaisesRegex(RuntimeError, "Output Root"):
            addon.run_export(None, collection_only=True)

    def test_unsaved_file_needs_output_root_for_collection_export(self) -> None:
        addon.bpy = types.SimpleNamespace(
            data=types.SimpleNamespace(filepath="", collections=[]),
            context=types.SimpleNamespace(collection=None, selected_objects=[]),
        )

        with self.assertRaisesRegex(RuntimeError, "Output Root"):
            addon.export_current_file(None)

    def test_texture_output_path_stays_under_blend_folder(self) -> None:
        output = addon.texture_output_path(
            Path("/repo/assets/source/characters/hero.blend"),
            Path("/repo/assets/compiled"),
            Path("/repo/assets/source/textures/albedo.png"),
        )

        self.assertEqual(output, Path("/repo/assets/compiled/characters/hero/textures/albedo.dds"))

    def test_asset_path_for_project_is_project_relative(self) -> None:
        path = addon.asset_path_for_project(
            Path("/repo"),
            Path("/repo/assets/compiled/hero/meshes/SM_Body.cmesh"),
        )

        self.assertEqual(path, "assets/compiled/hero/meshes/SM_Body.cmesh")

    def test_export_summary_includes_counts_and_destination(self) -> None:
        summary = addon.export_summary(
            addon.ExportResult(collections=1, textures=2, materials=3, skeletons=4, meshes=5, animations=6),
            Path("/repo/assets/compiled"),
        )

        self.assertIn("1 .casset", summary)
        self.assertIn("5 .cmesh", summary)
        self.assertIn("3 .cmat", summary)
        self.assertIn("/repo/assets/compiled", summary)

    def test_export_textures_uses_blender_image_datablocks(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "assets" / "source" / "characters" / "hero.blend"
            image = root / "assets" / "source" / "textures" / "albedo.png"
            source.parent.mkdir(parents=True)
            image.parent.mkdir(parents=True)
            source.write_bytes(b"blend")
            image.write_bytes(b"png")

            addon.bpy = types.SimpleNamespace(
                data=types.SimpleNamespace(images=[FakeImage(image), FakeImage(image)]),
                path=types.SimpleNamespace(abspath=lambda value: value),
            )

            with patch.object(addon, "convert_texture_to_dds") as convert:
                outputs = addon.export_textures(source, root / "assets" / "compiled", "DXT5")

            self.assertEqual(
                outputs,
                [
                    addon.TextureExport(
                        image,
                        root / "assets" / "compiled" / "characters" / "hero" / "textures" / "albedo.dds",
                    )
                ],
            )
            convert.assert_called_once_with(image, outputs[0].output, "DXT5")

    def test_packed_blender_image_is_written_as_texture_source(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "assets" / "source" / "hero" / "Hero.blend"
            output_root = root / "assets" / "compiled"
            source.parent.mkdir(parents=True)
            source.write_bytes(b"blend")
            image = FakeImage("", b"packed-png", "Packed Albedo")
            addon.bpy = types.SimpleNamespace(
                data=types.SimpleNamespace(filepath=str(source)),
                path=types.SimpleNamespace(abspath=lambda value: value),
            )

            packed_source = addon.image_source_path_for_export(image, source, output_root, {})

            self.assertIsNotNone(packed_source)
            assert packed_source is not None
            self.assertEqual(packed_source.read_bytes(), b"packed-png")
            self.assertEqual(packed_source.suffix, ".png")

    def test_material_images_only_uses_selected_collection_material_graphs(self) -> None:
        used_image = FakeImage("used.png")
        orphan_image = FakeImage("orphan.png")
        body = FakeObject("SM_Body", materials=[FakeMaterial("Body", [used_image])])
        orphan = FakeObject("SM_Orphan", materials=[FakeMaterial("Orphan", [orphan_image])])

        self.assertEqual(addon.material_images([body]), [used_image])
        self.assertEqual(addon.material_images([orphan]), [orphan_image])

    def test_saved_export_scopes_to_selected_collection_tree_and_skips_orphans(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "assets" / "source" / "hero" / "Hero.blend"
            used_image_path = root / "assets" / "source" / "hero" / "albedo.png"
            orphan_image_path = root / "assets" / "source" / "hero" / "orphan.png"
            source.parent.mkdir(parents=True)
            source.write_bytes(b"blend")
            used_image_path.write_bytes(b"png")
            orphan_image_path.write_bytes(b"png")

            used_image = FakeImage(used_image_path)
            orphan_image = FakeImage(orphan_image_path)
            material = FakeMaterial("HeroSkin", [used_image])
            body = FakeObject("SM_Body", materials=[material])
            selected = FakeCollection("PREFAB_Hero", [body])
            orphan = FakeCollection(
                "PREFAB_Orphan",
                [FakeObject("SM_Orphan", materials=[FakeMaterial("Orphan", [orphan_image])])],
            )
            addon.bpy = types.SimpleNamespace(
                data=types.SimpleNamespace(filepath=str(source), collections=[selected, orphan], images=[used_image, orphan_image]),
                context=types.SimpleNamespace(collection=selected, selected_objects=[]),
                path=types.SimpleNamespace(abspath=lambda value: value),
            )

            with (
                patch.object(addon, "convert_texture_to_dds") as convert,
                patch.object(addon, "write_material_assets") as write_materials,
                patch.object(addon, "write_skeleton_assets", return_value=[]),
                patch.object(addon, "write_animation_assets", return_value=[]),
                patch.object(addon, "bake_scene_lightmaps", return_value=({}, {}, [])),
                patch.object(addon, "write_mesh_assets") as write_meshes,
                patch.object(addon, "register_outputs"),
            ):
                write_materials.return_value = [
                    types.SimpleNamespace(
                        source=material,
                        output=root / "assets" / "compiled" / "hero" / "Hero" / "materials" / "HeroSkin.cmat",
                    )
                ]
                write_meshes.return_value = [
                    types.SimpleNamespace(
                        source=body,
                        output=root / "assets" / "compiled" / "hero" / "Hero" / "meshes" / "SM_Body.cmesh",
                        material_name="HeroSkin",
                    )
                ]

                result = addon.export_current_file(root / "assets" / "compiled")

            self.assertEqual(result.collections, 1)
            self.assertEqual(result.textures, 1)
            self.assertEqual(write_materials.call_args.args[2], [body])
            self.assertEqual(write_meshes.call_args.args[2], [body])
            convert.assert_called_once()
            self.assertEqual(convert.call_args.args[0], used_image_path)
            casset = root / "assets" / "compiled" / "hero" / "Hero.casset"
            self.assertTrue(casset.exists())
            self.assertIn(b"SM_Body", casset.read_bytes())
            self.assertNotIn(b"SM_Orphan", casset.read_bytes())

    def test_scene_collection_writes_cscene_after_generating_referenced_casset(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "assets" / "source" / "maps" / "Test.blend"
            source.parent.mkdir(parents=True)
            source.write_bytes(b"blend")
            placed_mesh = FakeObject("SM_Floor")
            prefab_mesh = FakeObject("SM_Door")
            prefab = FakeCollection("PREFAB_Door", [prefab_mesh])
            instance = FakeObject("Door", obj_type="EMPTY", instance_collection=prefab)
            scene = FakeCollection("SCENE_Test", [placed_mesh, instance])
            addon.bpy = types.SimpleNamespace(
                data=types.SimpleNamespace(filepath=str(source), collections=[scene, prefab], images=[]),
                context=types.SimpleNamespace(collection=scene, selected_objects=[]),
                path=types.SimpleNamespace(abspath=lambda value: value),
            )
            compiled = root / "assets" / "compiled"

            with (
                patch.object(addon, "write_material_assets") as write_materials,
                patch.object(addon, "write_skeleton_assets", return_value=[]),
                patch.object(addon, "write_animation_assets", return_value=[]),
                patch.object(addon, "bake_scene_lightmaps", return_value=({}, {}, [])),
                patch.object(addon, "write_mesh_assets") as write_meshes,
            ):
                write_materials.return_value = [
                    types.SimpleNamespace(
                        source=types.SimpleNamespace(name="SM_Floor_DefaultMaterial"),
                        output=compiled / "maps" / "materials" / "SM_Floor_DefaultMaterial.cmat"),
                    types.SimpleNamespace(
                        source=types.SimpleNamespace(name="SM_Door_DefaultMaterial"),
                        output=compiled / "maps" / "materials" / "SM_Door_DefaultMaterial.cmat"),
                ]
                write_meshes.return_value = [
                    types.SimpleNamespace(source=placed_mesh,
                        output=compiled / "maps" / "meshes" / "SM_Floor.cmesh",
                        material_name="SM_Floor_DefaultMaterial"),
                    types.SimpleNamespace(source=prefab_mesh,
                        output=compiled / "maps" / "meshes" / "SM_Door.cmesh",
                        material_name="SM_Door_DefaultMaterial"),
                ]
                result = addon.export_current_file(compiled)

            cscene = compiled / "maps" / "Test" / "Test.cscene"
            casset = compiled / "maps" / "Test" / "Door.casset"
            self.assertEqual(result.scenes, 1)
            self.assertEqual(result.collections, 1)
            self.assertTrue(cscene.exists())
            self.assertTrue(casset.exists())
            self.assertIn(b"assets/compiled/maps/Test/Door.casset", cscene.read_bytes())
            self.assertIn(b"assets/compiled/maps/meshes/SM_Floor.cmesh", cscene.read_bytes())

    def test_object_component_assets_reference_generated_targets(self) -> None:
        obj = FakeObject("SM_Body", ["HeroSkin"])
        mesh = Path("assets/compiled/hero/meshes/SM_Body.cmesh")
        material = Path("assets/compiled/hero/materials/HeroSkin.cmat")

        assets = addon.object_component_assets(
            obj,
            {"SM_Body": [(mesh, "HeroSkin")]},
            {"HeroSkin": material},
            {},
            {},
            lambda path: path.as_posix(),
        )

        self.assertEqual(
            assets,
            {
                "mesh": ["assets/compiled/hero/meshes/SM_Body.cmesh"],
                "materials": ["assets/compiled/hero/materials/HeroSkin.cmat"],
            },
        )

    def test_packaged_addon_contains_exporter_and_converter_library(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp) / "cengine_asset_exporter.zip"
            package_addon.write_addon_zip(output)

            with zipfile.ZipFile(output) as archive:
                names = set(archive.namelist())

            self.assertIn("__init__.py", names)
            self.assertIn("blender_manifest.toml", names)
            self.assertIn("meshes.py", names)
            self.assertIn("lightmaps.py", names)
            self.assertIn("ceassetlib/assetfile.py", names)
            self.assertIn("vendor/PIL/Image.py", names)
            self.assertIn("vendor/PIL/_imaging.cp311-win_amd64.pyd", names)
            self.assertNotIn("wheels/pillow-12.3.0-cp311-cp311-win_amd64.whl", names)
            self.assertNotIn("__pycache__/__init__.pyc", names)


if __name__ == "__main__":
    unittest.main()
