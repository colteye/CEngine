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

import struct
import sys
import tempfile
import unittest
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "blender_addon"))

from cengine_asset_exporter.animations import (  # noqa: E402
    action_targets,
    animation_output_path,
    animation_payload,
    armature_actions,
    write_animation_asset,
)
from ceassetlib.game_schema import load_bundled_game
from ceassetlib.wire import unpack_record


ASSET_HEADER = struct.Struct("<4sHHI16sQ16sQQQ")
GAME_SCHEMA = load_bundled_game()


class FakeKeyframe:
    """TODO: Describe `FakeKeyframe`."""

    def __init__(self, frame: float, value: float, interpolation: str = "BEZIER") -> None:
        """TODO: Describe `__init__`.

        Args:
            frame: TODO: Describe this parameter.
            value: TODO: Describe this parameter.
            interpolation: TODO: Describe this parameter.
        """
        self.co = (frame, value)
        self.interpolation = interpolation


class FakeFcurve:
    """TODO: Describe `FakeFcurve`."""

    def __init__(self, data_path: str, array_index: int, keyframes: list[FakeKeyframe]) -> None:
        """TODO: Describe `__init__`.

        Args:
            data_path: TODO: Describe this parameter.
            array_index: TODO: Describe this parameter.
            keyframes: TODO: Describe this parameter.
        """
        self.data_path = data_path
        self.array_index = array_index
        self.keyframe_points = keyframes


class FakeAction:
    """TODO: Describe `FakeAction`."""

    def __init__(self, name: str) -> None:
        """TODO: Describe `__init__`.

        Args:
            name: TODO: Describe this parameter.
        """
        self.name = name
        self.frame_range = (1.0, 24.0)
        self.fcurves = [
            FakeFcurve('pose.bones["root"].location', 0, [FakeKeyframe(1.0, 0.0), FakeKeyframe(24.0, 1.0, "LINEAR")])
        ]


class FakeStrip:
    """TODO: Describe `FakeStrip`."""

    def __init__(self, action: FakeAction) -> None:
        """TODO: Describe `__init__`.

        Args:
            action: TODO: Describe this parameter.
        """
        self.action = action


class FakeTrack:
    """TODO: Describe `FakeTrack`."""

    def __init__(self, actions: list[FakeAction]) -> None:
        """TODO: Describe `__init__`.

        Args:
            actions: TODO: Describe this parameter.
        """
        self.strips = [FakeStrip(action) for action in actions]


class FakeAnimationData:
    """TODO: Describe `FakeAnimationData`."""

    def __init__(self, action: FakeAction | None, nla_actions: list[FakeAction] | None = None) -> None:
        """TODO: Describe `__init__`.

        Args:
            action: TODO: Describe this parameter.
            nla_actions: TODO: Describe this parameter.
        """
        self.action = action
        self.nla_tracks = [FakeTrack(nla_actions or [])]


class FakeObject:
    """TODO: Describe `FakeObject`."""

    def __init__(self, name: str, obj_type: str, animation_data: FakeAnimationData | None = None) -> None:
        """TODO: Describe `__init__`.

        Args:
            name: TODO: Describe this parameter.
            obj_type: TODO: Describe this parameter.
            animation_data: TODO: Describe this parameter.
        """
        self.name = name
        self.type = obj_type
        self.animation_data = animation_data


class BlenderAnimationTests(unittest.TestCase):
    """TODO: Describe `BlenderAnimationTests`."""

    def test_animation_output_path_uses_armature_and_action(self) -> None:
        """TODO: Describe `test_animation_output_path_uses_armature_and_action`."""
        output = animation_output_path(Path("hero.blend"), Path("compiled"), "ARM Hero", "Walk Forward")

        self.assertEqual(output, Path("compiled/hero/animations/ARM_Hero_Walk_Forward.canim"))

    def test_armature_actions_are_unique_and_sorted(self) -> None:
        """TODO: Describe `test_armature_actions_are_unique_and_sorted`."""
        walk = FakeAction("Walk")
        idle = FakeAction("Idle")
        armature = FakeObject("ARM_Hero", "ARMATURE", FakeAnimationData(walk, [idle, walk]))

        actions = armature_actions(armature)

        self.assertEqual([action.name for action in actions], ["Idle", "Walk"])

    def test_action_targets_use_exported_armatures_only(self) -> None:
        """TODO: Describe `test_action_targets_use_exported_armatures_only`."""
        action = FakeAction("Walk")
        armature = FakeObject("ARM_Hero", "ARMATURE", FakeAnimationData(action))
        mesh = FakeObject("SK_Body", "MESH", FakeAnimationData(FakeAction("MeshAction")))

        targets = action_targets([mesh, armature])

        self.assertEqual(targets, [(armature, action)])

    def test_animation_payload_records_fcurves_and_keyframes(self) -> None:
        """TODO: Describe `test_animation_payload_records_fcurves_and_keyframes`."""
        action = FakeAction("Walk")
        armature = FakeObject("ARM_Hero", "ARMATURE", FakeAnimationData(action))

        payload = animation_payload(
            Path("hero.blend"), armature, action, 24.0,
            "assets/compiled/hero/skeletons/ARM_Hero.cskel",
        )

        decoded = unpack_record(GAME_SCHEMA, "animation", payload)
        self.assertEqual(decoded["fps"], 24.0)
        self.assertEqual(decoded["start"], 1.0)
        self.assertEqual(decoded["name"], "Walk")
        self.assertEqual(decoded["skeleton"]["path"],
                         "assets/compiled/hero/skeletons/ARM_Hero.cskel")
        self.assertEqual(len(decoded["tracks"]), 1)
        self.assertEqual(decoded["tracks"][0]["path"],
                         'pose.bones["root"].location')
        self.assertEqual(decoded["tracks"][0]["keys"][1]["interpolation"], 2)

    def test_write_animation_asset_writes_common_canim(self) -> None:
        """TODO: Describe `test_write_animation_asset_writes_common_canim`."""
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            blend = root / "hero.blend"
            blend.write_bytes(b"blend")
            action = FakeAction("Walk")
            armature = FakeObject("ARM_Hero", "ARMATURE", FakeAnimationData(action))

            export = write_animation_asset(
                blend,
                root / "compiled",
                armature,
                action,
                24.0,
                lambda name: root / "compiled" / "hero" / "skeletons" / f"{name}.cskel",
            )

            data = export.output.read_bytes()
            header = ASSET_HEADER.unpack_from(data)
            self.assertEqual(header[0], b"CEAF")
            self.assertEqual(header[3], 5)

            payload = data[header[7] : header[7] + header[8]]
            animation = unpack_record(GAME_SCHEMA, "animation", payload)
            self.assertEqual(animation["name"], "Walk")


if __name__ == "__main__":
    unittest.main()
