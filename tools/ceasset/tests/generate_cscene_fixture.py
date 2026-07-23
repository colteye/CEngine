from __future__ import annotations

import argparse
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from ceassetlib.assetfile import make_asset_desc, write_binary_asset
from ceassetlib.formats import AssetType
from ceassetlib.game_schema import load_bundled_game, make_schema_entity
from ceassetlib.scene_export import (
    AssetReference, EntityConnection, EntityDescription, LightEntity, Prop,
    SceneDescription, Transform, write_scene,
)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    mesh_desc = make_asset_desc(AssetType.MESH, "tests/python_fixture.cmesh", 0, b"fixture")
    write_binary_asset(args.output.parent / "python_fixture.cmesh", mesh_desc)
    material_desc = make_asset_desc(AssetType.MATERIAL, "tests/python_fixture.cmat", 0, b"fixture")
    write_binary_asset(args.output.parent / "python_fixture.cmat", material_desc)
    entity = EntityDescription(
        data=LightEntity(Transform(position=(1.0, 2.0, 3.0))),
        name="PythonFixture",
    )
    target = EntityDescription(data=LightEntity(), name="Target")
    prop = EntityDescription(data=Prop(
        AssetReference(AssetType.MESH, "python_fixture.cmesh", mesh_desc.guid),
        Transform(position=(4.0, 5.0, 6.0)),
        dynamic=True,
        collision_enabled=True,
        collision_half_extents=(1.0, 2.0, 3.0),
        mass=8.0,
    ), name="MovingProp")
    post_process = EntityDescription(data=make_schema_entity(
        load_bundled_game(), "post_process",
        bloom_threshold=1.75,
        exposure=1.25,
        ssao_radius=0.8,
    ), name="SceneLook")
    write_scene(args.output, SceneDescription(
        (entity, target, prop, post_process),
        connections=(EntityConnection(0, "OnReady", 1, "Enable"),)),
        "tests/python_fixture.cscene")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
