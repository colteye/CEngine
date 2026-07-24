"""Environment-probe bake math tests."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ADDON = ROOT.parent / "blender_addon"
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ADDON))

from cengine_asset_exporter.environment_probes import direction_to_face_uv


class EnvironmentProbeTests(unittest.TestCase):
    """Verify deterministic cube capture projection."""

    def test_cardinal_directions_select_center_of_expected_face(self) -> None:
        directions = (
            ((1.0, 0.0, 0.0), 0),
            ((-1.0, 0.0, 0.0), 1),
            ((0.0, 1.0, 0.0), 2),
            ((0.0, -1.0, 0.0), 3),
            ((0.0, 0.0, 1.0), 4),
            ((0.0, 0.0, -1.0), 5),
        )
        for direction, expected_face in directions:
            face, u, v = direction_to_face_uv(direction)
            self.assertEqual(face, expected_face)
            self.assertAlmostEqual(u, 0.5)
            self.assertAlmostEqual(v, 0.5)


if __name__ == "__main__":
    unittest.main()
