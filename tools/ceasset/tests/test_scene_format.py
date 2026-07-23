from __future__ import annotations

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from ceassetlib.formats import AssetType, asset_type_for_extension
from ceassetlib.scene_format import (
    ASSET_REFERENCE,
    ENTITY_CLASS_BLOCK,
    ENTITY_CONNECTION,
    SCENE_ENTITY,
    SCENE_HEADER,
    SCENE_SETTINGS,
    TRANSFORM,
)


class SceneFormatTests(unittest.TestCase):
    def test_cpp_and_python_fixed_layout_sizes_match(self) -> None:
        self.assertEqual(
            (
                SCENE_HEADER.size,
                SCENE_SETTINGS.size,
                ASSET_REFERENCE.size,
                SCENE_ENTITY.size,
                ENTITY_CLASS_BLOCK.size,
                ENTITY_CONNECTION.size,
                TRANSFORM.size,
            ),
            (96, 48, 32, 20, 52, 28, 40),
        )

    def test_scene_extension_has_a_distinct_asset_type(self) -> None:
        self.assertEqual(asset_type_for_extension(".cscene"), AssetType.SCENE)
        self.assertEqual(asset_type_for_extension(".casset"), AssetType.ASSET)


if __name__ == "__main__":
    unittest.main()
