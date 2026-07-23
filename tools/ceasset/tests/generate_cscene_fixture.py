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

import argparse
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT.parent / "blender_addon"))

from ceassetlib.assetfile import make_asset_desc, write_binary_asset
from ceassetlib.formats import AssetType
from ceassetlib.game_schema import load_bundled_game, make_schema_entity
from ceassetlib.scene_export import (
    AssetReference, EntityConnection, EntityDescription,
    SceneDescription, Transform, write_scene,
)
from cengine_asset_exporter.physics import (
    CollisionShape, ShapeType, physics_payload,
)


def main() -> int:
    """TODO: Describe `main`.

    Returns:
        TODO: Describe the produced value.
    """
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    mesh_desc = make_asset_desc(AssetType.MESH, "tests/python_fixture.cmesh", 0, b"fixture")
    write_binary_asset(args.output.parent / "python_fixture.cmesh", mesh_desc)
    material_desc = make_asset_desc(AssetType.MATERIAL, "tests/python_fixture.cmat", 0, b"fixture")
    write_binary_asset(args.output.parent / "python_fixture.cmat", material_desc)
    physics_desc = make_asset_desc(
        AssetType.PHYSICS, "tests/python_fixture.cphys", 0,
        physics_payload(CollisionShape(
            shape_type=ShapeType.BOX,
            half_extents=(1.0, 2.0, 3.0))))
    write_binary_asset(args.output.parent / "python_fixture.cphys", physics_desc)
    game = load_bundled_game()
    entity = EntityDescription(
        data=make_schema_entity(
            game, "light",
            transform=Transform(position=(1.0, 2.0, 3.0))),
        name="PythonFixture",
    )
    target = EntityDescription(data=make_schema_entity(
        game, "light", transform=Transform()), name="Target")
    prop = EntityDescription(data=make_schema_entity(
        game, "prop",
        mesh=AssetReference(
            AssetType.MESH, "python_fixture.cmesh", mesh_desc.guid),
        collision=AssetReference(
            AssetType.PHYSICS, "python_fixture.cphys", physics_desc.guid),
        transform=Transform(position=(4.0, 5.0, 6.0)),
        motion=2,
        mass=8.0,
    ), name="MovingProp")
    anchor = EntityDescription(data=make_schema_entity(
        game, "prop",
        mesh=AssetReference(
            AssetType.MESH, "python_fixture.cmesh", mesh_desc.guid),
        collision=AssetReference(
            AssetType.PHYSICS, "python_fixture.cphys", physics_desc.guid),
        transform=Transform(position=(4.0, 5.0, 3.0)),
        motion=1,
    ), name="StaticAnchor")
    constraint = EntityDescription(data=make_schema_entity(
        game, "physics_constraint",
        transform=Transform(position=(4.0, 5.0, 4.5)),
        first_entity=2,
        second_entity=3,
        type=0,
    ), name="FixtureConstraint")
    post_process = EntityDescription(data=make_schema_entity(
        game, "post_process",
        bloom_threshold=1.75,
        exposure=1.25,
        ssao_radius=0.8,
    ), name="SceneLook")
    write_scene(args.output, SceneDescription(
        (entity, target, prop, anchor, constraint, post_process),
        connections=(EntityConnection(0, "OnReady", 1, "Enable"),)),
        "tests/python_fixture.cscene")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
