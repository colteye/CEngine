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
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Iterable

from .collection_export import blender_to_engine_matrix_rows, object_role
from .formats import AssetType, asset_type_for_name
from .game_schema import (
    GameSchema, SchemaEntity, load_bundled_game, make_schema_entity,
)
from .ids import guid_from_stable_name
from .scene_export import (
    Reference, EntityConnection, EntityDescription,
    SceneDescription, SceneSettings, Transform,
)


@dataclass(frozen=True)
class LightmapPlacement:
    """TODO: Describe `LightmapPlacement`."""

    texture: Path
    scale: tuple[float, float]
    offset: tuple[float, float]
    rgbm_range: float = 8.0


def blender_scene_settings(blender: object) -> SceneSettings:
    """TODO: Describe `blender_scene_settings`.

    Args:
        blender: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    scene = getattr(getattr(blender, "context", None), "scene", None)
    world = getattr(scene, "world", None)
    color = tuple(float(value) for value in getattr(world, "color", (0.05, 0.05, 0.05)))[:3]
    strength = 1.0
    node_tree = getattr(world, "node_tree", None)
    for node in getattr(node_tree, "nodes", ()) if node_tree is not None else ():
        if getattr(node, "type", "") != "BACKGROUND":
            continue
        inputs = getattr(node, "inputs", None)
        getter = getattr(inputs, "get", None)
        color_socket = getter("Color") if callable(getter) else None
        strength_socket = getter("Strength") if callable(getter) else None
        if color_socket is not None:
            color = tuple(float(value) for value in color_socket.default_value)[:3]
        if strength_socket is not None:
            strength = max(0.0, float(strength_socket.default_value))
        break
    exposure = float(getattr(getattr(scene, "view_settings", None), "exposure", 0.0))
    return SceneSettings(
        ambient_color=tuple(max(0.0, component * strength) for component in color),
        exposure=2.0 ** exposure,
    )


def _property(obj: object, name: str, default: object = None) -> object:
    """TODO: Describe `_property`.

    Args:
        obj: TODO: Describe this parameter.
        name: TODO: Describe this parameter.
        default: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    getter = getattr(obj, "get", None)
    return getter(name, default) if callable(getter) else default


def _property_name(field: dict[str, object]) -> str:
    """TODO: Describe `_property_name`.

    Args:
        field: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    return str(field.get("blender_property", f"ce_{field['name']}"))


def _native_schema_value(
    obj: object,
    classname: str,
    field: dict[str, object],
    default: object,
) -> object:
    """TODO: Describe `_native_schema_value`.

    Args:
        obj: TODO: Describe this parameter.
        classname: TODO: Describe this parameter.
        field: TODO: Describe this parameter.
        default: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    name = str(field["name"])
    data = getattr(obj, "data", None)
    if classname in {"camera", "player"} and \
            getattr(obj, "type", "") == "CAMERA":
        if name == "vertical_fov_radians":
            return getattr(data, "angle_y", default)
        if name == "near_clip":
            return getattr(data, "clip_start", default)
        if name == "far_clip":
            return getattr(data, "clip_end", default)
    if classname == "audio_source" and getattr(obj, "type", "") == "SPEAKER":
        native = {
            "gain": "volume",
            "pitch": "pitch",
            "min_distance": "distance_reference",
            "max_distance": "distance_max",
            "cone_inner_angle": "cone_angle_inner",
            "cone_outer_angle": "cone_angle_outer",
            "cone_outer_gain": "cone_volume_outer",
        }.get(name)
        if native is not None:
            return getattr(data, native, default)
    return _property(obj, _property_name(field), default)


def _enum_value(field: dict[str, object], value: object) -> int:
    """TODO: Describe `_enum_value`.

    Args:
        field: TODO: Describe this parameter.
        value: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    values = field.get("values", {})
    if isinstance(value, str):
        normalized = value.replace("_", "").lower()
        for name, numeric in values.items():
            if str(name).replace("_", "").lower() == normalized:
                return int(numeric)
        raise ValueError(f"{value!r} is not a valid {field['name']} value")
    numeric = int(value)
    if numeric not in values.values():
        raise ValueError(f"{numeric} is not a valid {field['name']} value")
    return numeric


def schema_entity(
    obj: object,
    schema: dict[str, object],
    transform: Transform,
    asset_path: Callable[[Path], str] | None = None,
    resolve_asset_path: Callable[[str], Path] | None = None,
    entity_indices: dict[str, int] | None = None,
    asset_overrides: dict[str, Reference | tuple[Reference, ...] | None] | None = None,
) -> SchemaEntity:
    """TODO: Describe `schema_entity`.

    Args:
        obj: TODO: Describe this parameter.
        schema: TODO: Describe this parameter.
        transform: TODO: Describe this parameter.
        asset_path: TODO: Describe this parameter.
        resolve_asset_path: TODO: Describe this parameter.
        entity_indices: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    values: dict[str, object] = {}
    for field in schema["fields"]:
        field_type = field["type"]
        name = str(field["name"])
        if field_type == "transform":
            values[name] = transform
            continue
        if field_type == "flags":
            for member in field["members"]:
                values[str(member["name"])] = bool(_property(
                    obj,
                    str(member.get(
                        "blender_property", f"ce_{member['name']}")),
                    member.get("default", False),
                ))
            continue
        default = field.get("default")
        value = _native_schema_value(
            obj, str(schema["classname"]), field, default)
        if field_type == "enum":
            value = _enum_value(field, value)
        elif field_type in ("f32",):
            value = float(value)
        elif field_type in ("u32",):
            value = int(value)
        elif field_type == "entity":
            reference = str(value or "")
            if entity_indices is None or reference not in entity_indices:
                raise ValueError(
                    f"{schema['classname']}.{name} references no physics "
                    f"entity named {reference!r}")
            value = entity_indices[reference]
        elif field_type == "bool":
            value = bool(value)
        elif field_type in ("vec2", "vec3"):
            value = tuple(float(component) for component in value)
            expected = 2 if field_type == "vec2" else 3
            if len(value) != expected:
                raise ValueError(
                    f"{schema['classname']}.{name} must contain {expected} values")
        elif field_type in ("asset", "asset_list"):
            if asset_overrides is not None and name in asset_overrides:
                values[name] = asset_overrides[name]
                continue
            if asset_path is None or resolve_asset_path is None:
                raise ValueError(
                    f"generic Blender entity assets require a path resolver: "
                    f"{schema['classname']}.{name}")
            asset_type = asset_type_for_name(str(field.get("asset_type", "")))
            if asset_type == AssetType.UNKNOWN:
                raise ValueError(
                    f"{schema['classname']}.{name} uses an unknown asset type")
            authored_paths = (
                tuple(part.strip() for part in str(value or "").replace(
                    ";", "\n").splitlines() if part.strip())
                if field_type == "asset_list"
                else (() if not value else (str(value),))
            )
            references = tuple(
                _reference(
                    asset_type,
                    resolve_asset_path(authored),
                    asset_path,
                )
                for authored in authored_paths
            )
            value = references if field_type == "asset_list" else (
                references[0] if references else None)
        values[name] = value
    return SchemaEntity(schema, values)


def _quaternion(matrix: list[list[float]]) -> tuple[float, float, float, float]:
    """TODO: Describe `_quaternion`.

    Args:
        matrix: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    trace = matrix[0][0] + matrix[1][1] + matrix[2][2]
    if trace > 0.0:
        scale = math.sqrt(trace + 1.0) * 2.0
        return ((matrix[2][1] - matrix[1][2]) / scale,
                (matrix[0][2] - matrix[2][0]) / scale,
                (matrix[1][0] - matrix[0][1]) / scale, 0.25 * scale)
    if matrix[0][0] > matrix[1][1] and matrix[0][0] > matrix[2][2]:
        scale = math.sqrt(1.0 + matrix[0][0] - matrix[1][1] - matrix[2][2]) * 2.0
        return (0.25 * scale, (matrix[0][1] + matrix[1][0]) / scale,
                (matrix[0][2] + matrix[2][0]) / scale,
                (matrix[2][1] - matrix[1][2]) / scale)
    if matrix[1][1] > matrix[2][2]:
        scale = math.sqrt(1.0 + matrix[1][1] - matrix[0][0] - matrix[2][2]) * 2.0
        return ((matrix[0][1] + matrix[1][0]) / scale, 0.25 * scale,
                (matrix[1][2] + matrix[2][1]) / scale,
                (matrix[0][2] - matrix[2][0]) / scale)
    scale = math.sqrt(1.0 + matrix[2][2] - matrix[0][0] - matrix[1][1]) * 2.0
    return ((matrix[0][2] + matrix[2][0]) / scale,
            (matrix[1][2] + matrix[2][1]) / scale, 0.25 * scale,
            (matrix[1][0] - matrix[0][1]) / scale)


def object_transform(obj: object) -> Transform:
    """TODO: Describe `object_transform`.

    Args:
        obj: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    flat = blender_to_engine_matrix_rows(getattr(obj, "matrix_world", None))
    matrix = [flat[index:index + 4] for index in range(0, 16, 4)]
    scale = [math.sqrt(sum(matrix[row][column] ** 2 for row in range(3))) for column in range(3)]
    if any(value <= 1.0e-8 for value in scale):
        raise ValueError(f"object has a degenerate world transform: {getattr(obj, 'name', '<unnamed>')}")
    rotation = [[matrix[row][column] / scale[column] for column in range(3)] for row in range(3)]
    determinant = (
        rotation[0][0] * (rotation[1][1] * rotation[2][2] - rotation[1][2] * rotation[2][1])
        - rotation[0][1] * (rotation[1][0] * rotation[2][2] - rotation[1][2] * rotation[2][0])
        + rotation[0][2] * (rotation[1][0] * rotation[2][1] - rotation[1][1] * rotation[2][0])
    )
    if determinant < 0.0:
        scale[0] = -scale[0]
        for row in range(3):
            rotation[row][0] = -rotation[row][0]
    return Transform(
        position=(matrix[0][3], matrix[1][3], matrix[2][3]),
        rotation=_quaternion(rotation),
        scale=(scale[0], scale[1], scale[2]),
    )


def _reference(asset_type: AssetType, path: Path, asset_path: Callable[[Path], str]) -> Reference:
    """TODO: Describe `_reference`.

    Args:
        asset_type: TODO: Describe this parameter.
        path: TODO: Describe this parameter.
        asset_path: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    relative = asset_path(path)
    return Reference(asset_type, relative, guid_from_stable_name(relative))


def _mesh_armature(obj: object) -> object | None:
    parent = getattr(obj, "parent", None)
    if getattr(parent, "type", "") == "ARMATURE":
        return parent
    for modifier in getattr(obj, "modifiers", ()):
        if getattr(modifier, "type", "") == "ARMATURE":
            armature = getattr(modifier, "object", None)
            if armature is not None:
                return armature
    return None


def scene_description(
    objects: Iterable[object],
    mesh_outputs: dict[str, list[tuple[Path, str]]],
    material_outputs: dict[str, Path],
    asset_path: Callable[[Path], str],
    lightmaps: dict[str, LightmapPlacement] | None = None,
    skybox_outputs: dict[str, Path] | None = None,
    physics_outputs: dict[str, Path] | None = None,
    audio_outputs: dict[str, Path] | None = None,
    game_schema: GameSchema | None = None,
    resolve_asset_path: Callable[[str], Path] | None = None,
    skeleton_outputs: dict[str, Path] | None = None,
    animation_outputs: dict[str, list[Path]] | None = None,
) -> SceneDescription:
    """TODO: Describe `scene_description`.

    Args:
        objects: TODO: Describe this parameter.
        mesh_outputs: TODO: Describe this parameter.
        material_outputs: TODO: Describe this parameter.
        asset_path: TODO: Describe this parameter.
        lightmaps: TODO: Describe this parameter.
        skybox_outputs: TODO: Describe this parameter.
        physics_outputs: TODO: Describe this parameter.
        game_schema: TODO: Describe this parameter.
        resolve_asset_path: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    objects = tuple(objects)
    entities: list[EntityDescription] = []
    schemas = game_schema or load_bundled_game()
    deferred_constraints: list[object] = []
    physics_entity_indices: dict[str, int] = {}
    object_entity_indices: dict[str, int] = {}
    collision_helper_names = {
        str(_property(obj, "ce_collision_object", "") or "")
        for obj in objects
        if str(_property(obj, "ce_collision_object", "") or "")
    }
    for obj in sorted(objects, key=lambda value: str(getattr(value, "name", ""))):
        name = str(getattr(obj, "name", ""))
        obj_type = str(getattr(obj, "type", ""))
        transform = object_transform(obj)
        classname = str(_property(obj, "ce_classname", "") or "")
        if classname == "physics_constraint":
            if obj_type != "EMPTY":
                raise ValueError(
                    f"physics constraint must be a Blender empty: {name}")
            deferred_constraints.append(obj)
            continue

        if obj_type == "MESH":
            if bool(_property(obj, "ce_collision_helper", False)) or \
                    name in collision_helper_names:
                continue
            if classname in {"collider", "trigger_volume"}:
                collision_output = (physics_outputs or {}).get(name)
                if collision_output is None:
                    raise ValueError(
                        f"physics entity has no generated .cphys: {name}")
                schema = schemas.entity(classname)
                if schema is None:
                    raise ValueError(
                        f"game schema does not define {classname}")
                data = schema_entity(
                    obj, schema, transform, asset_path, resolve_asset_path,
                    asset_overrides={
                        "collision": _reference(
                            AssetType.PHYSICS, collision_output, asset_path),
                    })
                object_entity_indices[name] = len(entities)
                if classname == "collider":
                    physics_entity_indices[name] = len(entities)
                entities.append(EntityDescription(data, name))
                continue
            outputs = mesh_outputs.get(name, ())
            if not outputs:
                raise ValueError(f"mesh entity has no generated .cmesh: {name}")
            if classname not in ("", "prop"):
                raise ValueError(f"unsupported mesh entity classname: {classname}")
            motion_field = next(
                field for field in schemas.entity("prop")["fields"]
                if field["name"] == "motion")
            authored_motion = _property(
                obj, "ce_physics_motion", "None")
            motion = _enum_value(motion_field, authored_motion)
            collision_output = (physics_outputs or {}).get(name)
            if (collision_output is None) != (motion == 0):
                raise ValueError(
                    f"physics motion and cooked collision disagree: {name}")
            if motion in (2, 3) and len(outputs) != 1:
                raise ValueError(
                    f"movable prop requires one material section: {name}")
            placement = lightmaps.get(name) if lightmaps is not None else None
            if placement is not None and motion in (2, 3):
                raise ValueError(f"a movable prop may not have a baked lightmap: {name}")
            armature = _mesh_armature(obj)
            skeleton_output = (
                (skeleton_outputs or {}).get(
                    str(getattr(armature, "name", "")))
                if armature is not None else None
            )
            clips = (
                (animation_outputs or {}).get(
                    str(getattr(armature, "name", "")), ())
                if armature is not None else ()
            )
            if armature is not None and skeleton_output is None:
                raise ValueError(
                    f"skinned mesh has no generated .cskel: {name}")
            object_entity_indices[name] = len(entities)
            for section_index, (output, material_name) in enumerate(outputs):
                material_output = material_outputs.get(material_name)
                if material_output is None:
                    raise ValueError(f"mesh entity has no generated .cmat for {material_name}: {name}")
                data = make_schema_entity(
                    schemas, "prop",
                    mesh=_reference(AssetType.MESH, output, asset_path),
                    skeleton=_reference(
                        AssetType.SKELETON, skeleton_output, asset_path
                    ) if skeleton_output is not None else None,
                    animation=_reference(
                        AssetType.ANIMATION, sorted(clips)[0], asset_path
                    ) if clips else None,
                    animation_playback_rate=float(_property(
                        obj, "ce_animation_playback_rate", 1.0)),
                    animation_looping=bool(_property(
                        obj, "ce_animation_looping", True)),
                    transform=transform,
                    materials=(_reference(AssetType.MATERIAL, material_output, asset_path),),
                    lightmap=_reference(AssetType.TEXTURE, placement.texture, asset_path) if placement else None,
                    lightmap_scale=placement.scale if placement else (1.0, 1.0),
                    lightmap_offset=placement.offset if placement else (0.0, 0.0),
                    lightmap_rgbm_range=placement.rgbm_range if placement else 8.0,
                    collision=_reference(
                        AssetType.PHYSICS, collision_output, asset_path
                    ) if collision_output is not None and section_index == 0 else None,
                    motion=motion if section_index == 0 else 0,
                    mass=float(_property(obj, "ce_mass", 1.0)),
                    friction=float(_property(obj, "ce_friction", 0.6)),
                    restitution=float(_property(obj, "ce_restitution", 0.05)),
                    linear_damping=float(_property(
                        obj, "ce_linear_damping", 0.05)),
                    angular_damping=float(_property(
                        obj, "ce_angular_damping", 0.05)),
                    gravity_factor=float(_property(
                        obj, "ce_gravity_factor", 1.0)),
                    collision_layer=int(_property(
                        obj, "ce_collision_layer", 0)),
                    initial_linear_velocity=tuple(float(value) for value in _property(
                        obj, "ce_initial_linear_velocity", (0.0, 0.0, 0.0))),
                    initial_angular_velocity=tuple(float(value) for value in _property(
                        obj, "ce_initial_angular_velocity", (0.0, 0.0, 0.0))),
                    visible=not bool(getattr(obj, "hide_render", False)),
                    shadow_only=bool(_property(
                        obj, "ce_shadow_only", object_role(obj) == "occluder")),
                    sensor=bool(_property(obj, "ce_sensor", False)),
                    continuous=bool(_property(obj, "ce_continuous", False)),
                    allow_sleeping=bool(_property(obj, "ce_allow_sleeping", True)),
                    lock_translation_x=bool(_property(
                        obj, "ce_lock_translation_x", False)),
                    lock_translation_y=bool(_property(
                        obj, "ce_lock_translation_y", False)),
                    lock_translation_z=bool(_property(
                        obj, "ce_lock_translation_z", False)),
                    lock_rotation_x=bool(_property(
                        obj, "ce_lock_rotation_x", False)),
                    lock_rotation_y=bool(_property(
                        obj, "ce_lock_rotation_y", False)),
                    lock_rotation_z=bool(_property(
                        obj, "ce_lock_rotation_z", False)),
                )
                entity_name = name if len(outputs) == 1 else f"{name}:{material_name}"
                if section_index == 0 and motion != 0:
                    physics_entity_indices[name] = len(entities)
                entities.append(EntityDescription(data, entity_name))
            continue
        elif obj_type == "LIGHT":
            if classname not in ("", "light"):
                raise ValueError(
                    f"unsupported Blender light entity classname: {classname}")
            light = getattr(obj, "data", None)
            light_type = {"POINT": 0, "SUN": 1, "SPOT": 2, "AREA": 3}.get(
                str(getattr(light, "type", "POINT")), 0)
            mode = {"realtime": 0, "baked": 1, "mixed": 2}.get(
                str(_property(obj, "ce_light_mode", "realtime")).lower(), 0)
            color = tuple(float(value) for value in getattr(light, "color", (1.0, 1.0, 1.0)))
            spot_size = float(getattr(light, "spot_size", 0.7853982))
            spot_blend = float(getattr(light, "spot_blend", 0.0))
            data = make_schema_entity(
                schemas, "light",
                transform=transform, type=light_type, mode=mode, color=color,
                intensity=float(getattr(light, "energy", 1.0)),
                range=float(getattr(light, "cutoff_distance", 10.0)),
                inner_angle_radians=spot_size * max(0.0, 1.0 - spot_blend),
                outer_angle_radians=spot_size,
                area_size=(float(getattr(light, "size", 1.0)),
                    float(getattr(light, "size_y", 1.0))),
                enabled=not bool(getattr(obj, "hide_render", False)),
                casts_shadows=bool(getattr(light, "use_shadow", True)))
        elif obj_type == "SPEAKER":
            if classname not in ("", "audio_source"):
                raise ValueError(
                    f"unsupported Blender speaker entity classname: {classname}")
            output = (audio_outputs or {}).get(name)
            if output is None:
                raise ValueError(
                    f"audio source has no exported audio file: {name}")
            schema = schemas.entity("audio_source")
            if schema is None:
                raise ValueError("game schema does not define audio sources")
            data = schema_entity(
                obj, schema, transform, asset_path, resolve_asset_path,
                asset_overrides={
                    "audio": _reference(AssetType.AUDIO, output, asset_path),
                })
        elif obj_type == "EMPTY" and getattr(obj, "instance_collection", None) is not None:
            # Blender's dependency graph exposes the expanded objects as normal
            # mesh entries. The collection marker itself is not an entity.
            continue
        elif obj_type in ("EMPTY", "CAMERA"):
            if obj_type == "CAMERA" and not classname:
                classname = "camera"
            if classname in ("", "empty"):
                continue
            elif classname == "skybox":
                output = (skybox_outputs or {}).get(name)
                if output is None:
                    raise ValueError(f"skybox has no generated HDR panorama: {name}")
                data = make_schema_entity(
                    schemas, "skybox",
                    panorama=_reference(
                        AssetType.TEXTURE, output, asset_path),
                    transform=transform,
                    intensity=float(_property(obj, "ce_intensity", 1.0)),
                    rotation_radians=float(_property(
                        obj, "ce_rotation_radians", 0.0)),
                    enabled=bool(_property(obj, "ce_enabled", True)))
            else:
                schema = schemas.entity(classname)
                if schema is None:
                    raise ValueError(f"unsupported empty entity classname: {classname}")
                data = schema_entity(
                    obj, schema, transform, asset_path, resolve_asset_path)
        else:
            continue
        object_entity_indices[name] = len(entities)
        entities.append(EntityDescription(data, name))

    constraint_schema = schemas.entity("physics_constraint")
    for obj in deferred_constraints:
        if constraint_schema is None:
            raise ValueError("game schema does not define physics constraints")
        name = str(getattr(obj, "name", ""))
        data = schema_entity(
            obj, constraint_schema, object_transform(obj),
            asset_path, resolve_asset_path, physics_entity_indices)
        if data.values["first_entity"] == data.values["second_entity"]:
            raise ValueError(
                f"physics constraint must reference two different props: {name}")
        entities.append(EntityDescription(data, name))
        object_entity_indices[name] = len(entities) - 1
    if not entities:
        raise ValueError("scene contains no exportable entities")
    connections: list[EntityConnection] = []
    for obj in sorted(objects, key=lambda value: str(getattr(value, "name", ""))):
        source_name = str(getattr(obj, "name", ""))
        if source_name not in object_entity_indices:
            continue
        authored = getattr(obj, "cengine_connections", None)
        if authored is None:
            authored = _property(obj, "ce_connections", ()) or ()
        for connection in authored:
            getter = (
                connection.get
                if callable(getattr(connection, "get", None))
                else lambda key, default=None: getattr(connection, key, default)
            )
            target = getter("target", None)
            target_name = str(getattr(target, "name", target or ""))
            if target_name not in object_entity_indices:
                raise ValueError(
                    f"{source_name} connection target is not an exported entity: "
                    f"{target_name!r}")
            connections.append(EntityConnection(
                object_entity_indices[source_name],
                str(getter("event", "") or ""),
                object_entity_indices[target_name],
                str(getter("action", "") or ""),
                float(getter("delay_seconds", 0.0)),
                str(getter("parameter", "") or ""),
                int(getter("times_to_fire", 0)),
            ))
    return SceneDescription(tuple(entities), connections=tuple(connections))
