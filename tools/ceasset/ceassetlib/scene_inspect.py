#   _____ ______             _
#  / ____|  ____|           (_)
# | |    | |__   _ __   __ _ _ _ __   ___
# | |    |  __| | '_ \ / _` | | '_ \ / _ \
# | |____| |____| | | | (_| | | | | |  __/
#  \_____|______|_| |_|\__, |_|_| |_|\___|
#                       __/ |
#                      |___/

"""Inspect schema-decoded scene assets."""

from __future__ import annotations

import math
from dataclasses import dataclass
from pathlib import Path, PurePosixPath

from .assetfile import read_binary_asset
from .formats import AssetType
from .game_schema import entity_struct, load_bundled_game
from .scene_format import INVALID_INDEX
from .wire import unpack_record


GAME = load_bundled_game()
ENTITIES = {str(value["classname"]): value for value in GAME.entities}


@dataclass(frozen=True)
class SceneInspection:
    path: Path
    entities: int
    classes: tuple[tuple[str, int], ...]
    assets: tuple[tuple[AssetType, str], ...]
    connections: int
    active_player: int | None


def _dependency(root: Path, value: dict[str, object]) -> tuple[AssetType, str]:
    try:
        asset_type = AssetType(int(value["type"]))
    except ValueError as error:
        raise ValueError("scene asset reference type is unsupported") from error
    path = str(value["path"])
    normalized = PurePosixPath(path)
    if asset_type in (AssetType.UNKNOWN, AssetType.SCENE) or not path or \
            normalized.is_absolute() or ".." in normalized.parts:
        raise ValueError(f"scene asset reference is invalid: {path}")
    target = root / path
    if not target.is_file():
        raise ValueError(f"referenced asset does not exist: {path}")
    if asset_type not in (AssetType.TEXTURE, AssetType.AUDIO):
        desc, _ = read_binary_asset(target, asset_type)
        if desc.guid != value["guid"]:
            raise ValueError(f"referenced asset GUID does not match: {path}")
    return asset_type, path


def _validate_record(
    schema: dict[str, object],
    record: bytes,
    assets: list[dict[str, object]],
    auxiliary: list[int],
    entity_count: int,
) -> None:
    values = entity_struct(schema).unpack(record)
    cursor = 0
    for field in schema["fields"]:
        field_type = str(field["type"])
        name = f"{schema['classname']}.{field['name']}"
        count = {"transform": 10, "vec2": 2, "vec3": 3,
                 "asset_list": 2}.get(field_type, 1)
        selected = values[cursor:cursor + count]
        cursor += count
        if field_type in {"transform", "f32", "vec2", "vec3"}:
            if any(not math.isfinite(float(value)) for value in selected):
                raise ValueError(f"{name} must be finite")
        if field_type == "entity" and int(selected[0]) >= entity_count:
            raise ValueError(f"{name} entity reference is invalid")
        if field_type == "bool" and int(selected[0]) not in (0, 1):
            raise ValueError(f"{name} is not a boolean")
        if field_type == "enum" and int(selected[0]) not in field["values"].values():
            raise ValueError(f"{name} is invalid")
        if field_type == "flags":
            mask = sum(1 << int(member["bit"]) for member in field["members"])
            if int(selected[0]) & ~mask:
                raise ValueError(f"{name} contains unknown flags")
        if field_type == "asset":
            index = int(selected[0])
            if index == INVALID_INDEX:
                if not field.get("optional", False):
                    raise ValueError(f"{name} is required")
            elif index >= len(assets):
                raise ValueError(f"{name} asset reference is invalid")
        if field_type == "asset_list":
            first, count_value = (int(value) for value in selected)
            if count_value and (first == INVALID_INDEX or first > len(auxiliary) or
                                count_value > len(auxiliary) - first):
                raise ValueError(f"{name} range is invalid")
            if any(index >= len(assets) for index in
                   auxiliary[first:first + count_value] if first != INVALID_INDEX):
                raise ValueError(f"{name} asset reference is invalid")


def inspect_scene(
    path: Path, project_root: Path, validate_assets: bool = False
) -> SceneInspection:
    _, payload = read_binary_asset(path, AssetType.SCENE)
    scene = unpack_record(GAME, "scene", payload)
    assets = list(scene["assets"])
    inspected_assets = [
        _dependency(project_root, value) if validate_assets else
        (AssetType(int(value["type"])), str(value["path"]))
        for value in assets
    ]
    entities = list(scene["entities"])
    loaded = [False] * len(entities)
    classes = []
    for block in scene["classes"]:
        classname = str(block["class_name"])
        schema = ENTITIES.get(classname)
        if schema is None or int(block["version"]) != int(schema["version"]):
            raise ValueError(f"unsupported entity class: {classname}")
        stride = entity_struct(schema).size
        indices = list(block["entities"])
        records = bytes(block["records"])
        if int(block["stride"]) != stride or len(records) != stride * len(indices):
            raise ValueError(f"entity class record size is invalid: {classname}")
        for row, index in enumerate(indices):
            if index >= len(entities) or loaded[index] or \
                    entities[index]["class_name"] != classname:
                raise ValueError(f"entity class membership is invalid: {classname}")
            loaded[index] = True
            _validate_record(
                schema, records[row * stride:(row + 1) * stride],
                assets, list(block["assets"]), len(entities))
        classes.append((classname, len(indices)))
    if not all(loaded):
        raise ValueError("one or more entities have no class record")
    for connection in scene["connections"]:
        if int(connection["source"]) >= len(entities) or \
                int(connection["target"]) >= len(entities):
            raise ValueError("connection entity index is invalid")
    active = int(scene["settings"]["active_entity"])
    if active != INVALID_INDEX and active >= len(entities):
        raise ValueError("active player entity index is invalid")
    return SceneInspection(
        path, len(entities), tuple(classes), tuple(inspected_assets),
        len(scene["connections"]), None if active == INVALID_INDEX else active)


def print_scene(inspection: SceneInspection) -> None:
    print(f"scene: {inspection.path}")
    print(f"entities: {inspection.entities}")
    for classname, count in inspection.classes:
        print(f"  {classname}: {count}")
    for asset_type, path in inspection.assets:
        print(f"  {asset_type.name.lower()}: {path}")
    print(f"connections: {inspection.connections}")
    print(f"active player: {inspection.active_player if inspection.active_player is not None else 'none'}")
