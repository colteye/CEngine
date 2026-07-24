#!/usr/bin/env python3

"""Cook checked-in Sponza structure into one static collision asset.

The original Sponza source is not checked into this repository. This focused
content cooker reconstructs the collision-only payload from the checked-in
LOD-zero structural meshes, adds one collider entity to Sponza.cscene, and
places the player spawn origin on the floor.
"""

from __future__ import annotations

import argparse
import math
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "ceasset"))
sys.path.insert(0, str(ROOT / "tools" / "blender_addon"))

from ceassetlib.assetfile import (  # noqa: E402
    AssetWriteDesc,
    read_binary_asset,
    write_binary_asset,
)
from ceassetlib.formats import AssetType  # noqa: E402
from ceassetlib.game_schema import (  # noqa: E402
    entity_struct,
    load_bundled_game,
    make_schema_entity,
)
from ceassetlib.ids import guid_from_stable_name  # noqa: E402
from ceassetlib.scene_export import (  # noqa: E402
    Reference,
    Transform,
    _pack_schema_entity,
)
from ceassetlib.wire import pack_record, unpack_record  # noqa: E402
from cengine_asset_exporter.physics import (  # noqa: E402
    CollisionShape,
    ShapeType,
    physics_payload,
)


GAME = load_bundled_game()
SCENE_PATH = "assets/compiled/sponza/Sponza.cscene"
COLLISION_PATH = "assets/compiled/sponza/physics/SponzaCollision.cphys"
LEGACY_COLLISION_PATHS = {
    "assets/compiled/sponza/physics/Sponza.cphys",
}
COLLIDER_NAME = "SponzaCollision"
SPAWN_NAME = "PlayerSpawn"
SPAWN_FLOOR_HEIGHT = 0.0
COLLISION_CELL_SIZE = 0.75
STRUCTURAL_PROPS = {
    "sponza:arch",
    "sponza:bricks",
    "sponza:ceiling",
    "sponza:column_a",
    "sponza:column_b",
    "sponza:column_c",
    "sponza:details",
    "sponza:floor",
    "sponza:roof",
}


def _record_values(
    block: dict[str, object],
    schema: dict[str, object],
    row: int,
) -> list[int | float]:
    """Unpack one fixed-stride entity record."""
    layout = entity_struct(schema)
    records = bytes(block["records"])
    start = row * layout.size
    return list(layout.unpack(records[start:start + layout.size]))


def _replace_record(
    block: dict[str, object],
    schema: dict[str, object],
    row: int,
    values: list[int | float],
) -> None:
    """Replace one fixed-stride entity record in a decoded class block."""
    layout = entity_struct(schema)
    records = bytearray(block["records"])
    start = row * layout.size
    records[start:start + layout.size] = layout.pack(*values)
    block["records"] = bytes(records)


def _field_offset(schema: dict[str, object], field_name: str) -> int:
    """Return the scalar tuple offset of one entity field."""
    offset = 0
    counts = {"transform": 10, "vec2": 2, "vec3": 3, "asset_list": 2}
    for field in schema["fields"]:
        if field["name"] == field_name:
            return offset
        offset += counts.get(str(field["type"]), 1)
    raise ValueError(f"{schema['classname']} has no field {field_name}")


def _rotate(
    rotation: tuple[float, float, float, float],
    vector: tuple[float, float, float],
) -> tuple[float, float, float]:
    """Rotate a vector by an x/y/z/w quaternion."""
    x, y, z, w = rotation
    length = math.sqrt(x * x + y * y + z * z + w * w)
    if length <= 0.0:
        raise ValueError("Sponza prop rotation is invalid")
    x, y, z, w = (value / length for value in (x, y, z, w))
    vx, vy, vz = vector
    uv = (y * vz - z * vy, z * vx - x * vz, x * vy - y * vx)
    uuv = (
        y * uv[2] - z * uv[1],
        z * uv[0] - x * uv[2],
        x * uv[1] - y * uv[0],
    )
    return (
        vx + 2.0 * (w * uv[0] + uuv[0]),
        vy + 2.0 * (w * uv[1] + uuv[1]),
        vz + 2.0 * (w * uv[2] + uuv[2]),
    )


def _transform_position(
    transform: list[int | float],
    position: list[float],
) -> tuple[float, float, float]:
    """Apply an entity translation/rotation/scale to a mesh position."""
    translation = tuple(float(value) for value in transform[0:3])
    rotation = tuple(float(value) for value in transform[3:7])
    scale = tuple(float(value) for value in transform[7:10])
    if any(value <= 0.0 or not math.isfinite(value) for value in scale):
        raise ValueError("Sponza structural prop scale is invalid")
    scaled = tuple(float(position[index]) * scale[index] for index in range(3))
    rotated = _rotate(rotation, scaled)
    return tuple(rotated[index] + translation[index] for index in range(3))


def _triangle_area_squared(
    first: tuple[float, float, float],
    second: tuple[float, float, float],
    third: tuple[float, float, float],
) -> float:
    """Return four times a triangle's squared area."""
    ab = tuple(second[index] - first[index] for index in range(3))
    ac = tuple(third[index] - first[index] for index in range(3))
    cross = (
        ab[1] * ac[2] - ab[2] * ac[1],
        ab[2] * ac[0] - ab[0] * ac[2],
        ab[0] * ac[1] - ab[1] * ac[0],
    )
    return sum(value * value for value in cross)


def _cluster_position(
    position: tuple[float, float, float],
    cell_size: float,
) -> tuple[float, float, float]:
    """Snap a collision vertex to a coarse world-space grid."""
    if cell_size <= 0.0:
        return position
    return tuple(
        round(component / cell_size) * cell_size
        for component in position
    )


def _strip_generated_binding(scene: dict[str, object]) -> None:
    """Remove a previous generated binding so this cooker is repeatable."""
    matching_entities = [
        index for index, entity in enumerate(scene["entities"])
        if entity["name"] == COLLIDER_NAME
    ]
    if not matching_entities:
        return
    if matching_entities != [len(scene["entities"]) - 1]:
        raise ValueError("generated Sponza collider is not the final entity")
    collider_index = matching_entities[0]
    blocks = [
        block for block in scene["classes"]
        if collider_index in block["entities"]
    ]
    if len(blocks) != 1 or blocks[0]["class_name"] != "collider" or \
            list(blocks[0]["entities"]) != [collider_index]:
        raise ValueError("generated Sponza collider class binding is invalid")
    if any(int(connection["source"]) == collider_index or
           int(connection["target"]) == collider_index
           for connection in scene["connections"]):
        raise ValueError("generated Sponza collider has scene connections")
    scene["classes"].remove(blocks[0])
    scene["entities"].pop()

    matching_assets = [
        index for index, asset in enumerate(scene["assets"])
        if asset["path"] == COLLISION_PATH or
        asset["path"] in LEGACY_COLLISION_PATHS
    ]
    if matching_assets != [len(scene["assets"]) - 1]:
        raise ValueError("generated Sponza collision is not the final asset")
    scene["assets"].pop()


def _cook_collision(
    project_root: Path,
    scene: dict[str, object],
) -> CollisionShape:
    """Merge structural LOD-zero meshes in scene space."""
    prop_schema = GAME.entity("prop")
    if prop_schema is None:
        raise ValueError("game schema has no prop entity")
    prop_block = next(
        block for block in scene["classes"]
        if block["class_name"] == "prop"
    )
    mesh_offset = _field_offset(prop_schema, "mesh")
    selected: list[tuple[str, list[int | float], Path]] = []
    for row, entity_index in enumerate(prop_block["entities"]):
        entity = scene["entities"][entity_index]
        name = str(entity["name"])
        if name not in STRUCTURAL_PROPS:
            continue
        values = _record_values(prop_block, prop_schema, row)
        asset = scene["assets"][int(values[mesh_offset])]
        if int(asset["type"]) != int(AssetType.MESH):
            raise ValueError(f"Sponza structural prop is not a mesh: {name}")
        selected.append((
            name,
            values[:10],
            project_root / str(asset["path"]),
        ))
    selected_names = {item[0] for item in selected}
    if selected_names != STRUCTURAL_PROPS:
        missing = ", ".join(sorted(STRUCTURAL_PROPS - selected_names))
        raise ValueError(f"Sponza structural props are missing: {missing}")

    vertices: list[tuple[float, float, float]] = []
    vertex_indices: dict[tuple[float, float, float], int] = {}
    indices: list[int] = []
    triangles: set[tuple[int, int, int]] = set()
    for name, transform, mesh_path in selected:
        _, payload = read_binary_asset(mesh_path, AssetType.MESH)
        mesh = unpack_record(GAME, "mesh", payload)
        lod = mesh["lods"][0]
        remap: list[int] = []
        for vertex in lod["vertices"]:
            position = _transform_position(transform, vertex["position"])
            # Preserve authored floor and stair surfaces exactly. Other
            # structural detail is intentionally low-resolution collision.
            if name != "sponza:floor":
                position = _cluster_position(
                    position, COLLISION_CELL_SIZE)
            target = vertex_indices.get(position)
            if target is None:
                target = len(vertices)
                vertex_indices[position] = target
                vertices.append(position)
            remap.append(target)
        source_indices = lod["indices"]
        for cursor in range(0, len(source_indices), 3):
            triangle = tuple(
                remap[int(source_indices[cursor + corner])]
                for corner in range(3)
            )
            if len(set(triangle)) != 3 or _triangle_area_squared(
                    vertices[triangle[0]],
                    vertices[triangle[1]],
                    vertices[triangle[2]]) <= 1.0e-12:
                continue
            triangle_key = tuple(sorted(triangle))
            if triangle_key in triangles:
                continue
            triangles.add(triangle_key)
            indices.extend(triangle)
    if not vertices or not indices:
        raise ValueError("Sponza structural collision is empty")
    return CollisionShape(
        shape_type=ShapeType.TRIANGLE_MESH,
        vertices=vertices,
        indices=indices,
    )


def _set_spawn_origin(scene: dict[str, object]) -> None:
    """Place the player entity origin on the ground."""
    schema = GAME.entity("player_spawn")
    if schema is None:
        raise ValueError("game schema has no player_spawn entity")
    block = next(
        value for value in scene["classes"]
        if value["class_name"] == "player_spawn"
    )
    rows = [
        row for row, entity_index in enumerate(block["entities"])
        if scene["entities"][entity_index]["name"] == SPAWN_NAME
    ]
    if len(rows) != 1:
        raise ValueError("Sponza must contain one PlayerSpawn entity")
    values = _record_values(block, schema, rows[0])
    values[0:3] = [0.0, 0.0, SPAWN_FLOOR_HEIGHT]
    _replace_record(block, schema, rows[0], values)


def cook(project_root: Path) -> tuple[int, int]:
    """Cook collision and update the checked-in Sponza scene."""
    scene_path = project_root / SCENE_PATH
    scene_desc, scene_payload = read_binary_asset(
        scene_path, AssetType.SCENE)
    scene = unpack_record(GAME, "scene", scene_payload)
    _strip_generated_binding(scene)

    collision = _cook_collision(project_root, scene)
    collision_guid = guid_from_stable_name(COLLISION_PATH)
    collision_path = project_root / COLLISION_PATH
    write_binary_asset(collision_path, AssetWriteDesc(
        asset_type=AssetType.PHYSICS,
        guid=collision_guid,
        source_hash=scene_desc.source_hash,
        platform_target="generic",
        payload=physics_payload(collision),
    ))

    collision_reference = Reference(
        AssetType.PHYSICS, COLLISION_PATH, collision_guid)
    collision_index = len(scene["assets"])
    scene["assets"].append({
        "guid": collision_guid,
        "type": int(AssetType.PHYSICS),
        "path": COLLISION_PATH,
    })
    entity_index = len(scene["entities"])
    scene["entities"].append({
        "class_name": "collider",
        "name": COLLIDER_NAME,
        "flags": 1,
    })
    collider = make_schema_entity(
        GAME,
        "collider",
        transform=Transform(),
        collision=collision_reference,
    )
    collider_schema = collider.schema
    record = _pack_schema_entity(
        collider,
        {collision_reference: collision_index},
        [],
        len(scene["entities"]),
    )
    scene["classes"].append({
        "class_name": "collider",
        "version": int(collider_schema["version"]),
        "stride": entity_struct(collider_schema).size,
        "entities": [entity_index],
        "records": record,
        "assets": [],
    })
    scene["classes"].sort(key=lambda value: str(value["class_name"]))
    _set_spawn_origin(scene)
    write_binary_asset(scene_path, AssetWriteDesc(
        asset_type=scene_desc.asset_type,
        guid=scene_desc.guid,
        source_hash=scene_desc.source_hash,
        platform_target=scene_desc.platform_target,
        payload=pack_record(GAME, "scene", scene),
    ))
    return len(collision.vertices), len(collision.indices) // 3


def main() -> int:
    """Run the focused Sponza content cooker."""
    parser = argparse.ArgumentParser()
    parser.add_argument("--project-root", type=Path, default=ROOT)
    args = parser.parse_args()
    vertices, triangles = cook(args.project_root.resolve())
    print(
        f"cooked Sponza collision: {vertices} vertices, "
        f"{triangles} triangles")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
