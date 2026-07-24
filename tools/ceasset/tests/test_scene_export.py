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

from ceassetlib.formats import AssetType
from ceassetlib.game_schema import load_bundled_game, make_schema_entity
from ceassetlib.scene_export import (
    Reference,
    EntityConnection,
    EntityDescription,
    SceneDescription,
    SceneSettings,
    Transform,
    build_scene_payload,
)
from ceassetlib.wire import unpack_record


def guid(value: int) -> bytes:
    """TODO: Describe `guid`.

    Args:
        value: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    return bytes([value]) + bytes(15)


class SceneExportTests(unittest.TestCase):
    """TODO: Describe `SceneExportTests`."""

    game = load_bundled_game()

    def player(self):
        """TODO: Describe `player`.

        Returns:
            TODO: Describe the produced value.
        """
        return make_schema_entity(self.game, "player", transform=Transform())

    def entity(self, classname: str, **values):
        """TODO: Describe `entity`.

        Args:
            classname: TODO: Describe this parameter.
            **values: TODO: Describe this parameter.

        Returns:
            TODO: Describe the produced value.
        """
        return make_schema_entity(self.game, classname, **values)

    def make_scene(self) -> SceneDescription:
        """TODO: Describe `make_scene`.

        Returns:
            TODO: Describe the produced value.
        """
        mesh = Reference(AssetType.MESH, "assets/compiled/props/crate.cmesh", guid(1))
        material = Reference(AssetType.MATERIAL, "assets/compiled/props/crate.cmat", guid(2))
        lightmap = Reference(AssetType.TEXTURE, "assets/compiled/maps/test/lightmap_0.dds", guid(3))
        return SceneDescription(
            entities=(
                EntityDescription(
                    data=self.entity(
                        "light",
                        transform=Transform(position=(1.0, 2.0, 3.0)),
                        intensity=4.0),
                    name="KeyLight",
                ),
                EntityDescription(
                    data=self.entity(
                        "prop", transform=Transform(), mesh=mesh,
                        materials=(material,), lightmap=lightmap),
                    name="CrateA",
                ),
                EntityDescription(
                    data=self.player(),
                    name="Camera",
                ),
            ),
            settings=SceneSettings(active_entity=2),
            connections=(EntityConnection(0, "OnEnabled", 1, "Show", 0.25),),
        )

    def test_writer_is_deterministic_and_type_grouped(self) -> None:
        """TODO: Describe `test_writer_is_deterministic_and_type_grouped`."""
        scene = self.make_scene()
        first = build_scene_payload(scene)
        second = build_scene_payload(scene)
        self.assertEqual(first, second)

        decoded = unpack_record(self.game, "scene", first)
        self.assertEqual(len(decoded["assets"]), 3)
        self.assertEqual(len(decoded["entities"]), 3)
        self.assertEqual(len(decoded["classes"]), 3)
        self.assertEqual(len(decoded["connections"]), 1)
        self.assertEqual(
            sum(len(block["entities"]) for block in decoded["classes"]), 3)

    def test_invalid_descriptions_fail_before_writing(self) -> None:
        """TODO: Describe `test_invalid_descriptions_fail_before_writing`."""
        with self.assertRaisesRegex(ValueError, "project-relative"):
            build_scene_payload(SceneDescription((EntityDescription(
                self.entity(
                    "prop", transform=Transform(),
                    mesh=Reference(
                        AssetType.MESH, "../bad.cmesh", guid(9)))),)))
        with self.assertRaisesRegex(ValueError, "active entity index"):
            build_scene_payload(SceneDescription(
                (EntityDescription(self.player()),),
                SceneSettings(active_entity=4)))
        with self.assertRaisesRegex(ValueError, "must be enabled"):
            build_scene_payload(SceneDescription(
                (EntityDescription(self.entity(
                    "light", transform=Transform()), flags=0),),
                SceneSettings(active_entity=0)))
        with self.assertRaisesRegex(ValueError, "camera or player"):
            build_scene_payload(SceneDescription(
                (EntityDescription(self.entity(
                    "light", transform=Transform())),),
                SceneSettings(active_entity=0)))
        with self.assertRaisesRegex(ValueError, "connection entity index"):
            build_scene_payload(SceneDescription(
                (EntityDescription(self.player()),),
                connections=(EntityConnection(0, "OnEnabled", 2, "Enable"),)))
        mesh = Reference(AssetType.MESH, "assets/compiled/prop.cmesh", guid(10))
        lightmap = Reference(AssetType.TEXTURE, "assets/compiled/lightmap.dds", guid(11))
        collision = Reference(
            AssetType.PHYSICS, "assets/compiled/prop.cphys", guid(12))
        with self.assertRaisesRegex(ValueError, "movable prop"):
            build_scene_payload(SceneDescription((EntityDescription(
                self.entity(
                    "prop", transform=Transform(), mesh=mesh,
                    lightmap=lightmap, collision=collision, motion=2)),)))
        with self.assertRaisesRegex(ValueError, "below its minimum"):
            build_scene_payload(SceneDescription((EntityDescription(
                self.entity(
                    "prop", transform=Transform(), mesh=mesh,
                    collision=collision, motion=2, mass=0.0)),)))
        with self.assertRaisesRegex(ValueError, "both be set"):
            build_scene_payload(SceneDescription((EntityDescription(
                self.entity(
                    "prop", transform=Transform(), mesh=mesh,
                    collision=collision)),)))
        with self.assertRaisesRegex(ValueError, "below its minimum"):
            build_scene_payload(SceneDescription((EntityDescription(
                make_schema_entity(self.game, "post_process", exposure=-1.0)),)))
        with self.assertRaisesRegex(ValueError, "one enabled post-process"):
            build_scene_payload(SceneDescription((
                EntityDescription(make_schema_entity(self.game, "post_process")),
                EntityDescription(make_schema_entity(self.game, "post_process")),
            )))
        with self.assertRaisesRegex(ValueError, "entity reference is invalid"):
            build_scene_payload(SceneDescription((
                EntityDescription(self.entity(
                    "physics_constraint",
                    transform=Transform(),
                    first_entity=0,
                    second_entity=3)),
            )))


if __name__ == "__main__":
    unittest.main()
