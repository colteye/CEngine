from __future__ import annotations

import posixpath
import math
import struct
from dataclasses import dataclass, field
from pathlib import Path
from typing import Union

from .assetfile import align_up, make_asset_desc, write_binary_asset
from .formats import AssetType
from .scene_format import (
    ASSET_REFERENCE, CAMERA_ENTITY, DYNAMIC_PROP, ENTITY_CLASS_BLOCK,
    ENTITY_CLASS_VERSION, ENTITY_CONNECTION, EntityClassBlockFlags, LIGHT_ENTITY, PLAYER_START,
    PREFAB_ENTITY, SCENE_ENTITY, SCENE_HEADER, SCENE_MAGIC, SCENE_SETTINGS,
    SCENE_VERSION, STATIC_PROP, TRANSFORM, TRIGGER_ENTITY, INVALID_INDEX,
)

TABLE_ALIGNMENT = 16


def normalize_asset_path(path: str) -> str:
    normalized = posixpath.normpath(path.replace("\\", "/"))
    if normalized in ("", ".") or normalized.startswith("/") or normalized == ".." or normalized.startswith("../"):
        raise ValueError(f"asset path must be project-relative: {path}")
    return normalized


@dataclass(frozen=True)
class AssetReference:
    asset_type: AssetType
    path: str
    guid: bytes

    def normalized(self) -> "AssetReference":
        if len(self.guid) != 16:
            raise ValueError("asset GUID must contain 16 bytes")
        if self.asset_type in (AssetType.UNKNOWN, AssetType.SCENE):
            raise ValueError("scene reference has an invalid asset type")
        return AssetReference(self.asset_type, normalize_asset_path(self.path), self.guid)


@dataclass(frozen=True)
class Transform:
    position: tuple[float, float, float] = (0.0, 0.0, 0.0)
    rotation: tuple[float, float, float, float] = (0.0, 0.0, 0.0, 1.0)
    scale: tuple[float, float, float] = (1.0, 1.0, 1.0)


@dataclass(frozen=True)
class EmptyEntity:
    transform: Transform = field(default_factory=Transform)


@dataclass(frozen=True)
class StaticProp:
    mesh: AssetReference
    transform: Transform = field(default_factory=Transform)
    materials: tuple[AssetReference, ...] = ()
    lightmap: AssetReference | None = None
    lightmap_scale: tuple[float, float] = (1.0, 1.0)
    lightmap_offset: tuple[float, float] = (0.0, 0.0)
    lightmap_rgbm_range: float = 8.0
    visible: bool = True


@dataclass(frozen=True)
class DynamicProp:
    mesh: AssetReference
    transform: Transform = field(default_factory=Transform)
    materials: tuple[AssetReference, ...] = ()
    visible: bool = True
    physics_enabled: bool = False
    collision_half_extents: tuple[float, float, float] = (0.5, 0.5, 0.5)
    mass: float = 1.0


@dataclass(frozen=True)
class CameraEntity:
    transform: Transform = field(default_factory=Transform)
    projection: int = 0
    vertical_fov_radians: float = 1.0471976
    orthographic_size: float = 10.0
    near_clip: float = 0.1
    far_clip: float = 1000.0
    enabled: bool = True


@dataclass(frozen=True)
class LightEntity:
    transform: Transform = field(default_factory=Transform)
    light_type: int = 0
    mode: int = 0
    color: tuple[float, float, float] = (1.0, 1.0, 1.0)
    intensity: float = 1.0
    range: float = 10.0
    inner_angle_radians: float = 0.0
    outer_angle_radians: float = 0.7853982
    area_size: tuple[float, float] = (1.0, 1.0)
    enabled: bool = True


@dataclass(frozen=True)
class PrefabEntity:
    prefab: AssetReference
    transform: Transform = field(default_factory=Transform)


@dataclass(frozen=True)
class TriggerEntity:
    transform: Transform = field(default_factory=Transform)
    half_extents: tuple[float, float, float] = (0.5, 0.5, 0.5)
    enabled: bool = True
    trigger_once: bool = False


@dataclass(frozen=True)
class PlayerStart:
    transform: Transform = field(default_factory=Transform)
    team: int = 0


EntityData = Union[EmptyEntity, StaticProp, DynamicProp, CameraEntity,
                   LightEntity, PrefabEntity, TriggerEntity, PlayerStart]


@dataclass(frozen=True)
class EntityDescription:
    data: EntityData
    name: str = ""
    flags: int = 1


@dataclass(frozen=True)
class SceneSettings:
    ambient_color: tuple[float, float, float] = (0.0, 0.0, 0.0)
    exposure: float = 1.0
    gravity: tuple[float, float, float] = (0.0, -9.81, 0.0)
    active_camera_entity: int | None = None
    flags: int = 0


@dataclass(frozen=True)
class EntityConnection:
    source_entity: int
    event: str
    target_entity: int
    action: str
    delay_seconds: float = 0.0


@dataclass(frozen=True)
class SceneDescription:
    entities: tuple[EntityDescription, ...]
    settings: SceneSettings = field(default_factory=SceneSettings)
    connections: tuple[EntityConnection, ...] = ()


CLASS_INFO = {
    EmptyEntity: ("empty", TRANSFORM),
    StaticProp: ("prop_static", STATIC_PROP),
    DynamicProp: ("prop_dynamic", DYNAMIC_PROP),
    CameraEntity: ("camera", CAMERA_ENTITY),
    LightEntity: ("light", LIGHT_ENTITY),
    PrefabEntity: ("prefab_instance", PREFAB_ENTITY),
    TriggerEntity: ("trigger", TRIGGER_ENTITY),
    PlayerStart: ("info_player_start", PLAYER_START),
}


class StringTable:
    def __init__(self) -> None:
        self.data = bytearray()
        self.ranges: dict[str, tuple[int, int]] = {}

    def add(self, value: str) -> tuple[int, int]:
        if value in self.ranges:
            return self.ranges[value]
        encoded = value.encode("utf-8")
        result = (len(self.data), len(encoded))
        self.data.extend(encoded)
        self.ranges[value] = result
        return result


def _transform_values(value: Transform) -> tuple[float, ...]:
    return (*value.position, *value.rotation, *value.scale)


def _append_aligned(output: bytearray, data: bytes) -> int:
    offset = align_up(len(output), TABLE_ALIGNMENT)
    output.extend(bytes(offset - len(output)))
    output.extend(data)
    return offset


def _references(value: EntityData) -> tuple[AssetReference, ...]:
    if isinstance(value, (StaticProp, DynamicProp)):
        result = (value.mesh, *value.materials)
        if isinstance(value, StaticProp) and value.lightmap is not None:
            result += (value.lightmap,)
        return result
    if isinstance(value, PrefabEntity):
        return (value.prefab,)
    return ()


def _collect_assets(entities: list[EntityDescription]) -> tuple[list[AssetReference], dict[AssetReference, int]]:
    by_path: dict[str, AssetReference] = {}
    for entity in entities:
        for reference in _references(entity.data):
            normalized = reference.normalized()
            previous = by_path.get(normalized.path)
            if previous is not None and previous != normalized:
                raise ValueError(f"conflicting asset references for {normalized.path}")
            by_path[normalized.path] = normalized
    assets = sorted(by_path.values(), key=lambda item: (item.path, int(item.asset_type), item.guid))
    return assets, {asset: index for index, asset in enumerate(assets)}


def _asset_index(reference: AssetReference, indices: dict[AssetReference, int]) -> int:
    return indices[reference.normalized()]


def _pack_entity(value: EntityData, assets: dict[AssetReference, int], materials: list[int]) -> bytes:
    transform = _transform_values(value.transform)
    if isinstance(value, EmptyEntity):
        return TRANSFORM.pack(*transform)
    if isinstance(value, StaticProp):
        first = len(materials) if value.materials else INVALID_INDEX
        materials.extend(_asset_index(item, assets) for item in value.materials)
        lightmap = INVALID_INDEX if value.lightmap is None else _asset_index(value.lightmap, assets)
        if value.lightmap_rgbm_range <= 0.0:
            raise ValueError("lightmap RGBM range must be positive")
        if (value.lightmap is not None and
                (value.lightmap_scale[0] <= 0.0 or value.lightmap_scale[1] <= 0.0 or
                 value.lightmap_offset[0] < 0.0 or value.lightmap_offset[1] < 0.0 or
                 value.lightmap_offset[0] + value.lightmap_scale[0] > 1.0 or
                 value.lightmap_offset[1] + value.lightmap_scale[1] > 1.0)):
            raise ValueError("lightmap atlas transform must stay inside zero-to-one UV space")
        return STATIC_PROP.pack(*transform, _asset_index(value.mesh, assets), first, len(value.materials),
                                lightmap, *value.lightmap_scale, *value.lightmap_offset,
                                value.lightmap_rgbm_range, int(value.visible))
    if isinstance(value, DynamicProp):
        first = len(materials) if value.materials else INVALID_INDEX
        materials.extend(_asset_index(item, assets) for item in value.materials)
        flags = int(value.visible) | (int(value.physics_enabled) << 1)
        if value.physics_enabled and (value.mass <= 0.0 or any(size <= 0.0 for size in value.collision_half_extents)):
            raise ValueError("dynamic prop physics dimensions and mass must be positive")
        return DYNAMIC_PROP.pack(*transform, _asset_index(value.mesh, assets), first,
                                 len(value.materials), flags, *value.collision_half_extents, value.mass)
    if isinstance(value, CameraEntity):
        if value.near_clip <= 0.0 or value.far_clip <= value.near_clip:
            raise ValueError("camera clip distances are invalid")
        return CAMERA_ENTITY.pack(*transform, value.projection, value.vertical_fov_radians,
                                  value.orthographic_size, value.near_clip, value.far_clip, int(value.enabled))
    if isinstance(value, LightEntity):
        if value.intensity < 0.0 or value.range < 0.0:
            raise ValueError("light intensity and range must be nonnegative")
        return LIGHT_ENTITY.pack(*transform, value.light_type, value.mode, *value.color,
                                 value.intensity, value.range, value.inner_angle_radians,
                                 value.outer_angle_radians, *value.area_size, int(value.enabled))
    if isinstance(value, PrefabEntity):
        return PREFAB_ENTITY.pack(*transform, _asset_index(value.prefab, assets), 0)
    if isinstance(value, TriggerEntity):
        flags = int(value.enabled) | (int(value.trigger_once) << 1)
        return TRIGGER_ENTITY.pack(*transform, *value.half_extents, flags)
    if isinstance(value, PlayerStart):
        return PLAYER_START.pack(*transform, value.team)
    raise TypeError(f"unsupported entity class: {type(value).__name__}")


def build_scene_payload(scene: SceneDescription) -> bytes:
    entities = list(scene.entities)
    if not entities:
        raise ValueError("scene must contain at least one entity")
    for entity in entities:
        if type(entity.data) not in CLASS_INFO:
            raise ValueError("scene contains an unsupported entity class")

    assets, asset_indices = _collect_assets(entities)
    active_camera = INVALID_INDEX
    if scene.settings.active_camera_entity is not None:
        if not 0 <= scene.settings.active_camera_entity < len(entities):
            raise ValueError("active camera entity index is invalid")
        active_camera = scene.settings.active_camera_entity

    strings = StringTable()
    asset_rows = bytearray()
    for asset in assets:
        path_offset, path_size = strings.add(asset.path)
        asset_rows.extend(ASSET_REFERENCE.pack(asset.guid, int(asset.asset_type), 0, path_offset, path_size))

    entity_rows = bytearray()
    grouped: dict[type[EntityData], list[tuple[int, EntityDescription]]] = {}
    for index, entity in enumerate(entities):
        classname, _ = CLASS_INFO[type(entity.data)]
        class_offset, class_size = strings.add(classname)
        name_offset, name_size = strings.add(entity.name)
        flags = entity.flags | (2 if isinstance(entity.data, StaticProp) else 0)
        entity_rows.extend(SCENE_ENTITY.pack(class_offset, class_size,
                                             name_offset, name_size, flags))
        grouped.setdefault(type(entity.data), []).append((index, entity))

    output = bytearray(SCENE_HEADER.size)
    settings_offset = _append_aligned(output, SCENE_SETTINGS.pack(
        *scene.settings.ambient_color, scene.settings.exposure, *scene.settings.gravity,
        active_camera, scene.settings.flags, 0, 0, 0))
    asset_offset = _append_aligned(output, asset_rows) if asset_rows else 0
    entity_offset = _append_aligned(output, entity_rows)
    groups = sorted(grouped.items(), key=lambda item: CLASS_INFO[item[0]][0])
    class_offset = align_up(len(output), TABLE_ALIGNMENT)
    output.extend(bytes(class_offset - len(output) + len(groups) * ENTITY_CLASS_BLOCK.size))

    class_rows = bytearray()
    for data_type, rows in groups:
        classname, record_struct = CLASS_INFO[data_type]
        classname_offset, classname_size = strings.add(classname)
        indices = struct.pack(f"<{len(rows)}I", *(row[0] for row in rows))
        indices_offset = _append_aligned(output, indices)
        materials: list[int] = []
        records = b"".join(_pack_entity(row[1].data, asset_indices, materials) for row in rows)
        records_offset = _append_aligned(output, records)
        auxiliary = struct.pack(f"<{len(materials)}I", *materials) if materials else b""
        auxiliary_offset = _append_aligned(output, auxiliary) if auxiliary else 0
        class_rows.extend(ENTITY_CLASS_BLOCK.pack(classname_offset, classname_size,
            ENTITY_CLASS_VERSION, int(EntityClassBlockFlags.REQUIRED), len(rows), record_struct.size,
            indices_offset, records_offset, auxiliary_offset, len(auxiliary)))
    output[class_offset:class_offset + len(class_rows)] = class_rows

    connection_rows = bytearray()
    for connection in sorted(scene.connections,
            key=lambda item: (item.source_entity, item.event, item.target_entity, item.action, item.delay_seconds)):
        if not 0 <= connection.source_entity < len(entities) or not 0 <= connection.target_entity < len(entities):
            raise ValueError("scene connection entity index is invalid")
        if not connection.event or not connection.action:
            raise ValueError("scene connection names must not be empty")
        if not math.isfinite(connection.delay_seconds) or connection.delay_seconds < 0.0:
            raise ValueError("scene connection delay must be finite and nonnegative")
        event_offset, event_size = strings.add(connection.event)
        action_offset, action_size = strings.add(connection.action)
        connection_rows.extend(ENTITY_CONNECTION.pack(
            connection.source_entity, connection.target_entity,
            event_offset, event_size, action_offset, action_size,
            connection.delay_seconds))
    connection_offset = _append_aligned(output, connection_rows) if connection_rows else 0

    strings_offset = _append_aligned(output, strings.data)
    SCENE_HEADER.pack_into(output, 0, SCENE_MAGIC, SCENE_VERSION, SCENE_HEADER.size,
        settings_offset, asset_offset, len(assets), ASSET_REFERENCE.size if assets else 0,
        entity_offset, len(entities), SCENE_ENTITY.size,
        class_offset, len(groups), ENTITY_CLASS_BLOCK.size,
        connection_offset, len(scene.connections), ENTITY_CONNECTION.size if scene.connections else 0,
        strings_offset, len(strings.data))
    return bytes(output)


def write_scene(path: Path, scene: SceneDescription, stable_name: str, source_hash: int = 0) -> None:
    write_binary_asset(path, make_asset_desc(
        AssetType.SCENE, stable_name, source_hash, build_scene_payload(scene)))
