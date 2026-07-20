from __future__ import annotations

import struct
import sys
import tempfile
import unittest
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "blender_addon"))

from cengine_asset_exporter.skeletons import (  # noqa: E402
    SKELETON_BONE,
    SKELETON_HEADER,
    SKELETON_MAGIC,
    armature_objects,
    skeleton_output_path,
    skeleton_payload,
    write_skeleton_asset,
)


ASSET_HEADER = struct.Struct("<4sHHI16sQ16sQQQ")


class FakeBone:
    def __init__(self, name: str, parent: "FakeBone | None" = None, matrix=None) -> None:
        self.name = name
        self.parent = parent
        self.matrix_local = matrix


class FakeArmatureData:
    def __init__(self, bones: list[FakeBone]) -> None:
        self.bones = bones


class FakeObject:
    def __init__(self, name: str, obj_type: str, bones: list[FakeBone] | None = None) -> None:
        self.name = name
        self.type = obj_type
        self.data = FakeArmatureData(bones or [])


class BlenderSkeletonTests(unittest.TestCase):
    def test_skeleton_output_path_is_sanitized_cskel(self) -> None:
        output = skeleton_output_path(Path("hero.blend"), Path("compiled"), "ARM Hero")

        self.assertEqual(output, Path("compiled/hero/skeletons/ARM_Hero.cskel"))

    def test_armature_objects_are_unique_and_sorted(self) -> None:
        first = FakeObject("ARM_B", "ARMATURE")
        second = FakeObject("ARM_A", "ARMATURE")
        mesh = FakeObject("SM_Body", "MESH")

        armatures = armature_objects([first, mesh, second, first])

        self.assertEqual([obj.name for obj in armatures], ["ARM_A", "ARM_B"])

    def test_skeleton_payload_records_bone_hierarchy_and_matrices(self) -> None:
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

        header = SKELETON_HEADER.unpack_from(payload)
        self.assertEqual(header[0], SKELETON_MAGIC)
        self.assertEqual(header[3], 2)
        self.assertEqual(header[4], SKELETON_HEADER.size)
        root_row = SKELETON_BONE.unpack_from(payload, header[4])
        child_row = SKELETON_BONE.unpack_from(payload, header[4] + SKELETON_BONE.size)
        strings = payload[header[5] : header[5] + header[6]]
        self.assertEqual(root_row[0], -1)
        self.assertEqual(root_row[6], 1.0)
        self.assertEqual(strings[root_row[1] : root_row[1] + root_row[2]], b"root")
        self.assertEqual(child_row[0], 0)
        self.assertEqual(strings[child_row[1] : child_row[1] + child_row[2]], b"spine")

    def test_write_skeleton_asset_writes_common_cskel(self) -> None:
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
            skeleton = SKELETON_HEADER.unpack_from(payload)
            bone = SKELETON_BONE.unpack_from(payload, skeleton[4])
            strings = payload[skeleton[5] : skeleton[5] + skeleton[6]]
            self.assertEqual(strings[skeleton[7] : skeleton[7] + skeleton[8]], b"ARM_Hero")
            self.assertEqual(strings[bone[1] : bone[1] + bone[2]], b"root")


if __name__ == "__main__":
    unittest.main()
