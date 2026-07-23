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


@dataclass(frozen=True)
class EntityIssue:
    object_name: str
    message: str

    def __str__(self) -> str:
        return f"{self.object_name}: {self.message}"


def display_name(identifier: str) -> str:
    return identifier.replace("_", " ").strip().title()


def property_name(field: dict[str, object]) -> str:
    return str(field.get("blender_property", f"ce_{field['name']}"))


def entity_classname(obj: object) -> str:
    getter = getattr(obj, "get", None)
    explicit = str(getter(CLASSNAME_PROPERTY, "") if callable(getter) else "")
    if explicit:
        return explicit
    return {
        "MESH": "prop",
        "LIGHT": "light",
    }.get(str(getattr(obj, "type", "")), "")


def native_object_type(classname: str) -> str:
    return {
        "prop": "MESH",
        "light": "LIGHT",
        "player": "CAMERA",
    }.get(classname, "EMPTY")


def entity_schema(game_schema: GameSchema, obj: object) -> dict[str, object] | None:
    classname = entity_classname(obj)
    return game_schema.entity(classname) if classname else None


def iter_property_fields(
    schema: dict[str, object],
) -> Iterator[tuple[dict[str, object], dict[str, object] | None]]:
    for field in schema["fields"]:
        if field["type"] == "transform":
            continue
        if field["type"] == "flags":
            for member in field["members"]:
                yield field, member
            continue
        yield field, None


def _id_property_value(field: dict[str, object], default: object) -> object:
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
    if overwrite_classname:
        obj[CLASSNAME_PROPERTY] = str(schema["classname"])
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
        elif field["type"] in ("asset", "asset_list"):
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


def enum_items(field: dict[str, object]) -> tuple[tuple[str, str, str], ...]:
    return tuple(
        (str(name), display_name(str(name)), f"{display_name(str(name))} mode")
        for name in field.get("values", {})
    )


def field_by_property(
    schema: dict[str, object], key: str
) -> tuple[dict[str, object], dict[str, object] | None] | None:
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
    issues: list[EntityIssue] = []
    enabled_skyboxes = 0
    enabled_fog = 0
    enabled_post_process = 0
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
        if expected in {"MESH", "LIGHT", "CAMERA"} and actual != expected:
            issues.append(
                EntityIssue(
                    name,
                    f"{classname} should use a Blender {expected.lower()} object, not {actual.lower()}",
                )
            )
        for field, member in iter_property_fields(schema):
            if member is not None or field["type"] not in ("asset", "asset_list"):
                continue
            if classname in {"prop", "skybox"}:
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
        getter = getattr(obj, "get", None)
        enabled = bool(getter("ce_enabled", True) if callable(getter) else True)
        if enabled and classname == "skybox":
            enabled_skyboxes += 1
        elif enabled and classname == "exponential_height_fog":
            enabled_fog += 1
        elif enabled and classname == "post_process":
            enabled_post_process += 1
    if enabled_skyboxes > 1:
        issues.append(EntityIssue("Scene", "only one skybox may be enabled"))
    if enabled_fog > 1:
        issues.append(EntityIssue("Scene", "only one height-fog entity may be enabled"))
    if enabled_post_process > 1:
        issues.append(EntityIssue("Scene", "only one post-process entity may be enabled"))
    return tuple(issues)


def _get_property(obj: object, key: str, default: object) -> object:
    getter = getattr(obj, "get", None)
    return getter(key, default) if callable(getter) else default


def store_lightmap_bindings(
    objects: Iterable[object],
    placements: dict[str, LightmapPlacement],
    store_path: Callable[[Path], str],
) -> None:
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
            and not bool(_get_property(obj, "ce_dynamic", False))
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
