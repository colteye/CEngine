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

import math
import struct
from dataclasses import dataclass
from pathlib import Path, PurePosixPath

from .assetfile import ASSET_HEADER, ASSET_MAGIC, ASSET_VERSION
from .formats import AssetType
from .game_schema import entity_struct, load_bundled_game
from .scene_format import (
    ASSET_REFERENCE, ENTITY_CLASS_BLOCK, ENTITY_CONNECTION,
    SCENE_ENTITY, SCENE_HEADER, SCENE_MAGIC, SCENE_SETTINGS,
    SCENE_VERSION, INVALID_INDEX,
)


_GAME_SCHEMA = load_bundled_game()
CLASS_SCHEMAS = {
    str(entity["classname"]): entity for entity in _GAME_SCHEMA.entities
}
CLASS_RECORDS = {
    classname: entity_struct(schema)
    for classname, schema in CLASS_SCHEMAS.items()
}


@dataclass(frozen=True)
class SceneInspection:
    """TODO: Describe `SceneInspection`."""

    path: Path
    entities: int
    classes: tuple[tuple[str, int], ...]
    assets: tuple[tuple[AssetType, str], ...]
    connections: int
    active_player: int | None


def _range(data: bytes, offset: int, count: int, stride: int, name: str) -> memoryview:
    """TODO: Describe `_range`.

    Args:
        data: TODO: Describe this parameter.
        offset: TODO: Describe this parameter.
        count: TODO: Describe this parameter.
        stride: TODO: Describe this parameter.
        name: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    if offset < 0 or count < 0 or stride < 0:
        raise ValueError(f"{name} has a negative range")
    size = count * stride
    if offset > len(data) or size > len(data) - offset:
        raise ValueError(f"{name} is outside the scene payload")
    return memoryview(data)[offset:offset + size]


def _asset_payload(path: Path) -> bytes:
    """TODO: Describe `_asset_payload`.

    Args:
        path: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    data = path.read_bytes()
    if len(data) < ASSET_HEADER.size:
        raise ValueError("asset header is truncated")
    header = ASSET_HEADER.unpack_from(data)
    magic, version, header_size, asset_type = header[:4]
    payload_offset, payload_size, file_size = header[7:10]
    if magic != ASSET_MAGIC or version != ASSET_VERSION or header_size != ASSET_HEADER.size:
        raise ValueError("asset header is unsupported")
    if asset_type != int(AssetType.SCENE):
        raise ValueError("asset is not a .cscene")
    if file_size != len(data) or payload_offset > len(data) or payload_size > len(data) - payload_offset:
        raise ValueError("asset payload range is invalid")
    return data[payload_offset:payload_offset + payload_size]


def _validate_dependency(project_root: Path, asset_type: AssetType, path: str, guid: bytes) -> None:
    """TODO: Describe `_validate_dependency`.

    Args:
        project_root: TODO: Describe this parameter.
        asset_type: TODO: Describe this parameter.
        path: TODO: Describe this parameter.
        guid: TODO: Describe this parameter.
    """
    target = project_root / path
    if not target.is_file():
        raise ValueError(f"referenced asset does not exist: {path}")
    if asset_type in (AssetType.TEXTURE, AssetType.AUDIO):
        return
    data = target.read_bytes()
    if len(data) < ASSET_HEADER.size:
        raise ValueError(f"referenced asset header is truncated: {path}")
    header = ASSET_HEADER.unpack_from(data)
    if header[0] != ASSET_MAGIC or header[1] != ASSET_VERSION or header[2] != ASSET_HEADER.size:
        raise ValueError(f"referenced asset header is unsupported: {path}")
    if header[3] != int(asset_type):
        raise ValueError(f"referenced asset type does not match: {path}")
    if header[4] != guid:
        raise ValueError(f"referenced asset GUID does not match: {path}")


def _schema_asset_type(field: dict[str, object]) -> AssetType:
    """TODO: Describe `_schema_asset_type`.

    Args:
        field: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    try:
        return AssetType[str(field["asset_type"]).upper()]
    except KeyError as error:
        raise ValueError(
            f"schema uses an unknown asset type: {field.get('asset_type')}"
        ) from error


def _validate_scalar(
    classname: str, field: dict[str, object], value: float
) -> None:
    """TODO: Describe `_validate_scalar`.

    Args:
        classname: TODO: Describe this parameter.
        field: TODO: Describe this parameter.
        value: TODO: Describe this parameter.
    """
    name = str(field["name"])
    if not math.isfinite(value):
        raise ValueError(f"{classname}.{name} must be finite")
    if "min" in field:
        valid = value > float(field["min"]) if field.get("min_exclusive") \
            else value >= float(field["min"])
        if not valid:
            raise ValueError(f"{classname}.{name} is below its minimum")
    if "max" in field:
        valid = value < float(field["max"]) if field.get("max_exclusive") \
            else value <= float(field["max"])
        if not valid:
            raise ValueError(f"{classname}.{name} is above its maximum")


def _validate_entity_record(
    classname: str,
    schema: dict[str, object],
    record: memoryview,
    auxiliary: memoryview,
    assets: list[tuple[AssetType, str]],
    entity_count: int,
) -> None:
    """TODO: Describe `_validate_entity_record`.

    Args:
        classname: TODO: Describe this parameter.
        schema: TODO: Describe this parameter.
        record: TODO: Describe this parameter.
        auxiliary: TODO: Describe this parameter.
        assets: TODO: Describe this parameter.
        entity_count: TODO: Describe this parameter.
    """
    values = CLASS_RECORDS[classname].unpack(record)
    cursor = 0
    decoded: dict[str, float | int] = {}

    def require_asset(index: int, asset_type: AssetType, name: str) -> None:
        """TODO: Describe `require_asset`.

        Args:
            index: TODO: Describe this parameter.
            asset_type: TODO: Describe this parameter.
            name: TODO: Describe this parameter.
        """
        if index >= len(assets) or assets[index][0] != asset_type:
            raise ValueError(f"{classname}.{name} asset reference is invalid")

    for field in schema["fields"]:
        name = str(field["name"])
        field_type = field["type"]
        if field_type == "transform":
            transform = values[cursor:cursor + 10]
            if any(not math.isfinite(float(component)) for component in transform):
                raise ValueError(f"{classname}.transform must be finite")
            cursor += 10
        elif field_type == "f32":
            value = float(values[cursor])
            _validate_scalar(classname, field, value)
            decoded[name] = value
            cursor += 1
        elif field_type == "u32":
            value = int(values[cursor])
            if "min" in field and value < int(field["min"]):
                raise ValueError(f"{classname}.{name} is below its minimum")
            if "max" in field and value > int(field["max"]):
                raise ValueError(f"{classname}.{name} is above its maximum")
            decoded[name] = value
            cursor += 1
        elif field_type == "entity":
            index = int(values[cursor])
            if index >= entity_count:
                raise ValueError(
                    f"{classname}.{name} entity reference is invalid")
            decoded[name] = index
            cursor += 1
        elif field_type == "bool":
            value = int(values[cursor])
            if value not in (0, 1):
                raise ValueError(f"{classname}.{name} is not a boolean")
            decoded[name] = value
            cursor += 1
        elif field_type == "enum":
            value = int(values[cursor])
            if value not in field["values"].values():
                raise ValueError(f"{classname}.{name} is invalid")
            decoded[name] = value
            cursor += 1
        elif field_type in ("vec2", "vec3"):
            count = 2 if field_type == "vec2" else 3
            for component in values[cursor:cursor + count]:
                _validate_scalar(classname, field, float(component))
            cursor += count
        elif field_type == "asset":
            index = int(values[cursor])
            if index == INVALID_INDEX:
                if not field.get("optional", False):
                    raise ValueError(f"{classname}.{name} is required")
            else:
                require_asset(index, _schema_asset_type(field), name)
            cursor += 1
        elif field_type == "asset_list":
            first, count = (int(value) for value in values[cursor:cursor + 2])
            available = len(auxiliary) // 4
            if count:
                if first == INVALID_INDEX or first > available or \
                        count > available - first:
                    raise ValueError(f"{classname}.{name} range is invalid")
                for index in range(first, first + count):
                    asset = struct.unpack_from("<I", auxiliary, index * 4)[0]
                    require_asset(asset, _schema_asset_type(field), name)
            cursor += 2
        elif field_type == "flags":
            value = int(values[cursor])
            mask = sum(1 << int(member["bit"]) for member in field["members"])
            if value & ~mask:
                raise ValueError(f"{classname}.{name} contains unknown flags")
            decoded[name] = value
            cursor += 1
        else:
            raise ValueError(f"{classname}.{name} has an unsupported type")

    for field in schema["fields"]:
        if "greater_than" in field and \
                float(decoded[str(field["name"])]) <= \
                float(decoded[str(field["greater_than"])]):
            raise ValueError(
                f"{classname}.{field['name']} must be greater than "
                f"{field['greater_than']}"
            )


def inspect_scene(path: Path, project_root: Path, validate_assets: bool = False) -> SceneInspection:
    """TODO: Describe `inspect_scene`.

    Args:
        path: TODO: Describe this parameter.
        project_root: TODO: Describe this parameter.
        validate_assets: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    payload = _asset_payload(path)
    if len(payload) < SCENE_HEADER.size:
        raise ValueError("scene header is truncated")
    header = SCENE_HEADER.unpack_from(payload)
    if header[0] != SCENE_MAGIC or header[1] != SCENE_VERSION or header[2] != SCENE_HEADER.size:
        raise ValueError("scene header is unsupported")

    settings_offset = header[3]
    asset_offset, asset_count, asset_stride = header[4:7]
    entity_offset, entity_count, entity_stride = header[7:10]
    class_offset, class_count, class_stride = header[10:13]
    connection_offset, connection_count, connection_stride = header[13:16]
    strings_offset, strings_size = header[16:18]
    _range(payload, settings_offset, 1, SCENE_SETTINGS.size, "scene settings")
    if asset_count and asset_stride != ASSET_REFERENCE.size:
        raise ValueError("asset reference stride is unsupported")
    if entity_count and entity_stride != SCENE_ENTITY.size:
        raise ValueError("entity stride is unsupported")
    if class_count and class_stride != ENTITY_CLASS_BLOCK.size:
        raise ValueError("entity class stride is unsupported")
    if connection_count and connection_stride != ENTITY_CONNECTION.size:
        raise ValueError("connection stride is unsupported")
    assets_data = _range(payload, asset_offset, asset_count, asset_stride, "asset references")
    entities_data = _range(payload, entity_offset, entity_count, entity_stride, "entities")
    classes_data = _range(payload, class_offset, class_count, class_stride, "entity classes")
    connections_data = _range(payload, connection_offset, connection_count, connection_stride, "connections")
    strings = _range(payload, strings_offset, 1, strings_size, "string table")

    def text(offset: int, size: int, name: str) -> str:
        """TODO: Describe `text`.

        Args:
            offset: TODO: Describe this parameter.
            size: TODO: Describe this parameter.
            name: TODO: Describe this parameter.

        Returns:
            TODO: Describe the produced value.
        """
        if offset > len(strings) or size > len(strings) - offset:
            raise ValueError(f"{name} is outside the string table")
        try:
            return bytes(strings[offset:offset + size]).decode("utf-8")
        except UnicodeDecodeError as error:
            raise ValueError(f"{name} is not valid UTF-8") from error

    assets: list[tuple[AssetType, str]] = []
    for index in range(asset_count):
        guid, type_value, _flags, path_offset, path_size = ASSET_REFERENCE.unpack_from(
            assets_data, index * asset_stride)
        try:
            asset_type = AssetType(type_value)
        except ValueError as error:
            raise ValueError("scene asset reference type is unsupported") from error
        if asset_type in (AssetType.UNKNOWN, AssetType.SCENE):
            raise ValueError("scene asset reference type is invalid")
        asset_path = text(path_offset, path_size, "asset path")
        normalized = PurePosixPath(asset_path)
        if not asset_path or normalized.is_absolute() or ".." in normalized.parts:
            raise ValueError(f"asset path is not project-relative: {asset_path}")
        if validate_assets:
            _validate_dependency(project_root, asset_type, asset_path, guid)
        assets.append((asset_type, asset_path))

    classnames: list[str] = []
    for index in range(entity_count):
        classname_offset, classname_size, name_offset, name_size, _flags = SCENE_ENTITY.unpack_from(
            entities_data, index * entity_stride)
        classname = text(classname_offset, classname_size, "entity classname")
        text(name_offset, name_size, "entity name")
        if classname not in CLASS_RECORDS:
            raise ValueError(f"unsupported entity classname: {classname}")
        classnames.append(classname)

    loaded = [False] * entity_count
    classes: list[tuple[str, int]] = []

    for index in range(class_count):
        block = ENTITY_CLASS_BLOCK.unpack_from(classes_data, index * class_stride)
        name_offset, name_size, version, _flags, count, record_stride = block[:6]
        indices_offset, records_offset, auxiliary_offset, auxiliary_size = block[6:10]
        classname = text(name_offset, name_size, "class block classname")
        schema = CLASS_SCHEMAS.get(classname)
        if schema is None or version != schema["version"]:
            raise ValueError(f"unsupported entity class block: {classname}")
        if record_stride != CLASS_RECORDS[classname].size:
            raise ValueError(f"entity class record stride is unsupported: {classname}")
        indices = _range(payload, indices_offset, count, 4, "class entity indices")
        records = _range(payload, records_offset, count, record_stride, "class records")
        auxiliary = _range(payload, auxiliary_offset, 1, auxiliary_size, "class auxiliary data")
        for row in range(count):
            entity = struct.unpack_from("<I", indices, row * 4)[0]
            if entity >= entity_count or loaded[entity] or classnames[entity] != classname:
                raise ValueError(f"entity class membership is invalid: {classname}")
            loaded[entity] = True
            record = records[row * record_stride:(row + 1) * record_stride]
            _validate_entity_record(
                classname, schema, record, auxiliary, assets, entity_count)
        classes.append((classname, count))
    if not all(loaded):
        raise ValueError("one or more entities have no class record")

    for index in range(connection_count):
        source, target, event_offset, event_size, action_offset, action_size, delay = \
            ENTITY_CONNECTION.unpack_from(connections_data, index * connection_stride)
        if source >= entity_count or target >= entity_count:
            raise ValueError("connection entity index is invalid")
        if not text(event_offset, event_size, "connection event") or \
                not text(action_offset, action_size, "connection action"):
            raise ValueError("connection event and action must not be empty")
        if not math.isfinite(delay) or delay < 0.0:
            raise ValueError("connection delay is invalid")

    settings = SCENE_SETTINGS.unpack_from(payload, settings_offset)
    active_player = settings[7]
    if active_player != INVALID_INDEX and active_player >= entity_count:
        raise ValueError("active camera entity index is invalid")
    return SceneInspection(path, entity_count, tuple(classes), tuple(assets),
        connection_count, None if active_player == INVALID_INDEX else active_player)


def print_scene(inspection: SceneInspection) -> None:
    """TODO: Describe `print_scene`.

    Args:
        inspection: TODO: Describe this parameter.
    """
    print(f"scene: {inspection.path}")
    print(f"entities: {inspection.entities}")
    print(f"classes: {len(inspection.classes)}")
    for classname, count in inspection.classes:
        print(f"  {classname}: {count}")
    print(f"assets: {len(inspection.assets)}")
    for asset_type, path in inspection.assets:
        print(f"  {asset_type.name.lower()}: {path}")
    print(f"connections: {inspection.connections}")
    print(f"active player: {inspection.active_player if inspection.active_player is not None else 'none'}")
