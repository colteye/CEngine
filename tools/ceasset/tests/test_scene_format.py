from __future__ import annotations

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from ceassetlib.formats import AssetType, asset_type_for_extension
from ceassetlib.scene_format import FIXED_STRUCTURE_SIZES


class SceneFormatTests(unittest.TestCase):
    def test_cpp_and_python_fixed_layout_sizes_match(self) -> None:
        self.assertEqual(
            FIXED_STRUCTURE_SIZES,
            {
                "DiskSceneHeader": 96,
                "DiskSceneSettings": 48,
                "DiskAssetReference": 32,
                "DiskSceneEntity": 20,
                "DiskEntityClassBlock": 52,
                "DiskEntityConnection": 28,
                "DiskTransform": 40,
                "DiskProp": 96,
                "DiskPlayerEntity": 64,
                "DiskLightEntity": 88,
                "DiskPrefabEntity": 52,
                "DiskPrefabLightmap": 28,
                "DiskTriggerEntity": 56,
                "DiskPlayerStart": 44,
                "DiskSkyboxEntity": 56,
                "DiskExponentialHeightFogEntity": 76,
            },
        )

    def test_scene_extension_has_a_distinct_asset_type(self) -> None:
        self.assertEqual(asset_type_for_extension(".cscene"), AssetType.SCENE)
        self.assertEqual(asset_type_for_extension(".casset"), AssetType.ASSET)


if __name__ == "__main__":
    unittest.main()
