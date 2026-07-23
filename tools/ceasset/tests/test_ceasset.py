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
SPEC = importlib.util.spec_from_file_location("ceasset_tool", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
ceasset = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = ceasset
SPEC.loader.exec_module(ceasset)


def quiet_call(function, *args, **kwargs):
    """TODO: Describe `quiet_call`.

    Args:
        function: TODO: Describe this parameter.
        *args: TODO: Describe this parameter.
        **kwargs: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    with redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
        return function(*args, **kwargs)


class CeassetTests(unittest.TestCase):
    """TODO: Describe `CeassetTests`."""

    def test_import_roundtrips_binary_registry(self) -> None:
        """TODO: Describe `test_import_roundtrips_binary_registry`."""
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
        """TODO: Describe `test_convert_copies_passthrough_target_asset_and_skips_incremental_build`."""
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
        """TODO: Describe `test_png_converts_to_dds_with_pillow`."""
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "assets" / "source" / "textures" / "albedo.png"
            source.parent.mkdir(parents=True)
            source.write_bytes(b"png payload")

            class FakeImage:
                """TODO: Describe `FakeImage`."""

                mode = "RGBA"

                def __init__(self, size: tuple[int, int] = (4, 4)) -> None:
                    self.size = size

                def __enter__(self):
                    """TODO: Describe `__enter__`.

                    Returns:
                        TODO: Describe the produced value.
                    """
                    return self

                def __exit__(self, _exc_type, _exc, _traceback):
                    """TODO: Describe `__exit__`.

                    Args:
                        _exc_type: TODO: Describe this parameter.
                        _exc: TODO: Describe this parameter.
                        _traceback: TODO: Describe this parameter.

                    Returns:
                        TODO: Describe the produced value.
                    """
                    return False

                def resize(self, size: tuple[int, int], _resampling: int):
                    return FakeImage(size)

                def close(self) -> None:
                    pass

                def save(self, output: object, format: str, pixel_format: str) -> None:
                    """TODO: Describe `save`.

                    Args:
                        output: TODO: Describe this parameter.
                        format: TODO: Describe this parameter.
                        pixel_format: TODO: Describe this parameter.
                    """
                    data = bytearray(144)
                    data[:4] = b"DDS "
                    data[84:88] = pixel_format.encode("ascii")
                    output.write(data)

            image_module = types.ModuleType("PIL.Image")
            image_module.open = lambda _source: FakeImage()
            image_module.Resampling = types.SimpleNamespace(LANCZOS=1)
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
        """TODO: Describe `test_png_is_rejected_until_a_real_conversion_path_exists`."""
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "assets" / "source" / "textures" / "albedo.png"
            source.parent.mkdir(parents=True)
            source.write_bytes(b"png payload")

            paths = ceasset.make_project_paths(root)
            self.assertEqual(quiet_call(ceasset.import_asset, paths, source), 1)
            self.assertFalse(paths.registry_path.exists())

    def test_blend_is_not_a_ceasset_cli_source_path(self) -> None:
        """TODO: Describe `test_blend_is_not_a_ceasset_cli_source_path`."""
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "assets" / "source" / "hero.blend"
            source.parent.mkdir(parents=True)
            source.write_bytes(b"blend")

            paths = ceasset.make_project_paths(root)
            self.assertEqual(quiet_call(ceasset.convert_source, paths, source), 1)
            self.assertFalse(paths.registry_path.exists())

if __name__ == "__main__":
    unittest.main()
