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

import posixpath
import math
import struct
from dataclasses import dataclass, field
from pathlib import Path

from .assetfile import align_up, make_asset_desc, write_binary_asset
from .formats import AssetType
from .game_schema import SchemaEntity, entity_struct
from .scene_format import (
    ASSET_REFERENCE, ENTITY_CLASS_BLOCK,
    ENTITY_CONNECTION, ENTITY_CLASS_BLOCK_REQUIRED,
    SCENE_ENTITY, SCENE_HEADER, SCENE_MAGIC,
    SCENE_SETTINGS, SCENE_VERSION, INVALID_INDEX,
)

TABLE_ALIGNMENT = 16


def normalize_asset_path(path: str) -> str:
    """TODO: Describe `normalize_asset_path`.

    Args:
        path: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    normalized = posixpath.normpath(path.replace("\\", "/"))
    if normalized in ("", ".") or normalized.startswith("/") or normalized == ".." or normalized.startswith("../"):
        raise ValueError(f"asset path must be project-relative: {path}")
    return normalized


@dataclass(frozen=True)
class AssetReference:
    """TODO: Describe `AssetReference`."""

    asset_type: AssetType
    path: str
    guid: bytes

    def normalized(self) -> "AssetReference":
        """TODO: Describe `normalized`.

        Returns:
            TODO: Describe the produced value.
        """
        if len(self.guid) != 16:
            raise ValueError("asset GUID must contain 16 bytes")
        if self.asset_type in (AssetType.UNKNOWN, AssetType.SCENE):
            raise ValueError("scene reference has an invalid asset type")
        return AssetReference(self.asset_type, normalize_asset_path(self.path), self.guid)


@dataclass(frozen=True)
class Transform:
    """TODO: Describe `Transform`."""

    position: tuple[float, float, float] = (0.0, 0.0, 0.0)
    rotation: tuple[float, float, float, float] = (0.0, 0.0, 0.0, 1.0)
    scale: tuple[float, float, float] = (1.0, 1.0, 1.0)


@dataclass(frozen=True)
class EntityDescription:
    """TODO: Describe `EntityDescription`."""

    data: SchemaEntity
    name: str = ""
    flags: int = 1


@dataclass(frozen=True)
class SceneSettings:
    """TODO: Describe `SceneSettings`."""

    ambient_color: tuple[float, float, float] = (0.0, 0.0, 0.0)
    exposure: float = 1.0
    gravity: tuple[float, float, float] = (0.0, 0.0, -9.81)
    active_player_entity: int | None = None


@dataclass(frozen=True)
class EntityConnection:
    """TODO: Describe `EntityConnection`."""

    source_entity: int
    event: str
    target_entity: int
    action: str
    delay_seconds: float = 0.0


@dataclass(frozen=True)
class SceneDescription:
    """TODO: Describe `SceneDescription`."""

    entities: tuple[EntityDescription, ...]
    settings: SceneSettings = field(default_factory=SceneSettings)
    connections: tuple[EntityConnection, ...] = ()


def _class_info(value: SchemaEntity) -> tuple[str, struct.Struct]:
    """TODO: Describe `_class_info`.

    Args:
        value: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    if not isinstance(value, SchemaEntity):
        raise TypeError(f"unsupported entity class: {type(value).__name__}")
    return value.classname, entity_struct(value.schema)


class StringTable:
    """TODO: Describe `StringTable`."""

    def __init__(self) -> None:
        """TODO: Describe `__init__`."""
        self.data = bytearray()
        self.ranges: dict[str, tuple[int, int]] = {}

    def add(self, value: str) -> tuple[int, int]:
        """TODO: Describe `add`.

        Args:
            value: TODO: Describe this parameter.

        Returns:
            TODO: Describe the produced value.
        """
        if value in self.ranges:
            return self.ranges[value]
        encoded = value.encode("utf-8")
        result = (len(self.data), len(encoded))
        self.data.extend(encoded)
        self.ranges[value] = result
        return result


def _transform_values(value: Transform) -> tuple[float, ...]:
    """TODO: Describe `_transform_values`.

    Args:
        value: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    return (*value.position, *value.rotation, *value.scale)


def _append_aligned(output: bytearray, data: bytes) -> int:
    """TODO: Describe `_append_aligned`.

    Args:
        output: TODO: Describe this parameter.
        data: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    offset = align_up(len(output), TABLE_ALIGNMENT)
    output.extend(bytes(offset - len(output)))
    output.extend(data)
    return offset


def _references(value: SchemaEntity) -> tuple[AssetReference, ...]:
    """TODO: Describe `_references`.

    Args:
        value: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    result: list[AssetReference] = []
    for field in value.schema["fields"]:
        field_type = field["type"]
        if field_type == "asset":
            reference = value.values.get(field["name"])
            if reference is not None:
                result.append(reference)
        elif field_type == "asset_list":
            result.extend(value.values.get(field["name"], ()))
    return tuple(result)


def _collect_assets(entities: list[EntityDescription]) -> tuple[list[AssetReference], dict[AssetReference, int]]:
    """TODO: Describe `_collect_assets`.

    Args:
        entities: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
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
    """TODO: Describe `_asset_index`.

    Args:
        reference: TODO: Describe this parameter.
        indices: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    return indices[reference.normalized()]


def _valid_lightmap_binding(scale: tuple[float, float], offset: tuple[float, float],
                            rgbm_range: float) -> bool:
    """TODO: Describe `_valid_lightmap_binding`.

    Args:
        scale: TODO: Describe this parameter.
        offset: TODO: Describe this parameter.
        rgbm_range: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    return (scale[0] > 0.0 and scale[1] > 0.0 and
            offset[0] >= 0.0 and offset[1] >= 0.0 and
            offset[0] + scale[0] <= 1.0 and
            offset[1] + scale[1] <= 1.0 and rgbm_range > 0.0)


def _validate_schema_scalar(
    field: dict[str, object], value: float, values: dict[str, object]
) -> None:
    """TODO: Describe `_validate_schema_scalar`.

    Args:
        field: TODO: Describe this parameter.
        value: TODO: Describe this parameter.
        values: TODO: Describe this parameter.
    """
    if not math.isfinite(value):
        raise ValueError(f"{field['name']} must be finite")
    if "min" in field:
        valid = value > float(field["min"]) if field.get("min_exclusive") \
            else value >= float(field["min"])
        if not valid:
            raise ValueError(f"{field['name']} is below its minimum")
    if "max" in field:
        valid = value < float(field["max"]) if field.get("max_exclusive") \
            else value <= float(field["max"])
        if not valid:
            raise ValueError(f"{field['name']} is above its maximum")
    if "greater_than" in field and value <= float(values[str(field["greater_than"])]):
        raise ValueError(
            f"{field['name']} must be greater than {field['greater_than']}")


def _pack_schema_entity(
    value: SchemaEntity,
    assets: dict[AssetReference, int],
    auxiliary_assets: list[int],
    entity_count: int,
) -> bytes:
    """TODO: Describe `_pack_schema_entity`.

    Args:
        value: TODO: Describe this parameter.
        assets: TODO: Describe this parameter.
        auxiliary_assets: TODO: Describe this parameter.
        entity_count: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    _validate_entity_semantics(value)
    packed: list[object] = []
    values = value.values
    for field in value.schema["fields"]:
        name = str(field["name"])
        field_type = field["type"]
        if field_type == "transform":
            packed.extend(_transform_values(values[name] or Transform()))
        elif field_type == "f32":
            scalar = float(values[name])
            _validate_schema_scalar(field, scalar, values)
            packed.append(scalar)
        elif field_type == "u32":
            scalar = int(values[name])
            if "min" in field and scalar < int(field["min"]):
                raise ValueError(f"{value.classname}.{name} is below its minimum")
            if "max" in field and scalar > int(field["max"]):
                raise ValueError(f"{value.classname}.{name} is above its maximum")
            packed.append(scalar)
        elif field_type == "entity":
            index = int(values[name])
            if not 0 <= index < entity_count:
                raise ValueError(
                    f"{value.classname}.{name} entity reference is invalid")
            packed.append(index)
        elif field_type == "bool":
            packed.append(int(bool(values[name])))
        elif field_type == "enum":
            numeric = int(values[name])
            if numeric not in field["values"].values():
                raise ValueError(f"{value.classname}.{name} is invalid")
            packed.append(numeric)
        elif field_type in ("vec2", "vec3"):
            components = tuple(float(component) for component in values[name])
            expected = 2 if field_type == "vec2" else 3
            if len(components) != expected:
                raise ValueError(
                    f"{value.classname}.{name} must contain {expected} values")
            for component in components:
                _validate_schema_scalar(field, component, values)
            packed.extend(components)
        elif field_type == "asset":
            reference = values.get(name)
            if reference is None:
                if not field.get("optional", False):
                    raise ValueError(f"{value.classname}.{name} is required")
                packed.append(INVALID_INDEX)
            else:
                packed.append(_asset_index(reference, assets))
        elif field_type == "asset_list":
            references = tuple(values.get(name, ()))
            first = len(auxiliary_assets) if references else INVALID_INDEX
            auxiliary_assets.extend(_asset_index(item, assets) for item in references)
            packed.extend((first, len(references)))
        elif field_type == "flags":
            flags = 0
            for member in field["members"]:
                if bool(values.get(member["name"], member.get("default", False))):
                    flags |= 1 << int(member["bit"])
            packed.append(flags)
        else:
            raise ValueError(f"{value.classname}.{name} has an unsupported type")
    return entity_struct(value.schema).pack(*packed)

def _validate_entity_semantics(value: SchemaEntity) -> None:
    """TODO: Describe `_validate_entity_semantics`.

    Args:
        value: TODO: Describe this parameter.
    """
    if value.classname == "physics_constraint":
        fields = value.values
        constraint_type = int(fields["type"])
        motor = int(fields["motor"])
        minimum = float(fields["minimum"])
        maximum = float(fields["maximum"])
        if minimum > maximum:
            raise ValueError(
                "physics constraint minimum must not exceed maximum")
        if motor and constraint_type not in (2, 3):
            raise ValueError(
                "only hinge and slider constraints support motors")
        if motor and float(fields["motor_force_limit"]) <= 0.0:
            raise ValueError(
                "an enabled constraint motor requires a positive force limit")
        return
    if value.classname != "prop":
        return
    fields = value.values
    lightmap = fields.get("lightmap")
    collision = fields.get("collision")
    motion = int(fields.get("motion", 0))
    if (collision is None) != (motion == 0):
        raise ValueError(
            "prop collision and motion must either both be set or both be absent")
    if lightmap is not None and motion in (2, 3):
        raise ValueError("a movable prop may not have a baked lightmap")
    if lightmap is not None and not _valid_lightmap_binding(
            tuple(fields["lightmap_scale"]),
            tuple(fields["lightmap_offset"]),
            float(fields["lightmap_rgbm_range"])):
        raise ValueError(
            "lightmap atlas transform must stay inside zero-to-one UV space")
    lock_names = (
        "lock_translation_x", "lock_translation_y", "lock_translation_z",
        "lock_rotation_x", "lock_rotation_y", "lock_rotation_z",
    )
    if motion == 2 and all(bool(fields.get(name, False)) for name in lock_names):
        raise ValueError("a dynamic prop may not lock every degree of freedom")


def build_scene_payload(scene: SceneDescription) -> bytes:
    """TODO: Describe `build_scene_payload`.

    Args:
        scene: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    entities = list(scene.entities)
    if not entities:
        raise ValueError("scene must contain at least one entity")
    for entity in entities:
        try:
            _class_info(entity.data)
        except TypeError:
            raise ValueError("scene contains an unsupported entity class")
    if sum(entity.data.classname == "skybox" and
           bool(entity.data.values.get("enabled", True)) and bool(entity.flags & 1)
           for entity in entities) > 1:
        raise ValueError("scene may contain at most one enabled skybox")
    if sum(entity.data.classname == "exponential_height_fog" and
           bool(entity.data.values.get("enabled", True)) and bool(entity.flags & 1)
           for entity in entities) > 1:
        raise ValueError("scene may contain at most one enabled exponential height fog")
    if sum(entity.data.classname == "post_process" and bool(entity.flags & 1)
           for entity in entities) > 1:
        raise ValueError("scene may contain at most one enabled post-process entity")

    assets, asset_indices = _collect_assets(entities)
    active_player = INVALID_INDEX
    if scene.settings.active_player_entity is not None:
        if not 0 <= scene.settings.active_player_entity < len(entities):
            raise ValueError("active player entity index is invalid")
        if _class_info(entities[scene.settings.active_player_entity].data)[0] != "player":
            raise ValueError("active player entity must reference a player")
        active_player = scene.settings.active_player_entity

    strings = StringTable()
    asset_rows = bytearray()
    for asset in assets:
        path_offset, path_size = strings.add(asset.path)
        asset_rows.extend(ASSET_REFERENCE.pack(asset.guid, int(asset.asset_type), 0, path_offset, path_size))

    entity_rows = bytearray()
    grouped: dict[str, tuple[struct.Struct, list[tuple[int, EntityDescription]]]] = {}
    for index, entity in enumerate(entities):
        classname, record_struct = _class_info(entity.data)
        class_offset, class_size = strings.add(classname)
        name_offset, name_size = strings.add(entity.name)
        entity_rows.extend(SCENE_ENTITY.pack(class_offset, class_size,
                                             name_offset, name_size, entity.flags))
        if classname not in grouped:
            grouped[classname] = (record_struct, [])
        elif grouped[classname][0].format != record_struct.format:
            raise ValueError(f"entity classname has conflicting schemas: {classname}")
        grouped[classname][1].append((index, entity))

    output = bytearray(SCENE_HEADER.size)
    settings_offset = _append_aligned(output, SCENE_SETTINGS.pack(
        *scene.settings.ambient_color, scene.settings.exposure, *scene.settings.gravity,
        active_player, 0, 0, 0, 0))
    asset_offset = _append_aligned(output, asset_rows) if asset_rows else 0
    entity_offset = _append_aligned(output, entity_rows)
    groups = sorted(grouped.items())
    class_offset = align_up(len(output), TABLE_ALIGNMENT)
    output.extend(bytes(class_offset - len(output) + len(groups) * ENTITY_CLASS_BLOCK.size))

    class_rows = bytearray()
    for classname, (record_struct, rows) in groups:
        classname_offset, classname_size = strings.add(classname)
        indices = struct.pack(f"<{len(rows)}I", *(row[0] for row in rows))
        indices_offset = _append_aligned(output, indices)
        materials: list[int] = []
        records = b"".join(_pack_schema_entity(
            row[1].data, asset_indices, materials, len(entities))
            for row in rows)
        records_offset = _append_aligned(output, records)
        auxiliary = struct.pack(f"<{len(materials)}I", *materials) if materials else b""
        auxiliary_offset = _append_aligned(output, auxiliary) if auxiliary else 0
        class_rows.extend(ENTITY_CLASS_BLOCK.pack(classname_offset, classname_size,
            int(rows[0][1].data.schema["version"]),
            ENTITY_CLASS_BLOCK_REQUIRED, len(rows), record_struct.size,
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
    """TODO: Describe `write_scene`.

    Args:
        path: TODO: Describe this parameter.
        scene: TODO: Describe this parameter.
        stable_name: TODO: Describe this parameter.
        source_hash: TODO: Describe this parameter.
    """
    write_binary_asset(path, make_asset_desc(
        AssetType.SCENE, stable_name, source_hash, build_scene_payload(scene)))
