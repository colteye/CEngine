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
    DEFAULT_INCLUDE_DIRECT, DEFAULT_INDIRECT_CLAMP, DEFAULT_PADDING,
    DEFAULT_RESOLUTION, DEFAULT_SAMPLES, DIRECT_BAKE_LIGHT_MODES,
    INDIRECT_BAKE_LIGHT_MODES, _apply_coverage_mask, _clamp_rgb, _combine_light_passes,
    LIGHTMAP_PACK_MARGIN, _bake_targets, _maximum_rgb, _static_meshes,
    _trimmed_density_ratio, encode_rgbexp32, encode_rgbm, plan_atlas,
)


class FakeMeshObject:
    def __init__(self, name: str, role: str = "") -> None:
        self.name = name
        self.type = "MESH"
        self.matrix_world = object()
        self.properties = {"ce_role": role} if role else {}

    def get(self, key: str, default: object = None) -> object:
        return self.properties.get(key, default)


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

    def test_rgbexp32_roundtrip_preserves_hdr_color_without_blocks(self) -> None:
        encoded = encode_rgbexp32((4.0, 2.0, 1.0, 1.0))
        multiplier = math.ldexp(1.0, encoded[3] - 128)
        decoded = tuple(channel / 255.0 * multiplier for channel in encoded[:3])

        self.assertAlmostEqual(decoded[0], 4.0, delta=0.01)
        self.assertAlmostEqual(decoded[1], 2.0, delta=0.01)
        self.assertAlmostEqual(decoded[2], 1.0, delta=0.01)

    def test_indirect_and_static_direct_passes_are_added_before_encoding(self) -> None:
        combined = _combine_light_passes(
            (0.5, 0.25, 0.0, 1.0),
            (1.0, 0.25, 0.125, 1.0),
        )

        self.assertEqual(combined, [1.5, 0.5, 0.125, 1.0])

    def test_light_modes_have_explicit_bake_semantics(self) -> None:
        self.assertEqual(INDIRECT_BAKE_LIGHT_MODES, {"baked", "mixed"})
        self.assertEqual(DIRECT_BAKE_LIGHT_MODES, {"baked"})
        self.assertNotIn("realtime", INDIRECT_BAKE_LIGHT_MODES)
        self.assertNotIn("mixed", DIRECT_BAKE_LIGHT_MODES)

    def test_offline_bake_quality_does_not_inherit_preview_defaults(self) -> None:
        self.assertEqual(DEFAULT_RESOLUTION, 4096)
        self.assertEqual(DEFAULT_PADDING, 64)
        self.assertEqual(DEFAULT_SAMPLES, 256)
        self.assertEqual(DEFAULT_INDIRECT_CLAMP, 2.0)
        self.assertFalse(DEFAULT_INCLUDE_DIRECT)

    def test_lightmap_pack_margin_is_fixed_for_bakes(self) -> None:
        self.assertEqual(LIGHTMAP_PACK_MARGIN, 0.001)

    def test_occluders_are_not_lightmap_unwrap_or_bake_targets(self) -> None:
        wall = FakeMeshObject("SM_Wall")
        blocker = FakeMeshObject("OCC_LightBlocker", "occluder")

        self.assertEqual(_static_meshes((blocker, wall)), [wall])
        targets = _bake_targets((blocker, wall), {})
        self.assertEqual([target.source for target in targets], [wall])

    def test_density_ratio_trims_single_triangle_outliers(self) -> None:
        densities = [1.0] * 10 + [1000.0]

        self.assertEqual(_trimmed_density_ratio(densities), 1.0)

    def test_maximum_rgb_ignores_alpha(self) -> None:
        self.assertEqual(_maximum_rgb((0.25, 0.5, 0.125, 1.0, 2.0, 1.0, 0.0, 0.0)), 2.0)

    def test_denoise_output_is_clamped_to_source_energy(self) -> None:
        pixels = [1.5, -0.25, 0.5, 1.0]

        self.assertEqual(_clamp_rgb(pixels, 1.0), [1.0, 0.0, 0.5, 1.0])

    def test_denoise_uses_coverage_instead_of_black_energy_as_chart_mask(self) -> None:
        denoised = [0.25, 0.5, 0.75, 0.2, 0.25, 0.5, 0.75, 0.2]
        coverage = [1.0, 1.0, 1.0, 1.0, 0.0, 0.0, 0.0, 1.0]

        self.assertEqual(_apply_coverage_mask(denoised, coverage), [
            0.25, 0.5, 0.75, 1.0,
            0.0, 0.0, 0.0, 1.0,
        ])


if __name__ == "__main__":
    unittest.main()
