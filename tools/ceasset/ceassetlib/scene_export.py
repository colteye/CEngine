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

from .assetfile import make_asset_desc, write_binary_asset
from .formats import AssetType
from .game_schema import SchemaEntity, entity_struct, load_bundled_game
from .scene_format import INVALID_INDEX
from .wire import pack_record

GAME_SCHEMA = load_bundled_game()


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
class Reference:
    """A complete project-relative asset reference."""

    asset_type: AssetType
    path: str
    guid: bytes

    def normalized(self) -> "Reference":
        """TODO: Describe `normalized`.

        Returns:
            TODO: Describe the produced value.
        """
        if len(self.guid) != 16:
            raise ValueError("asset GUID must contain 16 bytes")
        if self.asset_type in (AssetType.UNKNOWN, AssetType.SCENE):
            raise ValueError("scene reference has an invalid asset type")
        return Reference(self.asset_type, normalize_asset_path(self.path), self.guid)


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
    active_entity: int | None = None


@dataclass(frozen=True)
class EntityConnection:
    """TODO: Describe `EntityConnection`."""

    source_entity: int
    event: str
    target_entity: int
    action: str
    delay_seconds: float = 0.0
    parameter: str = ""
    times_to_fire: int = 0


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


def _transform_values(value: Transform) -> tuple[float, ...]:
    """TODO: Describe `_transform_values`.

    Args:
        value: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    return (*value.position, *value.rotation, *value.scale)


def _references(value: SchemaEntity) -> tuple[Reference, ...]:
    """TODO: Describe `_references`.

    Args:
        value: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    result: list[Reference] = []
    for field in value.schema["fields"]:
        field_type = field["type"]
        if field_type == "asset":
            reference = value.values.get(field["name"])
            if reference is not None:
                result.append(reference)
        elif field_type == "asset_list":
            result.extend(value.values.get(field["name"], ()))
    return tuple(result)


def _collect_assets(entities: list[EntityDescription]) -> tuple[list[Reference], dict[Reference, int]]:
    """TODO: Describe `_collect_assets`.

    Args:
        entities: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    by_path: dict[str, Reference] = {}
    for entity in entities:
        for reference in _references(entity.data):
            normalized = reference.normalized()
            previous = by_path.get(normalized.path)
            if previous is not None and previous != normalized:
                raise ValueError(f"conflicting asset references for {normalized.path}")
            by_path[normalized.path] = normalized
    assets = sorted(by_path.values(), key=lambda item: (item.path, int(item.asset_type), item.guid))
    return assets, {asset: index for index, asset in enumerate(assets)}


def _asset_index(reference: Reference, indices: dict[Reference, int]) -> int:
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
    assets: dict[Reference, int],
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
    if sum(entity.data.classname == "fog" and
           bool(entity.data.values.get("enabled", True)) and bool(entity.flags & 1)
           for entity in entities) > 1:
        raise ValueError("scene may contain at most one enabled exponential height fog")
    if sum(entity.data.classname == "post_process" and bool(entity.flags & 1)
           for entity in entities) > 1:
        raise ValueError("scene may contain at most one enabled post-process entity")
    if sum(entity.data.classname == "audio_environment" and bool(entity.flags & 1)
           for entity in entities) > 1:
        raise ValueError(
            "scene may contain at most one enabled audio-environment entity")

    assets, asset_indices = _collect_assets(entities)
    active_entity = INVALID_INDEX
    if scene.settings.active_entity is not None:
        if not 0 <= scene.settings.active_entity < len(entities):
            raise ValueError("active entity index is invalid")
        if not bool(entities[scene.settings.active_entity].flags & 1):
            raise ValueError("active entity must be enabled")
        if entities[scene.settings.active_entity].data.classname not in {
                "camera", "player"}:
            raise ValueError(
                "active entity must reference a camera or player")
        active_entity = scene.settings.active_entity

    asset_rows = [
        {"guid": asset.guid, "type": int(asset.asset_type), "path": asset.path}
        for asset in assets
    ]
    entity_rows: list[dict[str, object]] = []
    grouped: dict[str, tuple[struct.Struct, list[tuple[int, EntityDescription]]]] = {}
    for index, entity in enumerate(entities):
        classname, record_struct = _class_info(entity.data)
        entity_rows.append({
            "class_name": classname,
            "name": entity.name,
            "flags": entity.flags,
        })
        if classname not in grouped:
            grouped[classname] = (record_struct, [])
        elif grouped[classname][0].format != record_struct.format:
            raise ValueError(f"entity classname has conflicting schemas: {classname}")
        grouped[classname][1].append((index, entity))

    groups = sorted(grouped.items())
    class_rows: list[dict[str, object]] = []
    for classname, (record_struct, rows) in groups:
        materials: list[int] = []
        records = b"".join(_pack_schema_entity(
            row[1].data, asset_indices, materials, len(entities))
            for row in rows)
        class_rows.append({
            "class_name": classname,
            "version": int(rows[0][1].data.schema["version"]),
            "stride": record_struct.size,
            "entities": [row[0] for row in rows],
            "records": records,
            "assets": materials,
        })

    connection_rows: list[dict[str, object]] = []
    for connection in sorted(scene.connections,
            key=lambda item: (
                item.source_entity, item.event, item.target_entity, item.action,
                item.delay_seconds, item.parameter, item.times_to_fire)):
        if not 0 <= connection.source_entity < len(entities) or not 0 <= connection.target_entity < len(entities):
            raise ValueError("scene connection entity index is invalid")
        if not connection.event or not connection.action:
            raise ValueError("scene connection names must not be empty")
        if not math.isfinite(connection.delay_seconds) or connection.delay_seconds < 0.0:
            raise ValueError("scene connection delay must be finite and nonnegative")
        if connection.times_to_fire < 0:
            raise ValueError("scene connection fire count must be nonnegative")
        source_schema = entities[connection.source_entity].data.schema
        target_schema = entities[connection.target_entity].data.schema
        source_outputs = {
            str(item["name"]) for item in source_schema.get("outputs", ())
        } | {"OnEnabled", "OnDisabled"}
        target_inputs = {
            str(item["name"]) for item in target_schema.get("inputs", ())
        } | {"Enable", "Disable", "Toggle"}
        if connection.event not in source_outputs:
            raise ValueError(
                f"{entities[connection.source_entity].data.classname} "
                f"has no output {connection.event}")
        if connection.action not in target_inputs:
            raise ValueError(
                f"{entities[connection.target_entity].data.classname} "
                f"has no input {connection.action}")
        connection_rows.append({
            "source": connection.source_entity,
            "target": connection.target_entity,
            "event": connection.event,
            "action": connection.action,
            "parameter": connection.parameter,
            "delay": connection.delay_seconds,
            "times_to_fire": connection.times_to_fire,
        })

    return pack_record(GAME_SCHEMA, "scene", {
        "settings": {
            "ambient_color": scene.settings.ambient_color,
            "exposure": scene.settings.exposure,
            "gravity": scene.settings.gravity,
            "active_entity": active_entity,
        },
        "assets": asset_rows,
        "entities": entity_rows,
        "classes": class_rows,
        "connections": connection_rows,
    })


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
