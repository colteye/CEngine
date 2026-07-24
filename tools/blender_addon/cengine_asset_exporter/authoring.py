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

from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Iterable, Iterator

from ceassetlib.blender_scene import LightmapPlacement
from ceassetlib.game_schema import GameSchema


CLASSNAME_PROPERTY = "ce_classname"
LIGHTMAP_PATH_PROPERTY = "ce_lightmap_path"
LIGHTMAP_SCALE_PROPERTY = "ce_lightmap_scale"
LIGHTMAP_OFFSET_PROPERTY = "ce_lightmap_offset"
LIGHTMAP_RANGE_PROPERTY = "ce_lightmap_rgbm_range"
LIGHTMAP_UV_NAME = "Lightmap"
COLLIDER_PROPERTY = "ce_collider"


@dataclass(frozen=True)
class EntityIssue:
    """TODO: Describe `EntityIssue`."""

    object_name: str
    message: str

    def __str__(self) -> str:
        """TODO: Describe `__str__`.

        Returns:
            TODO: Describe the produced value.
        """
        return f"{self.object_name}: {self.message}"


def display_name(identifier: str) -> str:
    """TODO: Describe `display_name`.

    Args:
        identifier: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    return identifier.replace("_", " ").strip().title()


def property_name(field: dict[str, object]) -> str:
    """TODO: Describe `property_name`.

    Args:
        field: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    return str(field.get("blender_property", f"ce_{field['name']}"))


def entity_classname(obj: object) -> str:
    """TODO: Describe `entity_classname`.

    Args:
        obj: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    getter = getattr(obj, "get", None)
    explicit = str(getter(CLASSNAME_PROPERTY, "") if callable(getter) else "")
    if explicit:
        return explicit
    return {
        "MESH": "prop",
        "LIGHT": "light",
        "CAMERA": "camera",
        "SPEAKER": "audio_source",
    }.get(str(getattr(obj, "type", "")), "")


def native_object_type(classname: str) -> str:
    """TODO: Describe `native_object_type`.

    Args:
        classname: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    return {
        "prop": "MESH",
        "collider": "MESH",
        "trigger_volume": "MESH",
        "light": "LIGHT",
        "camera": "CAMERA",
        "player": "CAMERA",
        "audio_source": "SPEAKER",
    }.get(classname, "EMPTY")


def entity_schema(game_schema: GameSchema, obj: object) -> dict[str, object] | None:
    """TODO: Describe `entity_schema`.

    Args:
        game_schema: TODO: Describe this parameter.
        obj: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    classname = entity_classname(obj)
    return game_schema.entity(classname) if classname else None


def iter_property_fields(
    schema: dict[str, object],
) -> Iterator[tuple[dict[str, object], dict[str, object] | None]]:
    """TODO: Describe `iter_property_fields`.

    Args:
        schema: TODO: Describe this parameter.

    Yields:
        TODO: Describe the produced value.
    """
    for field in schema["fields"]:
        if field["type"] == "transform":
            continue
        if field["type"] == "flags":
            for member in field["members"]:
                yield field, member
            continue
        yield field, None


def _id_property_value(field: dict[str, object], default: object) -> object:
    """TODO: Describe `_id_property_value`.

    Args:
        field: TODO: Describe this parameter.
        default: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    field_type = field["type"]
    if field_type == "enum":
        values = field.get("values", {})
        for name, numeric in values.items():
            if int(numeric) == int(default):
                return str(name)
    if field_type == "asset_list":
        return ""
    if isinstance(default, tuple):
        return list(default)
    return default


def initialize_entity_properties(
    obj: object,
    schema: dict[str, object],
    *,
    overwrite_classname: bool = True,
) -> None:
    """TODO: Describe `initialize_entity_properties`.

    Args:
        obj: TODO: Describe this parameter.
        schema: TODO: Describe this parameter.
        overwrite_classname: TODO: Describe this parameter.
    """
    if overwrite_classname:
        obj[CLASSNAME_PROPERTY] = str(schema["classname"])
    if schema["classname"] in {"prop", "collider", "trigger_volume"} and \
            COLLIDER_PROPERTY not in obj:
        obj[COLLIDER_PROPERTY] = "Box"
    if schema["classname"] in {"prop", "collider", "trigger_volume"} and \
            "ce_collision_object" not in obj:
        obj["ce_collision_object"] = ""
    for field, member in iter_property_fields(schema):
        descriptor = member or field
        key = property_name(descriptor)
        if key in obj:
            continue
        if member is not None:
            default = member.get("default", False)
        elif "default" in field:
            default = field["default"]
        elif field.get("optional", False):
            default = ""
        elif field["type"] in ("asset", "asset_list", "entity"):
            default = ""
        else:
            continue
        obj[key] = _id_property_value(field, default)
        ui_getter = getattr(obj, "id_properties_ui", None)
        if not callable(ui_getter):
            continue
        try:
            ui = ui_getter(key)
            options: dict[str, object] = {
                "description": display_name(str(descriptor["name"])),
            }
            if "min" in descriptor:
                options["min"] = descriptor["min"]
            if "max" in descriptor:
                options["max"] = descriptor["max"]
            ui.update(**options)
        except (KeyError, TypeError, RuntimeError):
            # Blender releases differ slightly in the ID-property metadata
            # accepted here. The property itself remains fully usable.
            continue
    if schema["classname"] == "trigger_volume":
        obj["ce_physics_motion"] = (
            "Kinematic" if bool(obj.get("ce_kinematic", False)) else "Static")


def enum_items(field: dict[str, object]) -> tuple[tuple[str, str, str], ...]:
    """TODO: Describe `enum_items`.

    Args:
        field: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    return tuple(
        (str(name), display_name(str(name)), f"{display_name(str(name))} mode")
        for name in field.get("values", {})
    )


def field_by_property(
    schema: dict[str, object], key: str
) -> tuple[dict[str, object], dict[str, object] | None] | None:
    """TODO: Describe `field_by_property`.

    Args:
        schema: TODO: Describe this parameter.
        key: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    return next(
        (
            (field, member)
            for field, member in iter_property_fields(schema)
            if property_name(member or field) == key
        ),
        None,
    )


def validate_entities(
    objects: Iterable[object],
    game_schema: GameSchema,
) -> tuple[EntityIssue, ...]:
    """TODO: Describe `validate_entities`.

    Args:
        objects: TODO: Describe this parameter.
        game_schema: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    objects = tuple(objects)
    issues: list[EntityIssue] = []
    enabled_skyboxes = 0
    enabled_fog = 0
    enabled_post_process = 0
    enabled_audio_environment = 0
    physics_names = {
        str(getattr(obj, "name", ""))
        for obj in objects
        if entity_classname(obj) in {"prop", "collider"} and
        str(_get_property(
            obj, "ce_physics_motion",
            "None" if entity_classname(obj) == "prop" else "Static"
        )).lower() != "none"
    }
    objects_by_name = {
        str(getattr(obj, "name", "")): obj for obj in objects
    }
    for obj in objects:
        name = str(getattr(obj, "name", "<unnamed>"))
        classname = entity_classname(obj)
        if not classname:
            continue
        schema = game_schema.entity(classname)
        if schema is None:
            issues.append(EntityIssue(name, f"unknown entity type {classname!r}"))
            continue
        expected = native_object_type(classname)
        actual = str(getattr(obj, "type", ""))
        if expected in {"MESH", "LIGHT", "CAMERA", "SPEAKER"} and actual != expected:
            issues.append(
                EntityIssue(
                    name,
                    f"{classname} should use a Blender {expected.lower()} object, not {actual.lower()}",
                )
            )
        for field, member in iter_property_fields(schema):
            if member is not None or field["type"] not in ("asset", "asset_list"):
                continue
            if classname in {
                    "prop", "collider", "trigger_volume", "audio_source",
                    "skybox"}:
                continue
            getter = getattr(obj, "get", None)
            value = getter(property_name(field), "") if callable(getter) else ""
            if not value and not field.get("optional", False):
                issues.append(
                    EntityIssue(name, f"{display_name(str(field['name']))} is required")
                )
        if classname == "skybox":
            getter = getattr(obj, "get", None)
            panorama = getter("ce_panorama", "") if callable(getter) else ""
            if not panorama:
                issues.append(EntityIssue(name, "Panorama is required"))
        if classname == "physics_constraint":
            getter = getattr(obj, "get", None)
            first = str(getter(
                "ce_first_entity", "") if callable(getter) else "")
            second = str(getter(
                "ce_second_entity", "") if callable(getter) else "")
            if not first or not second:
                issues.append(EntityIssue(
                    name, "First Entity and Second Entity are required"))
            elif first == second:
                issues.append(EntityIssue(
                    name, "Constraint entities must be different"))
            else:
                for label, reference in (
                    ("First Entity", first), ("Second Entity", second)
                ):
                    if reference not in physics_names:
                        issues.append(EntityIssue(
                            name,
                            f"{label} must name a prop or collider with physics enabled"))
            constraint_type = str(
                getter("ce_type", "Fixed")
                if callable(getter) else "Fixed").lower()
            motor = str(
                getter("ce_motor", "Off")
                if callable(getter) else "Off").lower()
            if motor != "off" and constraint_type not in {"hinge", "slider"}:
                issues.append(EntityIssue(
                    name, "Only hinge and slider constraints support motors"))
            if motor != "off" and float(
                    getter("ce_motor_force_limit", 0.0)
                    if callable(getter) else 0.0) <= 0.0:
                issues.append(EntityIssue(
                    name, "Motor Force Limit must be positive"))
        if classname in {"prop", "collider", "trigger_volume"}:
            motion = str(
                ("Kinematic" if bool(getter("ce_kinematic", False))
                 else "Static")
                if classname == "trigger_volume" and callable(getter)
                else getter(
                    "ce_physics_motion",
                    "None" if classname == "prop" else "Static")
                if callable(getter)
                else "None"
            ).lower()
            collider = str(
                getter(COLLIDER_PROPERTY, "Box")
                if callable(getter) else "Box"
            ).lower()
            allowed_motions = (
                {"none", "static", "dynamic", "kinematic"}
                if classname == "prop"
                else {"static", "dynamic", "kinematic"})
            if motion not in allowed_motions:
                issues.append(EntityIssue(name, "Physics Motion is invalid"))
            if collider not in {
                "box", "sphere", "capsule", "cylinder",
                "convex_hull", "triangle_mesh", "height_field",
                "compound", "plane",
            }:
                issues.append(EntityIssue(name, "Collider is invalid"))
            if motion in {"dynamic", "kinematic"} and \
                    collider in {"triangle_mesh", "height_field", "plane"}:
                issues.append(EntityIssue(
                    name,
                    "movable props require convex or compound collision, "
                    "not a triangle mesh, height field, or plane",
                ))
            collision_name = str(
                getter("ce_collision_object", "")
                if callable(getter) else "")
            if collision_name:
                collision_source = objects_by_name.get(collision_name)
                if collision_source is None:
                    issues.append(EntityIssue(
                        name, "Collision Object does not exist"))
                elif getattr(collision_source, "type", "") != "MESH":
                    issues.append(EntityIssue(
                        name, "Collision Object must be a mesh"))
                elif getattr(collision_source, "parent", None) is not obj:
                    issues.append(EntityIssue(
                    name, "Collision Object must be parented to this entity"))
        getter = getattr(obj, "get", None)
        enabled = bool(getter("ce_enabled", True) if callable(getter) else True)
        if enabled and classname == "skybox":
            enabled_skyboxes += 1
        elif enabled and classname == "fog":
            enabled_fog += 1
        elif enabled and classname == "post_process":
            enabled_post_process += 1
        elif enabled and classname == "audio_environment":
            enabled_audio_environment += 1
        connections = getattr(obj, "cengine_connections", None)
        if connections is None:
            connections = _get_property(obj, "ce_connections", ()) or ()
        source_outputs = {
            str(endpoint["name"]) for endpoint in schema.get("outputs", ())
        } | {"OnEnabled", "OnDisabled"}
        for connection in connections:
            connection_get = (
                connection.get
                if callable(getattr(connection, "get", None))
                else lambda key, default=None: getattr(
                    connection, key, default)
            )
            event = str(connection_get("event", "") or "")
            action = str(connection_get("action", "") or "")
            target = connection_get("target", None)
            target_name = str(getattr(target, "name", target or ""))
            target_object = objects_by_name.get(target_name)
            target_classname = (
                entity_classname(target_object)
                if target_object is not None else "")
            target_schema = (
                game_schema.entity(target_classname)
                if target_classname else None)
            if event not in source_outputs:
                issues.append(EntityIssue(
                    name, f"connection output {event!r} is not supported"))
            if target_schema is None:
                issues.append(EntityIssue(
                    name,
                    f"connection target {target_name!r} is not an entity"))
                continue
            target_inputs = {
                str(endpoint["name"])
                for endpoint in target_schema.get("inputs", ())
            } | {"Enable", "Disable", "Toggle"}
            if action not in target_inputs:
                issues.append(EntityIssue(
                    name,
                    f"{target_name} does not support input {action!r}"))
    if enabled_skyboxes > 1:
        issues.append(EntityIssue("Scene", "only one skybox may be enabled"))
    if enabled_fog > 1:
        issues.append(EntityIssue("Scene", "only one height-fog entity may be enabled"))
    if enabled_post_process > 1:
        issues.append(EntityIssue("Scene", "only one post-process entity may be enabled"))
    if enabled_audio_environment > 1:
        issues.append(EntityIssue(
            "Scene", "only one audio-environment entity may be enabled"))
    return tuple(issues)


def _get_property(obj: object, key: str, default: object) -> object:
    """TODO: Describe `_get_property`.

    Args:
        obj: TODO: Describe this parameter.
        key: TODO: Describe this parameter.
        default: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    getter = getattr(obj, "get", None)
    return getter(key, default) if callable(getter) else default


def store_lightmap_bindings(
    objects: Iterable[object],
    placements: dict[str, LightmapPlacement],
    store_path: Callable[[Path], str],
) -> None:
    """TODO: Describe `store_lightmap_bindings`.

    Args:
        objects: TODO: Describe this parameter.
        placements: TODO: Describe this parameter.
        store_path: TODO: Describe this parameter.
    """
    by_name = {
        str(getattr(obj, "name", "")): obj
        for obj in objects
        if getattr(obj, "type", "") == "MESH"
    }
    for name, placement in placements.items():
        obj = by_name.get(name)
        if obj is None:
            continue
        obj[LIGHTMAP_PATH_PROPERTY] = store_path(placement.texture)
        obj[LIGHTMAP_SCALE_PROPERTY] = list(placement.scale)
        obj[LIGHTMAP_OFFSET_PROPERTY] = list(placement.offset)
        obj[LIGHTMAP_RANGE_PROPERTY] = placement.rgbm_range


def clear_lightmap_bindings(objects: Iterable[object]) -> int:
    """TODO: Describe `clear_lightmap_bindings`.

    Args:
        objects: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    cleared = 0
    for obj in objects:
        changed = False
        for key in (
            LIGHTMAP_PATH_PROPERTY,
            LIGHTMAP_SCALE_PROPERTY,
            LIGHTMAP_OFFSET_PROPERTY,
            LIGHTMAP_RANGE_PROPERTY,
        ):
            try:
                if key in obj:
                    del obj[key]
                    changed = True
            except (KeyError, TypeError):
                continue
        cleared += int(changed)
    return cleared


def load_lightmap_bindings(
    objects: Iterable[object],
    resolve_path: Callable[[str], Path],
) -> tuple[dict[str, LightmapPlacement], list[Path]]:
    """TODO: Describe `load_lightmap_bindings`.

    Args:
        objects: TODO: Describe this parameter.
        resolve_path: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    object_list = tuple(objects)
    placements: dict[str, LightmapPlacement] = {}
    outputs: set[Path] = set()
    for obj in object_list:
        if getattr(obj, "type", "") != "MESH":
            continue
        stored_path = str(_get_property(obj, LIGHTMAP_PATH_PROPERTY, "") or "")
        if not stored_path:
            continue
        name = str(getattr(obj, "name", ""))
        path = resolve_path(stored_path)
        if not path.is_file():
            raise RuntimeError(
                f"lightmap for {name} no longer exists: {path}; rebake or clear its binding"
            )
        scale = tuple(
            float(value)
            for value in _get_property(obj, LIGHTMAP_SCALE_PROPERTY, (1.0, 1.0))
        )
        offset = tuple(
            float(value)
            for value in _get_property(obj, LIGHTMAP_OFFSET_PROPERTY, (0.0, 0.0))
        )
        rgbm_range = float(_get_property(obj, LIGHTMAP_RANGE_PROPERTY, 8.0))
        if len(scale) != 2 or len(offset) != 2 or rgbm_range <= 0.0:
            raise RuntimeError(f"lightmap binding for {name} is invalid; rebake it")
        uv_layers = getattr(getattr(obj, "data", None), "uv_layers", None)
        if uv_layers is not None and getattr(uv_layers, "get", lambda *_: None)(
            LIGHTMAP_UV_NAME
        ) is None:
            raise RuntimeError(
                f"lightmap binding for {name} has no {LIGHTMAP_UV_NAME} UV map; rebake it"
            )
        placements[name] = LightmapPlacement(
            path,
            (scale[0], scale[1]),
            (offset[0], offset[1]),
            rgbm_range,
        )
        outputs.add(path)
    needs_lightmaps = any(
        getattr(obj, "type", "") == "LIGHT"
        and str(_get_property(obj, "ce_light_mode", "realtime")).lower()
        in {"baked", "mixed"}
        for obj in object_list
    )
    if needs_lightmaps:
        missing = [
            str(getattr(obj, "name", ""))
            for obj in object_list
            if getattr(obj, "type", "") == "MESH"
            and str(_get_property(
                obj, "ce_physics_motion", "None")).lower()
                not in {"dynamic", "kinematic"}
            and str(_get_property(obj, "ce_role", "")).lower() != "occluder"
            and str(getattr(obj, "name", "")) not in placements
        ]
        if missing:
            preview = ", ".join(missing[:4])
            suffix = f" and {len(missing) - 4} more" if len(missing) > 4 else ""
            raise RuntimeError(
                "scene uses baked or mixed lights, but these static meshes "
                f"have no baked binding: {preview}{suffix}; bake lightmaps or "
                "change the lights to realtime"
            )
    return placements, sorted(outputs)
