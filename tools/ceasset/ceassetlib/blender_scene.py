from __future__ import annotations

import math
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Iterable

from .collection_export import blender_to_engine_matrix_rows
from .formats import AssetType
from .ids import guid_from_stable_name
from .scene_export import (
    AssetReference, CameraEntity, DynamicProp, EmptyEntity, EntityDescription,
    LightEntity, PlayerStart, PrefabEntity, SceneDescription, StaticProp,
    Transform, TriggerEntity,
)


@dataclass(frozen=True)
class LightmapPlacement:
    texture: Path
    scale: tuple[float, float]
    offset: tuple[float, float]
    rgbm_range: float = 8.0


def _property(obj: object, name: str, default: object = None) -> object:
    getter = getattr(obj, "get", None)
    return getter(name, default) if callable(getter) else default


def _quaternion(matrix: list[list[float]]) -> tuple[float, float, float, float]:
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


def _reference(asset_type: AssetType, path: Path, asset_path: Callable[[Path], str]) -> AssetReference:
    relative = asset_path(path)
    return AssetReference(asset_type, relative, guid_from_stable_name(relative))


def scene_description(
    objects: Iterable[object],
    mesh_outputs: dict[str, list[Path]],
    material_outputs: dict[str, Path],
    prefab_outputs: dict[str, Path],
    asset_path: Callable[[Path], str],
    material_names: Callable[[object], list[str]] | None = None,
    lightmaps: dict[str, LightmapPlacement] | None = None,
) -> SceneDescription:
    entities: list[EntityDescription] = []
    for obj in sorted(objects, key=lambda value: str(getattr(value, "name", ""))):
        name = str(getattr(obj, "name", ""))
        obj_type = str(getattr(obj, "type", ""))
        transform = object_transform(obj)
        classname = str(_property(obj, "ce_classname", "") or "")

        if obj_type == "MESH":
            outputs = mesh_outputs.get(name, ())
            if not outputs:
                raise ValueError(f"mesh entity has no generated .cmesh: {name}")
            dynamic = classname == "prop_dynamic" or bool(_property(obj, "ce_dynamic", False))
            if classname not in ("", "prop_static", "prop_dynamic"):
                raise ValueError(f"unsupported mesh entity classname: {classname}")
            names = material_names(obj) if material_names is not None else [
                slot.material.name for slot in getattr(obj, "material_slots", ())
                if getattr(slot, "material", None) is not None]
            missing = [material for material in names if material not in material_outputs]
            if missing:
                raise ValueError(f"mesh entity has no generated .cmat for {missing[0]}: {name}")
            materials = tuple(_reference(
                AssetType.MATERIAL, material_outputs[material], asset_path) for material in names)
            mesh = _reference(AssetType.MESH, outputs[0], asset_path)
            if dynamic:
                data = DynamicProp(mesh, transform, materials,
                    physics_enabled=bool(_property(obj, "ce_physics", False)),
                    collision_half_extents=tuple(float(value) for value in _property(
                        obj, "ce_collision_half_extents", (0.5, 0.5, 0.5))),
                    mass=float(_property(obj, "ce_mass", 1.0)))
            else:
                placement = lightmaps.get(name) if lightmaps is not None else None
                data = StaticProp(
                    mesh,
                    transform,
                    materials,
                    _reference(AssetType.TEXTURE, placement.texture, asset_path) if placement else None,
                    placement.scale if placement else (1.0, 1.0),
                    placement.offset if placement else (0.0, 0.0),
                    placement.rgbm_range if placement else 8.0,
                )
        elif obj_type == "CAMERA":
            camera = getattr(obj, "data", None)
            projection = 1 if str(getattr(camera, "type", "PERSP")) == "ORTHO" else 0
            data = CameraEntity(transform, projection,
                float(getattr(camera, "angle_y", getattr(camera, "angle", 1.0471976))),
                float(getattr(camera, "ortho_scale", 10.0)),
                float(getattr(camera, "clip_start", 0.1)),
                float(getattr(camera, "clip_end", 1000.0)))
        elif obj_type == "LIGHT":
            light = getattr(obj, "data", None)
            light_type = {"POINT": 0, "SUN": 1, "SPOT": 2, "AREA": 3}.get(
                str(getattr(light, "type", "POINT")), 0)
            mode = {"realtime": 0, "baked": 1, "mixed": 2}.get(
                str(_property(obj, "ce_light_mode", "realtime")).lower(), 0)
            color = tuple(float(value) for value in getattr(light, "color", (1.0, 1.0, 1.0)))
            spot_size = float(getattr(light, "spot_size", 0.7853982))
            spot_blend = float(getattr(light, "spot_blend", 0.0))
            data = LightEntity(transform, light_type, mode, color,
                float(getattr(light, "energy", 1.0)),
                float(getattr(light, "cutoff_distance", 10.0)),
                spot_size * max(0.0, 1.0 - spot_blend), spot_size,
                (float(getattr(light, "size", 1.0)), float(getattr(light, "size_y", 1.0))))
        elif obj_type == "EMPTY" and getattr(obj, "instance_collection", None) is not None:
            collection = obj.instance_collection
            output = prefab_outputs.get(str(getattr(collection, "name", "")))
            if output is None:
                raise ValueError(f"collection instance has no generated .casset: {name}")
            data = PrefabEntity(_reference(AssetType.ASSET, output, asset_path), transform)
        elif obj_type == "EMPTY":
            if classname in ("", "empty"):
                data = EmptyEntity(transform)
            elif classname == "trigger":
                half_extents = tuple(float(value) for value in _property(
                    obj, "ce_half_extents", (0.5, 0.5, 0.5)))
                if len(half_extents) != 3:
                    raise ValueError(f"trigger half extents must have three values: {name}")
                data = TriggerEntity(transform, half_extents,
                    trigger_once=bool(_property(obj, "ce_trigger_once", False)))
            elif classname == "info_player_start":
                data = PlayerStart(transform, int(_property(obj, "ce_team", 0)))
            else:
                raise ValueError(f"unsupported empty entity classname: {classname}")
        else:
            continue
        entities.append(EntityDescription(data, name))
    if not entities:
        raise ValueError("scene contains no exportable entities")
    return SceneDescription(tuple(entities))
