from __future__ import annotations

import sys
import unittest
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from ceassetlib.texture import normalize_pillow_dds_format


class TextureTests(unittest.TestCase):
    def test_pillow_dds_format_normalization_is_small_and_explicit(self) -> None:
        self.assertEqual(normalize_pillow_dds_format("BC1"), "DXT1")
        self.assertEqual(normalize_pillow_dds_format("bc3"), "DXT5")
        self.assertEqual(normalize_pillow_dds_format("BC5"), "BC5")
        with self.assertRaises(RuntimeError):
            normalize_pillow_dds_format("BC7")


if __name__ == "__main__":
    unittest.main()
