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
    ASSET_REFERENCE, CAMERA_ENTITY, ENTITY_CLASS_BLOCK,
    ENTITY_CLASS_VERSION, ENTITY_CONNECTION, EntityClassBlockFlags, LIGHT_ENTITY, LightFlags, PLAYER_START,
    PREFAB_ENTITY, PREFAB_LIGHTMAP, PROP, PropFlags, SCENE_ENTITY, SCENE_HEADER, SCENE_MAGIC,
    SCENE_SETTINGS, SCENE_VERSION, TRANSFORM, TRIGGER_ENTITY, INVALID_INDEX,
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
class Prop:
    mesh: AssetReference
    transform: Transform = field(default_factory=Transform)
    materials: tuple[AssetReference, ...] = ()
    lightmap: AssetReference | None = None
    lightmap_scale: tuple[float, float] = (1.0, 1.0)
    lightmap_offset: tuple[float, float] = (0.0, 0.0)
    lightmap_rgbm_range: float = 8.0
    dynamic: bool = False
    visible: bool = True
    shadow_only: bool = False
    collision_enabled: bool = False
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
    casts_shadows: bool = True


@dataclass(frozen=True)
class PrefabLightmap:
    object_index: int
    lightmap: AssetReference
    scale: tuple[float, float] = (1.0, 1.0)
    offset: tuple[float, float] = (0.0, 0.0)
    rgbm_range: float = 8.0


@dataclass(frozen=True)
class PrefabEntity:
    prefab: AssetReference
    transform: Transform = field(default_factory=Transform)
    lightmaps: tuple[PrefabLightmap, ...] = ()


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


EntityData = Union[EmptyEntity, Prop, CameraEntity,
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
    gravity: tuple[float, float, float] = (0.0, 0.0, -9.81)
    active_camera_entity: int | None = None


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
    Prop: ("prop", PROP),
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
    if isinstance(value, Prop):
        result = (value.mesh, *value.materials)
        if value.lightmap is not None:
            result += (value.lightmap,)
        return result
    if isinstance(value, PrefabEntity):
        return (value.prefab, *(binding.lightmap for binding in value.lightmaps))
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


def _valid_lightmap_binding(scale: tuple[float, float], offset: tuple[float, float],
                            rgbm_range: float) -> bool:
    return (scale[0] > 0.0 and scale[1] > 0.0 and
            offset[0] >= 0.0 and offset[1] >= 0.0 and
            offset[0] + scale[0] <= 1.0 and
            offset[1] + scale[1] <= 1.0 and rgbm_range > 0.0)


def _pack_entity(value: EntityData, assets: dict[AssetReference, int], materials: list[int],
                 prefab_lightmaps: bytearray) -> bytes:
    transform = _transform_values(value.transform)
    if isinstance(value, EmptyEntity):
        return TRANSFORM.pack(*transform)
    if isinstance(value, Prop):
        first = len(materials) if value.materials else INVALID_INDEX
        materials.extend(_asset_index(item, assets) for item in value.materials)
        lightmap = INVALID_INDEX if value.lightmap is None else _asset_index(value.lightmap, assets)
        if value.lightmap is not None and value.dynamic:
            raise ValueError("only a static prop may have a baked lightmap")
        if value.lightmap_rgbm_range <= 0.0:
            raise ValueError("lightmap RGBM range must be positive")
        if value.lightmap is not None and not _valid_lightmap_binding(
                value.lightmap_scale, value.lightmap_offset, value.lightmap_rgbm_range):
            raise ValueError("lightmap atlas transform must stay inside zero-to-one UV space")
        if value.collision_enabled and any(size <= 0.0 for size in value.collision_half_extents):
            raise ValueError("prop collision dimensions must be positive")
        if value.dynamic and value.collision_enabled and value.mass <= 0.0:
            raise ValueError("dynamic prop mass must be positive")
        flags = (PropFlags.VISIBLE if value.visible else PropFlags(0))
        if value.collision_enabled:
            flags |= PropFlags.COLLISION_ENABLED
        if value.dynamic:
            flags |= PropFlags.DYNAMIC
        if value.shadow_only:
            flags |= PropFlags.SHADOW_ONLY
        return PROP.pack(*transform, _asset_index(value.mesh, assets), first, len(value.materials),
                         lightmap, *value.lightmap_scale, *value.lightmap_offset,
                         value.lightmap_rgbm_range, int(flags),
                         *value.collision_half_extents, value.mass)
    if isinstance(value, CameraEntity):
        if value.near_clip <= 0.0 or value.far_clip <= value.near_clip:
            raise ValueError("camera clip distances are invalid")
        return CAMERA_ENTITY.pack(*transform, value.projection, value.vertical_fov_radians,
                                  value.orthographic_size, value.near_clip, value.far_clip, int(value.enabled))
    if isinstance(value, LightEntity):
        if value.intensity < 0.0 or value.range < 0.0:
            raise ValueError("light intensity and range must be nonnegative")
        flags = (int(LightFlags.ENABLED) if value.enabled else 0) | \
            (int(LightFlags.CASTS_SHADOW) if value.casts_shadows else 0)
        return LIGHT_ENTITY.pack(*transform, value.light_type, value.mode, *value.color,
                                 value.intensity, value.range, value.inner_angle_radians,
                                 value.outer_angle_radians, *value.area_size, flags)
    if isinstance(value, PrefabEntity):
        first = len(prefab_lightmaps) // PREFAB_LIGHTMAP.size if value.lightmaps else INVALID_INDEX
        previous_index = -1
        for binding in sorted(value.lightmaps, key=lambda item: item.object_index):
            if binding.object_index < 0 or binding.object_index > 0xffffffff:
                raise ValueError("prefab lightmap object index is invalid")
            if binding.object_index == previous_index:
                raise ValueError("prefab lightmap object index is duplicated")
            if not _valid_lightmap_binding(binding.scale, binding.offset, binding.rgbm_range):
                raise ValueError("prefab lightmap binding is invalid")
            prefab_lightmaps.extend(PREFAB_LIGHTMAP.pack(
                binding.object_index, _asset_index(binding.lightmap, assets),
                *binding.scale, *binding.offset, binding.rgbm_range))
            previous_index = binding.object_index
        return PREFAB_ENTITY.pack(
            *transform, _asset_index(value.prefab, assets), first, len(value.lightmaps))
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
        if not isinstance(entities[scene.settings.active_camera_entity].data, CameraEntity):
            raise ValueError("active camera entity must reference a camera")
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
        entity_rows.extend(SCENE_ENTITY.pack(class_offset, class_size,
                                             name_offset, name_size, entity.flags))
        grouped.setdefault(type(entity.data), []).append((index, entity))

    output = bytearray(SCENE_HEADER.size)
    settings_offset = _append_aligned(output, SCENE_SETTINGS.pack(
        *scene.settings.ambient_color, scene.settings.exposure, *scene.settings.gravity,
        active_camera, 0, 0, 0, 0))
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
        prefab_lightmaps = bytearray()
        records = b"".join(_pack_entity(
            row[1].data, asset_indices, materials, prefab_lightmaps) for row in rows)
        records_offset = _append_aligned(output, records)
        if materials and prefab_lightmaps:
            raise ValueError("entity class auxiliary data has conflicting layouts")
        auxiliary = (struct.pack(f"<{len(materials)}I", *materials) if materials
                     else bytes(prefab_lightmaps))
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
