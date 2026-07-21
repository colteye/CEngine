from __future__ import annotations

import importlib.util
import io
import sys
import tempfile
import types
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path
from unittest.mock import patch

SCRIPT = Path(__file__).resolve().parents[1] / "ceasset.py"
sys.path.insert(0, str(SCRIPT.parent))
from ceassetlib.assetfile import make_asset_desc, write_binary_asset
from ceassetlib.scene_export import (
    AssetReference, EntityDescription, PrefabEntity, SceneDescription, write_scene,
)
SPEC = importlib.util.spec_from_file_location("ceasset_tool", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
ceasset = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = ceasset
SPEC.loader.exec_module(ceasset)


def quiet_call(function, *args, **kwargs):
    with redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
        return function(*args, **kwargs)


class CeassetTests(unittest.TestCase):
    def test_import_roundtrips_binary_registry(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "assets" / "source" / "textures" / "albedo.dds"
            source.parent.mkdir(parents=True)
            source.write_bytes(b"DDS test")

            paths = ceasset.make_project_paths(root)
            self.assertEqual(quiet_call(ceasset.import_asset, paths, source), 0)

            records = ceasset.load_registry(paths)
            self.assertEqual(len(records), 1)
            self.assertEqual(records[0].asset_type, ceasset.AssetType.TEXTURE)
            self.assertEqual(records[0].source, Path("assets/source/textures/albedo.dds"))
            self.assertEqual(records[0].output, Path("assets/compiled/textures/albedo.dds"))

    def test_convert_copies_passthrough_target_asset_and_skips_incremental_build(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "assets" / "source" / "audio" / "voice.opus"
            source.parent.mkdir(parents=True)
            source.write_bytes(b"opus payload")

            paths = ceasset.make_project_paths(root)
            self.assertEqual(quiet_call(ceasset.convert_source, paths, source), 0)
            output = root / "assets" / "compiled" / "audio" / "voice.opus"
            self.assertEqual(output.read_bytes(), b"opus payload")

            self.assertEqual(quiet_call(ceasset.build, paths, ceasset.BuildOptions(force=False)), 0)
            cache = ceasset.load_cache(paths)
            self.assertEqual(len(cache), 1)

    def test_png_converts_to_dds_with_pillow(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "assets" / "source" / "textures" / "albedo.png"
            source.parent.mkdir(parents=True)
            source.write_bytes(b"png payload")

            class FakeImage:
                def __enter__(self):
                    return self

                def __exit__(self, _exc_type, _exc, _traceback):
                    return False

                def save(self, output: Path, format: str, pixel_format: str) -> None:
                    output.write_bytes(b"DDS " + format.encode("ascii") + b":" + pixel_format.encode("ascii"))

            image_module = types.ModuleType("PIL.Image")
            image_module.open = lambda _source: FakeImage()
            pil_module = types.ModuleType("PIL")
            pil_module.Image = image_module

            paths = ceasset.make_project_paths(root)
            with patch.dict(sys.modules, {"PIL": pil_module, "PIL.Image": image_module}):
                self.assertEqual(quiet_call(ceasset.convert_texture_source, paths, source, "DXT5"), 0)

            output = root / "assets" / "compiled" / "textures" / "albedo.dds"
            self.assertEqual(output.read_bytes()[:4], b"DDS ")

            records = ceasset.load_registry(paths)
            self.assertEqual(len(records), 1)
            self.assertEqual(records[0].output, Path("assets/compiled/textures/albedo.dds"))

    def test_png_is_rejected_until_a_real_conversion_path_exists(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "assets" / "source" / "textures" / "albedo.png"
            source.parent.mkdir(parents=True)
            source.write_bytes(b"png payload")

            paths = ceasset.make_project_paths(root)
            self.assertEqual(quiet_call(ceasset.import_asset, paths, source), 1)
            self.assertFalse(paths.registry_path.exists())

    def test_blend_is_not_a_ceasset_cli_source_path(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "assets" / "source" / "hero.blend"
            source.parent.mkdir(parents=True)
            source.write_bytes(b"blend")

            paths = ceasset.make_project_paths(root)
            self.assertEqual(quiet_call(ceasset.convert_source, paths, source), 1)
            self.assertFalse(paths.registry_path.exists())

    def test_inspect_and_validate_cscene_with_casset_reference(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            prefab_path = root / "assets" / "compiled" / "Door.casset"
            prefab_name = "assets/compiled/Door.casset"
            prefab = make_asset_desc(ceasset.AssetType.ASSET, prefab_name, 0, b"prefab")
            write_binary_asset(prefab_path, prefab)
            scene_path = root / "assets" / "compiled" / "Map.cscene"
            write_scene(scene_path, SceneDescription((EntityDescription(
                PrefabEntity(AssetReference(ceasset.AssetType.ASSET, prefab_name, prefab.guid))),)),
                "assets/compiled/Map.cscene")

            output = io.StringIO()
            with redirect_stdout(output):
                self.assertEqual(ceasset.main([
                    "--project", str(root), "inspect", "assets/compiled/Map.cscene"]), 0)
            self.assertIn("prefab_instance: 1", output.getvalue())
            self.assertIn(prefab_name, output.getvalue())
            self.assertEqual(quiet_call(ceasset.main,
                ["--project", str(root), "validate", "assets/compiled/Map.cscene"]), 0)

            prefab_path.unlink()
            self.assertEqual(quiet_call(ceasset.main,
                ["--project", str(root), "validate", "assets/compiled/Map.cscene"]), 1)


if __name__ == "__main__":
    unittest.main()
