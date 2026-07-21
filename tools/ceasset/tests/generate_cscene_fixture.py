from __future__ import annotations

import argparse
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from ceassetlib.assetfile import make_asset_desc, write_binary_asset
from ceassetlib.formats import AssetType
from ceassetlib.scene_export import (
    AssetReference, EmptyEntity, EntityConnection, EntityDescription,
    PrefabEntity, SceneDescription, Transform, write_scene,
)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    prefab_desc = make_asset_desc(AssetType.ASSET, "tests/python_fixture.casset", 0, b"fixture")
    write_binary_asset(args.output.parent / "python_fixture.casset", prefab_desc)
    entity = EntityDescription(
        data=PrefabEntity(AssetReference(
            AssetType.ASSET, "python_fixture.casset", prefab_desc.guid),
            Transform(position=(1.0, 2.0, 3.0))),
        name="PythonFixture",
    )
    target = EntityDescription(data=EmptyEntity(), name="Target")
    write_scene(args.output, SceneDescription(
        (entity, target),
        connections=(EntityConnection(0, "OnReady", 1, "Enable"),)),
        "tests/python_fixture.cscene")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
