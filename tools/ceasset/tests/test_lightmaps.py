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

import math
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ADDON = ROOT.parent / "blender_addon"
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ADDON))

from cengine_asset_exporter.lightmaps import (
    ALPHA_CLIP_ENABLED, DEFAULT_INCLUDE_DIRECT, DEFAULT_PADDING,
    DEFAULT_RESOLUTION, DEFAULT_SAMPLES, INDIRECT_BAKE_LIGHT_MODES,
    LIGHTMAP_PACK_MARGIN, NORMAL_SEAM_DOT_THRESHOLD, _bake_targets,
    _maximum_rgb, _static_meshes, _trimmed_density_ratio, encode_rgbexp32,
    encode_rgbm, plan_atlas,
)


class FakeMeshObject:
    """TODO: Describe `FakeMeshObject`."""

    def __init__(self, name: str, role: str = "") -> None:
        """TODO: Describe `__init__`.

        Args:
            name: TODO: Describe this parameter.
            role: TODO: Describe this parameter.
        """
        self.name = name
        self.type = "MESH"
        self.matrix_world = object()
        self.properties = {"ce_role": role} if role else {}

    def get(self, key: str, default: object = None) -> object:
        """TODO: Describe `get`.

        Args:
            key: TODO: Describe this parameter.
            default: TODO: Describe this parameter.

        Returns:
            TODO: Describe the produced value.
        """
        return self.properties.get(key, default)


class LightmapTests(unittest.TestCase):
    """TODO: Describe `LightmapTests`."""

    def test_atlas_plan_is_deterministic_and_padded(self) -> None:
        """TODO: Describe `test_atlas_plan_is_deterministic_and_padded`."""
        first = plan_atlas(("Wall", "Floor", "Ceiling"), 64, 2)
        second = plan_atlas(("Ceiling", "Wall", "Floor"), 64, 2)

        self.assertEqual(first, second)
        self.assertEqual(first["Ceiling"].scale, (0.4375, 0.4375))
        self.assertEqual(first["Ceiling"].offset, (0.03125, 0.03125))
        self.assertEqual(first["Wall"].offset, (0.03125, 0.53125))

    def test_single_bake_target_uses_the_full_lightmap(self) -> None:
        """TODO: Describe `test_single_bake_target_uses_the_full_lightmap`."""
        tile = plan_atlas(("Sponza",), 2048, 64)["Sponza"]

        self.assertEqual(tile.scale, (1.0, 1.0))
        self.assertEqual(tile.offset, (0.0, 0.0))

    def test_rgbm_roundtrip_preserves_hdr_color(self) -> None:
        """TODO: Describe `test_rgbm_roundtrip_preserves_hdr_color`."""
        encoded = encode_rgbm((4.0, 2.0, 1.0, 1.0), 8.0)
        multiplier = encoded[3] / 255.0 * 8.0
        decoded = tuple(channel / 255.0 * multiplier for channel in encoded[:3])

        self.assertAlmostEqual(decoded[0], 4.0, places=2)
        self.assertAlmostEqual(decoded[1], 2.0, places=2)
        self.assertAlmostEqual(decoded[2], 1.0, places=1)

    def test_rgbexp32_roundtrip_preserves_hdr_color_without_blocks(self) -> None:
        """TODO: Describe `test_rgbexp32_roundtrip_preserves_hdr_color_without_blocks`."""
        encoded = encode_rgbexp32((4.0, 2.0, 1.0, 1.0))
        multiplier = math.ldexp(1.0, encoded[3] - 128)
        decoded = tuple(channel / 255.0 * multiplier for channel in encoded[:3])

        self.assertAlmostEqual(decoded[0], 4.0, delta=0.01)
        self.assertAlmostEqual(decoded[1], 2.0, delta=0.01)
        self.assertAlmostEqual(decoded[2], 1.0, delta=0.01)

    def test_light_modes_have_explicit_bake_semantics(self) -> None:
        """TODO: Describe `test_light_modes_have_explicit_bake_semantics`."""
        self.assertEqual(INDIRECT_BAKE_LIGHT_MODES, {"baked", "mixed"})
        self.assertNotIn("realtime", INDIRECT_BAKE_LIGHT_MODES)

    def test_offline_bake_quality_does_not_inherit_preview_defaults(self) -> None:
        """TODO: Describe `test_offline_bake_quality_does_not_inherit_preview_defaults`."""
        self.assertEqual(DEFAULT_RESOLUTION, 4096)
        self.assertEqual(DEFAULT_PADDING, 8)
        self.assertEqual(DEFAULT_SAMPLES, 1024)
        self.assertFalse(DEFAULT_INCLUDE_DIRECT)
        self.assertFalse(ALPHA_CLIP_ENABLED)

    def test_lightmap_pack_margin_is_fixed_for_bakes(self) -> None:
        """TODO: Describe `test_lightmap_pack_margin_is_fixed_for_bakes`."""
        self.assertEqual(LIGHTMAP_PACK_MARGIN, 0.0001)
        self.assertAlmostEqual(
            NORMAL_SEAM_DOT_THRESHOLD,
            math.cos(math.radians(45.0)),
        )

    def test_occluders_are_not_lightmap_unwrap_or_bake_targets(self) -> None:
        """TODO: Describe `test_occluders_are_not_lightmap_unwrap_or_bake_targets`."""
        wall = FakeMeshObject("SM_Wall")
        blocker = FakeMeshObject("OCC_LightBlocker", "occluder")

        self.assertEqual(_static_meshes((blocker, wall)), [wall])
        targets = _bake_targets((blocker, wall), {})
        self.assertEqual([target.source for target in targets], [wall])

    def test_density_ratio_trims_single_triangle_outliers(self) -> None:
        """TODO: Describe `test_density_ratio_trims_single_triangle_outliers`."""
        densities = [1.0] * 10 + [1000.0]

        self.assertEqual(_trimmed_density_ratio(densities), 1.0)

    def test_maximum_rgb_ignores_alpha(self) -> None:
        """TODO: Describe `test_maximum_rgb_ignores_alpha`."""
        self.assertEqual(_maximum_rgb((0.25, 0.5, 0.125, 1.0, 2.0, 1.0, 0.0, 0.0)), 2.0)

if __name__ == "__main__":
    unittest.main()
