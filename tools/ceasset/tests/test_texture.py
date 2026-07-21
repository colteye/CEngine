from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from ceassetlib.texture import convert_texture_to_dds, normalize_pillow_dds_format, write_rgba_dxt5


class TextureTests(unittest.TestCase):
    def test_pillow_dds_format_normalization_is_small_and_explicit(self) -> None:
        self.assertEqual(normalize_pillow_dds_format("BC1"), "DXT1")
        self.assertEqual(normalize_pillow_dds_format("bc3"), "DXT5")
        self.assertEqual(normalize_pillow_dds_format("BC5"), "BC5")
        with self.assertRaises(RuntimeError):
            normalize_pillow_dds_format("BC7")

    def test_builtin_dxt5_writer_emits_one_block_for_four_by_four_rgba(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "lightmap.dds"
            write_rgba_dxt5(output, 4, 4, bytes((255, 128, 0, 255)) * 16)

            data = output.read_bytes()
            self.assertEqual(data[:4], b"DDS ")
            self.assertEqual(data[84:88], b"DXT5")
            self.assertEqual(len(data), 128 + 16)

    def test_pillow_conversion_accepts_one_bit_source_images(self) -> None:
        try:
            from PIL import Image
        except ImportError as error:
            self.skipTest(str(error))

        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary) / "mask.png"
            output = Path(temporary) / "mask.dds"
            Image.new("1", (4, 4), 1).save(source)

            convert_texture_to_dds(source, output, "DXT5")

            self.assertEqual(output.read_bytes()[:4], b"DDS ")


if __name__ == "__main__":
    unittest.main()
