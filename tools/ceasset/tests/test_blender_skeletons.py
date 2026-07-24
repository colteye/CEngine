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

from cengine_asset_exporter.skeletons import (  # noqa: E402
    armature_objects,
    skeleton_output_path,
    skeleton_payload,
    write_skeleton_asset,
)
from ceassetlib.game_schema import load_bundled_game
from ceassetlib.wire import unpack_record


ASSET_HEADER = struct.Struct("<4sHHI16sQ16sQQQ")
GAME_SCHEMA = load_bundled_game()


class FakeBone:
    """TODO: Describe `FakeBone`."""

    def __init__(self, name: str, parent: "FakeBone | None" = None, matrix=None) -> None:
        """TODO: Describe `__init__`.

        Args:
            name: TODO: Describe this parameter.
            parent: TODO: Describe this parameter.
            matrix: TODO: Describe this parameter.
        """
        self.name = name
        self.parent = parent
        self.matrix_local = matrix


class FakeArmatureData:
    """TODO: Describe `FakeArmatureData`."""

    def __init__(self, bones: list[FakeBone]) -> None:
        """TODO: Describe `__init__`.

        Args:
            bones: TODO: Describe this parameter.
        """
        self.bones = bones


class FakeObject:
    """TODO: Describe `FakeObject`."""

    def __init__(self, name: str, obj_type: str, bones: list[FakeBone] | None = None) -> None:
        """TODO: Describe `__init__`.

        Args:
            name: TODO: Describe this parameter.
            obj_type: TODO: Describe this parameter.
            bones: TODO: Describe this parameter.
        """
        self.name = name
        self.type = obj_type
        self.data = FakeArmatureData(bones or [])


class BlenderSkeletonTests(unittest.TestCase):
    """TODO: Describe `BlenderSkeletonTests`."""

    def test_skeleton_output_path_is_sanitized_cskel(self) -> None:
        """TODO: Describe `test_skeleton_output_path_is_sanitized_cskel`."""
        output = skeleton_output_path(Path("hero.blend"), Path("compiled"), "ARM Hero")

        self.assertEqual(output, Path("compiled/hero/skeletons/ARM_Hero.cskel"))

    def test_armature_objects_are_unique_and_sorted(self) -> None:
        """TODO: Describe `test_armature_objects_are_unique_and_sorted`."""
        first = FakeObject("ARM_B", "ARMATURE")
        second = FakeObject("ARM_A", "ARMATURE")
        mesh = FakeObject("SM_Body", "MESH")

        armatures = armature_objects([first, mesh, second, first])

        self.assertEqual([obj.name for obj in armatures], ["ARM_A", "ARM_B"])

    def test_skeleton_payload_records_bone_hierarchy_and_matrices(self) -> None:
        """TODO: Describe `test_skeleton_payload_records_bone_hierarchy_and_matrices`."""
        root = FakeBone(
            "root",
            matrix=[
                [1.0, 0.0, 0.0, 1.0],
                [0.0, 1.0, 0.0, 2.0],
                [0.0, 0.0, 1.0, 3.0],
                [0.0, 0.0, 0.0, 1.0],
            ],
        )
        child = FakeBone("spine", root)
        armature = FakeObject("ARM_Hero", "ARMATURE", [root, child])

        payload = skeleton_payload(Path("hero.blend"), armature)

        decoded = unpack_record(GAME_SCHEMA, "skeleton", payload)
        self.assertEqual(decoded["name"], "ARM_Hero")
        self.assertEqual(len(decoded["bones"]), 2)
        self.assertEqual(decoded["bones"][0]["parent"], -1)
        self.assertEqual(decoded["bones"][0]["rest"]["translation"], [2.0, -1.0, 3.0])
        self.assertEqual(decoded["bones"][0]["joint_from_armature_bind"][3], -2.0)
        self.assertEqual(decoded["bones"][0]["name"], "root")
        self.assertEqual(decoded["bones"][1]["parent"], 0)
        self.assertEqual(decoded["bones"][1]["name"], "spine")

    def test_write_skeleton_asset_writes_common_cskel(self) -> None:
        """TODO: Describe `test_write_skeleton_asset_writes_common_cskel`."""
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            blend = root / "hero.blend"
            blend.write_bytes(b"blend")
            armature = FakeObject("ARM_Hero", "ARMATURE", [FakeBone("root")])

            export = write_skeleton_asset(blend, root / "compiled", armature)

            data = export.output.read_bytes()
            header = ASSET_HEADER.unpack_from(data)
            self.assertEqual(header[0], b"CEAF")
            self.assertEqual(header[3], 4)

            payload = data[header[7] : header[7] + header[8]]
            skeleton = unpack_record(GAME_SCHEMA, "skeleton", payload)
            self.assertEqual(skeleton["name"], "ARM_Hero")
            self.assertEqual(skeleton["bones"][0]["name"], "root")


if __name__ == "__main__":
    unittest.main()
