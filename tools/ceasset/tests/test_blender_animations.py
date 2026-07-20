from __future__ import annotations

import struct
import sys
import tempfile
import unittest
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "blender_addon"))

from cengine_asset_exporter.animations import (  # noqa: E402
    ANIMATION_HEADER,
    ANIMATION_KEYFRAME,
    ANIMATION_MAGIC,
    ANIMATION_TRACK,
    action_targets,
    animation_output_path,
    animation_payload,
    armature_actions,
    write_animation_asset,
)


ASSET_HEADER = struct.Struct("<4sHHI16sQ16sQQQ")


class FakeKeyframe:
    def __init__(self, frame: float, value: float, interpolation: str = "BEZIER") -> None:
        self.co = (frame, value)
        self.interpolation = interpolation


class FakeFcurve:
    def __init__(self, data_path: str, array_index: int, keyframes: list[FakeKeyframe]) -> None:
        self.data_path = data_path
        self.array_index = array_index
        self.keyframe_points = keyframes


class FakeAction:
    def __init__(self, name: str) -> None:
        self.name = name
        self.frame_range = (1.0, 24.0)
        self.fcurves = [
            FakeFcurve('pose.bones["root"].location', 0, [FakeKeyframe(1.0, 0.0), FakeKeyframe(24.0, 1.0, "LINEAR")])
        ]


class FakeStrip:
    def __init__(self, action: FakeAction) -> None:
        self.action = action


class FakeTrack:
    def __init__(self, actions: list[FakeAction]) -> None:
        self.strips = [FakeStrip(action) for action in actions]


class FakeAnimationData:
    def __init__(self, action: FakeAction | None, nla_actions: list[FakeAction] | None = None) -> None:
        self.action = action
        self.nla_tracks = [FakeTrack(nla_actions or [])]


class FakeObject:
    def __init__(self, name: str, obj_type: str, animation_data: FakeAnimationData | None = None) -> None:
        self.name = name
        self.type = obj_type
        self.animation_data = animation_data


class BlenderAnimationTests(unittest.TestCase):
    def test_animation_output_path_uses_armature_and_action(self) -> None:
        output = animation_output_path(Path("hero.blend"), Path("compiled"), "ARM Hero", "Walk Forward")

        self.assertEqual(output, Path("compiled/hero/animations/ARM_Hero_Walk_Forward.canim"))

    def test_armature_actions_are_unique_and_sorted(self) -> None:
        walk = FakeAction("Walk")
        idle = FakeAction("Idle")
        armature = FakeObject("ARM_Hero", "ARMATURE", FakeAnimationData(walk, [idle, walk]))

        actions = armature_actions(armature)

        self.assertEqual([action.name for action in actions], ["Idle", "Walk"])

    def test_action_targets_use_exported_armatures_only(self) -> None:
        action = FakeAction("Walk")
        armature = FakeObject("ARM_Hero", "ARMATURE", FakeAnimationData(action))
        mesh = FakeObject("SK_Body", "MESH", FakeAnimationData(FakeAction("MeshAction")))

        targets = action_targets([mesh, armature])

        self.assertEqual(targets, [(armature, action)])

    def test_animation_payload_records_fcurves_and_keyframes(self) -> None:
        action = FakeAction("Walk")
        armature = FakeObject("ARM_Hero", "ARMATURE", FakeAnimationData(action))

        payload = animation_payload(Path("hero.blend"), armature, action, 24.0)

        header = ANIMATION_HEADER.unpack_from(payload)
        self.assertEqual(header[0], ANIMATION_MAGIC)
        self.assertEqual(header[3], 24.0)
        self.assertEqual(header[4], 1.0)
        self.assertEqual(header[6], 1)
        track = ANIMATION_TRACK.unpack_from(payload, header[7])
        keyframe = ANIMATION_KEYFRAME.unpack_from(payload, header[8] + ANIMATION_KEYFRAME.size)
        strings = payload[header[9] : header[9] + header[10]]
        self.assertEqual(strings[header[13] : header[13] + header[14]], b"Walk")
        self.assertEqual(strings[header[15] : header[15] + header[16]], b"ARM_Hero")
        self.assertEqual(strings[track[0] : track[0] + track[1]], b'pose.bones["root"].location')
        self.assertEqual(keyframe[2], 2)

    def test_write_animation_asset_writes_common_canim(self) -> None:
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
            animation = ANIMATION_HEADER.unpack_from(payload)
            strings = payload[animation[9] : animation[9] + animation[10]]
            self.assertEqual(strings[animation[13] : animation[13] + animation[14]], b"Walk")


if __name__ == "__main__":
    unittest.main()
