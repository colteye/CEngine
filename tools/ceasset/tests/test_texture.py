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

import sys
import struct
import tempfile
import unittest
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from ceassetlib.texture import (
    convert_texture_to_dds, normalize_pillow_dds_format, write_rgbexp32_dds, write_rgba_dxt5,
)


class TextureTests(unittest.TestCase):
    """TODO: Describe `TextureTests`."""

    def test_pillow_dds_format_normalization_is_small_and_explicit(self) -> None:
        """TODO: Describe `test_pillow_dds_format_normalization_is_small_and_explicit`."""
        self.assertEqual(normalize_pillow_dds_format("BC1"), "DXT1")
        self.assertEqual(normalize_pillow_dds_format("bc3"), "DXT5")
        self.assertEqual(normalize_pillow_dds_format("BC5"), "BC5")
        with self.assertRaises(RuntimeError):
            normalize_pillow_dds_format("BC7")

    def test_builtin_dxt5_writer_emits_one_block_for_four_by_four_rgba(self) -> None:
        """TODO: Describe `test_builtin_dxt5_writer_emits_one_block_for_four_by_four_rgba`."""
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "lightmap.dds"
            write_rgba_dxt5(output, 4, 4, bytes((255, 128, 0, 255)) * 16)

            data = output.read_bytes()
            self.assertEqual(data[:4], b"DDS ")
            self.assertEqual(data[84:88], b"DXT5")
            self.assertEqual(struct.unpack_from("<I", data, 28)[0], 3)
            self.assertEqual(struct.unpack_from("<I", data, 8)[0] & 0x00020000, 0x00020000)
            self.assertEqual(struct.unpack_from("<I", data, 108)[0] & 0x00400008, 0x00400008)
            self.assertEqual(len(data), 128 + 16 * 3)

    def test_pillow_conversion_accepts_one_bit_source_images(self) -> None:
        """TODO: Describe `test_pillow_conversion_accepts_one_bit_source_images`."""
        try:
            from PIL import Image
        except ImportError as error:
            self.skipTest(str(error))

        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary) / "mask.png"
            output = Path(temporary) / "mask.dds"
            Image.new("1", (4, 4), 1).save(source)

            convert_texture_to_dds(source, output, "DXT5")

            data = output.read_bytes()
            self.assertEqual(data[:4], b"DDS ")
            self.assertEqual(struct.unpack_from("<I", data, 28)[0], 3)

    def test_builtin_rgbexp32_writer_emits_tagged_uncompressed_texture(self) -> None:
        """TODO: Describe `test_builtin_rgbexp32_writer_emits_tagged_uncompressed_texture`."""
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "lightmap.dds"
            pixels = bytes((255, 128, 64, 129, 0, 64, 32, 127))
            write_rgbexp32_dds(output, 2, 1, pixels)

            data = output.read_bytes()
            self.assertEqual(data[:4], b"DDS ")
            self.assertEqual(data[84:88], b"RGBE")
            self.assertEqual(struct.unpack_from("<I", data, 28)[0], 2)
            self.assertEqual(data[128:136], pixels)
            self.assertEqual(len(data), 128 + 8 + 4)


if __name__ == "__main__":
    unittest.main()
