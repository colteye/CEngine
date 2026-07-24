#   _____ ______             _
#  / ____|  ____|           (_)
# | |    | |__   _ __   __ _ _ _ __   ___
# | |    |  __| | '_ \ / _` | | '_ \ / _ |
# | |____| |____| | | | (_| | | | | |  __/
#  \_____|______|_| |_|\__, |_|_| |_|\___|
#                       __/ |
#                      |___/

"""Tests for schema-driven Blender particle export.

Author:
    Erik Coltey"""

from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "blender_addon"))

from ceassetlib.game_schema import load_bundled_game  # noqa: E402
from ceassetlib.wire import unpack_record  # noqa: E402
from cengine_asset_exporter.particles import (  # noqa: E402
    particle_payload,
    write_particle_assets,
)


class FakeParticle(dict):
    def __init__(self) -> None:
        super().__init__(
            ce_particle=True,
            ce_particle_texture="assets/compiled/fx/smoke.dds",
            ce_particle_start_color=(1.0, 0.5, 0.25, 1.0),
        )
        self.name = "Smoke"
        self.particle_systems = [SimpleNamespace(settings=SimpleNamespace(
            lifetime=2.0, lifetime_random=0.5, normal_factor=4.0,
            factor_random=0.25, count=60, frame_start=1, frame_end=31,
            particle_size=0.75,
        ))]


class ParticleTests(unittest.TestCase):
    game = load_bundled_game()

    def test_blender_settings_map_to_generated_particle(self) -> None:
        decoded = unpack_record(
            self.game, "particle", particle_payload(FakeParticle()))
        self.assertEqual(decoded["name"], "Smoke")
        self.assertEqual(decoded["lifetime"], [1.0, 2.0])
        self.assertEqual(decoded["speed"], [3.0, 4.0])
        self.assertEqual(decoded["rate"], 2.0)
        self.assertEqual(decoded["textures"][0]["path"],
                         "assets/compiled/fx/smoke.dds")

    def test_particle_asset_uses_common_envelope(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "Smoke.blend"
            source.write_bytes(b"blend")
            exports = write_particle_assets(
                source, root / "compiled", [FakeParticle()])
            data = exports[0].output.read_bytes()
            self.assertEqual(data[:4], b"CEAF")
            self.assertEqual(int.from_bytes(data[8:12], "little"), 15)


if __name__ == "__main__":
    unittest.main()
