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
    Reference, EntityConnection, EntityDescription,
    SceneDescription, Transform, write_scene,
)
from ceassetlib.texture import write_rgbexp32_dds
from ceassetlib.wire import pack_record
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
    game = load_bundled_game()
    vertex = {
        "position": (0.0, 0.0, 0.0),
        "normal": (0.0, 0.0, 1.0),
        "uv": (0.0, 0.0),
        "lightmap_uv": (0.0, 0.0),
        "joints": (0, 0, 0, 0),
        "weights": (0.0, 0.0, 0.0, 0.0),
    }
    mesh_payload = pack_record(game, "mesh", {
        "flags": 0,
        "bounds_min": (0.0, 0.0, 0.0),
        "bounds_max": (1.0, 1.0, 0.0),
        "lods": [
            {"screen_size": 1.0, "vertices": [
                vertex,
                {**vertex, "position": (1.0, 0.0, 0.0), "uv": (1.0, 0.0)},
                {**vertex, "position": (0.0, 1.0, 0.0), "uv": (0.0, 1.0)},
            ], "indices": (0, 1, 2)},
            {"screen_size": 0.25, "vertices": [
                vertex,
                {**vertex, "position": (1.0, 0.0, 0.0)},
                {**vertex, "position": (0.0, 1.0, 0.0)},
            ], "indices": (0, 1, 2)},
        ],
    })
    mesh_desc = make_asset_desc(
        AssetType.MESH, "python_fixture.cmesh", 0, mesh_payload)
    write_binary_asset(args.output.parent / "python_fixture.cmesh", mesh_desc)
    material_desc = make_asset_desc(
        AssetType.MATERIAL, "python_fixture.cmat", 0,
        pack_record(game, "material", {
            "name": "Fixture", "shader": 1, "render_mode": 0,
            "textures": [], "base_color": (1.0, 1.0, 1.0, 1.0),
            "metallic": 0.0, "roughness": 0.5, "ao": 1.0,
            "alpha_cutoff": 0.5,
        }))
    write_binary_asset(args.output.parent / "python_fixture.cmat", material_desc)
    physics_desc = make_asset_desc(
        AssetType.PHYSICS, "tests/python_fixture.cphys", 0,
        physics_payload(CollisionShape(
            shape_type=ShapeType.BOX,
            half_extents=(1.0, 2.0, 3.0))))
    write_binary_asset(args.output.parent / "python_fixture.cphys", physics_desc)
    skeleton_desc = make_asset_desc(
        AssetType.SKELETON, "python_fixture.cskel", 0,
        pack_record(game, "skeleton", {
            "name": "FixtureRig",
            "bones": [{
                "name": "Root", "parent": -1,
                "armature_from_bone": (
                    1.0, 0.0, 0.0, 0.0,
                    0.0, 1.0, 0.0, 0.0,
                    0.0, 0.0, 1.0, 0.0,
                    0.0, 0.0, 0.0, 1.0),
            }],
        }))
    write_binary_asset(args.output.parent / "python_fixture.cskel", skeleton_desc)
    skeleton_ref = {
        "guid": skeleton_desc.guid, "type": int(AssetType.SKELETON),
        "path": "python_fixture.cskel",
    }
    animation_desc = make_asset_desc(
        AssetType.ANIMATION, "python_fixture.canim", 0,
        pack_record(game, "animation", {
            "name": "Idle", "skeleton": skeleton_ref, "fps": 24.0,
            "start": 0.0, "end": 1.0,
            "tracks": [{"path": "Root.location", "component": 0, "keys": [
                {"frame": 0.0, "value": 0.0, "interpolation": 1},
                {"frame": 1.0, "value": 1.0, "interpolation": 1},
            ]}],
        }))
    write_binary_asset(args.output.parent / "python_fixture.canim", animation_desc)
    casset_desc = make_asset_desc(
        AssetType.ASSET, "python_fixture.casset", 0,
        pack_record(game, "casset", {
            "name": "Fixture",
            "objects": [{
                "name": "Root", "role": 0, "type": 0, "parent": -1,
                "first_component": 0, "component_count": 1,
                "world_from_local": (
                    1.0, 0.0, 0.0, 0.0,
                    0.0, 1.0, 0.0, 0.0,
                    0.0, 0.0, 1.0, 0.0,
                    0.0, 0.0, 0.0, 1.0),
            }],
            "components": [{"asset": {
                "guid": mesh_desc.guid, "type": int(AssetType.MESH),
                "path": "python_fixture.cmesh",
            }}],
        }))
    write_binary_asset(args.output.parent / "python_fixture.casset", casset_desc)
    particle_desc = make_asset_desc(
        AssetType.PARTICLE, "python_fixture.cparticle", 0,
        pack_record(game, "particle", {
            "name": "Smoke", "textures": [], "flags": 1, "blend_mode": 1,
            "rate": 10.0, "lifetime": (1.0, 2.0), "speed": (0.5, 1.0),
            "spread": 0.25, "gravity": (0.0, 0.0, -1.0),
            "size": (1.0, 0.0), "start_color": (1.0, 1.0, 1.0, 1.0),
            "end_color": (1.0, 1.0, 1.0, 0.0),
        }))
    write_binary_asset(args.output.parent / "python_fixture.cparticle", particle_desc)
    write_rgbexp32_dds(
        args.output.parent / "python_fixture.dds",
        2,
        2,
        bytes((255, 128, 64, 129)) * 4,
    )
    (args.output.parent / "python_fixture.ogg").write_bytes(b"OggSfixture")
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
        mesh=Reference(
            AssetType.MESH, "python_fixture.cmesh", mesh_desc.guid),
        collision=Reference(
            AssetType.PHYSICS, "python_fixture.cphys", physics_desc.guid),
        transform=Transform(position=(4.0, 5.0, 6.0)),
        motion=2,
        mass=8.0,
    ), name="MovingProp")
    anchor = EntityDescription(data=make_schema_entity(
        game, "prop",
        mesh=Reference(
            AssetType.MESH, "python_fixture.cmesh", mesh_desc.guid),
        collision=Reference(
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
