from __future__ import annotations

import argparse
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from ceassetlib.assetfile import make_asset_desc, write_binary_asset
from ceassetlib.collection_export import CollectionExportSpec, collection_payload
from ceassetlib.formats import AssetType
from ceassetlib.scene_export import (
    AssetReference, EmptyEntity, EntityConnection, EntityDescription,
    PrefabEntity, Prop, SceneDescription, Transform, write_scene,
)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    mesh_desc = make_asset_desc(AssetType.MESH, "tests/python_fixture.cmesh", 0, b"fixture")
    write_binary_asset(args.output.parent / "python_fixture.cmesh", mesh_desc)
    material_desc = make_asset_desc(AssetType.MATERIAL, "tests/python_fixture.cmat", 0, b"fixture")
    write_binary_asset(args.output.parent / "python_fixture.cmat", material_desc)

    class FixtureObject:
        name = "PrefabMesh"
        type = "MESH"
        parent = None
        matrix_world = None

        @staticmethod
        def get(_key: str, default: object = None) -> object:
            return default

    prefab_payload = collection_payload(
        Path("python_fixture.blend"),
        CollectionExportSpec(AssetType.PREFAB, "python_fixture", "python_fixture"),
        [FixtureObject()],
        lambda _obj: {"mesh": "python_fixture.cmesh", "material": "python_fixture.cmat"},
        args.output.parent,
    )
    prefab_desc = make_asset_desc(AssetType.ASSET, "tests/python_fixture.casset", 0, prefab_payload)
    write_binary_asset(args.output.parent / "python_fixture.casset", prefab_desc)
    entity = EntityDescription(
        data=PrefabEntity(AssetReference(
            AssetType.ASSET, "python_fixture.casset", prefab_desc.guid),
            Transform(position=(1.0, 2.0, 3.0))),
        name="PythonFixture",
    )
    target = EntityDescription(data=EmptyEntity(), name="Target")
    prop = EntityDescription(data=Prop(
        AssetReference(AssetType.MESH, "python_fixture.cmesh", mesh_desc.guid),
        Transform(position=(4.0, 5.0, 6.0)),
        dynamic=True,
        collision_enabled=True,
        collision_half_extents=(1.0, 2.0, 3.0),
        mass=8.0,
    ), name="MovingProp")
    write_scene(args.output, SceneDescription(
        (entity, target, prop),
        connections=(EntityConnection(0, "OnReady", 1, "Enable"),)),
        "tests/python_fixture.cscene")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
