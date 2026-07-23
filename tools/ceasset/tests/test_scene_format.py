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
    """TODO: Describe `SceneFormatTests`."""

    def test_cpp_and_python_fixed_layout_sizes_match(self) -> None:
        """TODO: Describe `test_cpp_and_python_fixed_layout_sizes_match`."""
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
        """TODO: Describe `test_scene_extension_has_a_distinct_asset_type`."""
        self.assertEqual(asset_type_for_extension(".cscene"), AssetType.SCENE)
        self.assertEqual(asset_type_for_extension(".casset"), AssetType.ASSET)


if __name__ == "__main__":
    unittest.main()
