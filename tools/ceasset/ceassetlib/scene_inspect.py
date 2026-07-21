from __future__ import annotations

import math
import struct
from dataclasses import dataclass
from pathlib import Path, PurePosixPath

from .assetfile import ASSET_HEADER, ASSET_MAGIC, ASSET_VERSION
from .formats import AssetType
from .scene_format import (
    ASSET_REFERENCE, CAMERA_ENTITY, DYNAMIC_PROP, ENTITY_CLASS_BLOCK,
    ENTITY_CLASS_VERSION, ENTITY_CONNECTION, LIGHT_ENTITY, PLAYER_START,
    PREFAB_ENTITY, SCENE_ENTITY, SCENE_HEADER, SCENE_MAGIC, SCENE_SETTINGS,
    SCENE_VERSION, STATIC_PROP, TRANSFORM, TRIGGER_ENTITY, INVALID_INDEX,
)


CLASS_STRIDES = {
    "empty": TRANSFORM.size,
    "prop_static": STATIC_PROP.size,
    "prop_dynamic": DYNAMIC_PROP.size,
    "camera": CAMERA_ENTITY.size,
    "light": LIGHT_ENTITY.size,
    "prefab_instance": PREFAB_ENTITY.size,
    "trigger": TRIGGER_ENTITY.size,
    "info_player_start": PLAYER_START.size,
}


@dataclass(frozen=True)
class SceneInspection:
    path: Path
    entities: int
    classes: tuple[tuple[str, int], ...]
    assets: tuple[tuple[AssetType, str], ...]
    connections: int
    active_camera: int | None


def _range(data: bytes, offset: int, count: int, stride: int, name: str) -> memoryview:
    if offset < 0 or count < 0 or stride < 0:
        raise ValueError(f"{name} has a negative range")
    size = count * stride
    if offset > len(data) or size > len(data) - offset:
        raise ValueError(f"{name} is outside the scene payload")
    return memoryview(data)[offset:offset + size]


def _asset_payload(path: Path) -> bytes:
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


def inspect_scene(path: Path, project_root: Path, validate_assets: bool = False) -> SceneInspection:
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
        if classname not in CLASS_STRIDES:
            raise ValueError(f"unsupported entity classname: {classname}")
        classnames.append(classname)

    loaded = [False] * entity_count
    classes: list[tuple[str, int]] = []

    def require_asset(index: int, asset_type: AssetType, name: str) -> None:
        if index >= len(assets) or assets[index][0] != asset_type:
            raise ValueError(f"{name} asset reference is invalid")

    for index in range(class_count):
        block = ENTITY_CLASS_BLOCK.unpack_from(classes_data, index * class_stride)
        name_offset, name_size, version, _flags, count, record_stride = block[:6]
        indices_offset, records_offset, auxiliary_offset, auxiliary_size = block[6:10]
        classname = text(name_offset, name_size, "class block classname")
        if version != ENTITY_CLASS_VERSION or classname not in CLASS_STRIDES:
            raise ValueError(f"unsupported entity class block: {classname}")
        if record_stride != CLASS_STRIDES[classname]:
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
            if classname in ("prop_static", "prop_dynamic"):
                values = (STATIC_PROP if classname == "prop_static" else DYNAMIC_PROP).unpack(record)
                require_asset(values[10], AssetType.MESH, f"{classname} mesh")
                first_material, material_count = values[11:13]
                available = len(auxiliary) // 4
                if material_count:
                    if first_material == INVALID_INDEX or first_material > available or \
                            material_count > available - first_material:
                        raise ValueError(f"{classname} material range is invalid")
                    for material in range(first_material, first_material + material_count):
                        require_asset(struct.unpack_from("<I", auxiliary, material * 4)[0],
                            AssetType.MATERIAL, f"{classname} material")
                if classname == "prop_static" and values[13] != INVALID_INDEX:
                    require_asset(values[13], AssetType.TEXTURE, "prop_static lightmap")
                    if (values[14] <= 0.0 or values[15] <= 0.0 or
                            values[16] < 0.0 or values[17] < 0.0 or
                            values[14] + values[16] > 1.0 or
                            values[15] + values[17] > 1.0):
                        raise ValueError("prop_static lightmap atlas transform is invalid")
                    if values[18] <= 0.0:
                        raise ValueError("prop_static lightmap RGBM range is invalid")
                if classname == "prop_dynamic" and values[13] & 2 and (
                        any(value <= 0.0 for value in values[14:17]) or values[17] <= 0.0):
                    raise ValueError("prop_dynamic physics record is invalid")
            elif classname == "prefab_instance":
                require_asset(PREFAB_ENTITY.unpack(record)[10], AssetType.ASSET, "prefab")
            elif classname == "camera":
                values = CAMERA_ENTITY.unpack(record)
                if values[10] > 1 or values[13] <= 0.0 or values[14] <= values[13]:
                    raise ValueError("camera record is invalid")
            elif classname == "light":
                values = LIGHT_ENTITY.unpack(record)
                if values[10] > 3 or values[11] > 2 or values[15] < 0.0 or values[16] < 0.0:
                    raise ValueError("light record is invalid")
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
    active_camera = settings[7]
    if active_camera != INVALID_INDEX and active_camera >= entity_count:
        raise ValueError("active camera entity index is invalid")
    return SceneInspection(path, entity_count, tuple(classes), tuple(assets),
        connection_count, None if active_camera == INVALID_INDEX else active_camera)


def print_scene(inspection: SceneInspection) -> None:
    print(f"scene: {inspection.path}")
    print(f"entities: {inspection.entities}")
    print(f"classes: {len(inspection.classes)}")
    for classname, count in inspection.classes:
        print(f"  {classname}: {count}")
    print(f"assets: {len(inspection.assets)}")
    for asset_type, path in inspection.assets:
        print(f"  {asset_type.name.lower()}: {path}")
    print(f"connections: {inspection.connections}")
    print(f"active camera: {inspection.active_camera if inspection.active_camera is not None else 'none'}")
