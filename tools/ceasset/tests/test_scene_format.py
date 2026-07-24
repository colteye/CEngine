#   _____ ______             _
#  / ____|  ____|           (_)
# | |    | |__   _ __   __ _ _ _ __   ___
# | |    |  __| | '_ \ / _` | | '_ \ / _ |
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
from ceassetlib.game_schema import load_bundled_game


class SceneFormatTests(unittest.TestCase):
    """TODO: Describe `SceneFormatTests`."""

    def test_scene_layout_is_owned_by_the_schema(self) -> None:
        game = load_bundled_game()
        scene = game.wire("scene")
        self.assertIsNotNone(scene)
        self.assertEqual(
            [field["name"] for field in scene["fields"]],
            ["settings", "assets", "entities", "classes", "connections"],
        )

    def test_scene_extension_has_a_distinct_asset_type(self) -> None:
        """TODO: Describe `test_scene_extension_has_a_distinct_asset_type`."""
        self.assertEqual(asset_type_for_extension(".cscene"), AssetType.SCENE)
        self.assertEqual(asset_type_for_extension(".casset"), AssetType.ASSET)


if __name__ == "__main__":
    unittest.main()
