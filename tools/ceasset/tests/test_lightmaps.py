from __future__ import annotations

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ADDON = ROOT.parent / "blender_addon"
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ADDON))

from cengine_asset_exporter.lightmaps import (
    _combine_light_passes, _maximum_rgb, encode_rgbm, plan_atlas,
)


class LightmapTests(unittest.TestCase):
    def test_atlas_plan_is_deterministic_and_padded(self) -> None:
        first = plan_atlas(("Wall", "Floor", "Ceiling"), 64, 2)
        second = plan_atlas(("Ceiling", "Wall", "Floor"), 64, 2)

        self.assertEqual(first, second)
        self.assertEqual(first["Ceiling"].scale, (0.4375, 0.4375))
        self.assertEqual(first["Ceiling"].offset, (0.03125, 0.03125))
        self.assertEqual(first["Wall"].offset, (0.03125, 0.53125))

    def test_rgbm_roundtrip_preserves_hdr_color(self) -> None:
        encoded = encode_rgbm((4.0, 2.0, 1.0, 1.0), 8.0)
        multiplier = encoded[3] / 255.0 * 8.0
        decoded = tuple(channel / 255.0 * multiplier for channel in encoded[:3])

        self.assertAlmostEqual(decoded[0], 4.0, places=2)
        self.assertAlmostEqual(decoded[1], 2.0, places=2)
        self.assertAlmostEqual(decoded[2], 1.0, places=1)

    def test_indirect_and_static_direct_passes_are_added_before_encoding(self) -> None:
        combined = _combine_light_passes(
            (0.5, 0.25, 0.0, 1.0),
            (1.0, 0.25, 0.125, 1.0),
        )

        self.assertEqual(combined, [1.5, 0.5, 0.125, 1.0])

    def test_maximum_rgb_ignores_alpha(self) -> None:
        self.assertEqual(_maximum_rgb((0.25, 0.5, 0.125, 1.0, 2.0, 1.0, 0.0, 0.0)), 2.0)


if __name__ == "__main__":
    unittest.main()
