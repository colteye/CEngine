from __future__ import annotations

import math
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Iterable

from .collection_export import blender_to_engine_matrix_rows, object_role
from .formats import AssetType
from .ids import guid_from_stable_name
from .scene_export import (
    AssetReference, CameraEntity, EmptyEntity, EntityDescription, ExponentialHeightFogEntity,
    LightEntity, PlayerStart, PrefabEntity, PrefabLightmap, Prop, SceneDescription,
    SceneSettings, SkyboxEntity, Transform, TriggerEntity,
)


@dataclass(frozen=True)
class LightmapPlacement:
    texture: Path
    scale: tuple[float, float]
    offset: tuple[float, float]
    rgbm_range: float = 8.0


def blender_scene_settings(blender: object) -> SceneSettings:
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
    mesh_outputs: dict[str, list[tuple[Path, str]]],
    material_outputs: dict[str, Path],
    prefab_outputs: dict[str, Path],
    asset_path: Callable[[Path], str],
    lightmaps: dict[str, LightmapPlacement] | None = None,
    prefab_lightmaps: dict[str, tuple[tuple[int, LightmapPlacement], ...]] | None = None,
    skybox_outputs: dict[str, Path] | None = None,
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
            if classname not in ("", "prop"):
                raise ValueError(f"unsupported mesh entity classname: {classname}")
            dynamic = bool(_property(obj, "ce_dynamic", False))
            placement = lightmaps.get(name) if lightmaps is not None else None
            if placement is not None and dynamic:
                raise ValueError(f"only a static prop may have a baked lightmap: {name}")
            for output, material_name in outputs:
                material_output = material_outputs.get(material_name)
                if material_output is None:
                    raise ValueError(f"mesh entity has no generated .cmat for {material_name}: {name}")
                data = Prop(
                    mesh=_reference(AssetType.MESH, output, asset_path),
                    transform=transform,
                    materials=(_reference(AssetType.MATERIAL, material_output, asset_path),),
                    lightmap=_reference(AssetType.TEXTURE, placement.texture, asset_path) if placement else None,
                    lightmap_scale=placement.scale if placement else (1.0, 1.0),
                    lightmap_offset=placement.offset if placement else (0.0, 0.0),
                    lightmap_rgbm_range=placement.rgbm_range if placement else 8.0,
                    dynamic=dynamic,
                    shadow_only=object_role(obj) == "occluder",
                    collision_enabled=bool(_property(obj, "ce_collision",
                        _property(obj, "ce_physics", False))),
                    collision_half_extents=tuple(float(value) for value in _property(
                        obj, "ce_collision_half_extents", (0.5, 0.5, 0.5))),
                    mass=float(_property(obj, "ce_mass", 1.0)),
                )
                entity_name = name if len(outputs) == 1 else f"{name}:{material_name}"
                entities.append(EntityDescription(data, entity_name))
            continue
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
                (float(getattr(light, "size", 1.0)), float(getattr(light, "size_y", 1.0))),
                enabled=True, casts_shadows=bool(getattr(light, "use_shadow", True)))
        elif obj_type == "EMPTY" and getattr(obj, "instance_collection", None) is not None:
            collection = obj.instance_collection
            output = prefab_outputs.get(str(getattr(collection, "name", "")))
            if output is None:
                raise ValueError(f"collection instance has no generated .casset: {name}")
            placements = prefab_lightmaps.get(name, ()) if prefab_lightmaps is not None else ()
            data = PrefabEntity(
                _reference(AssetType.ASSET, output, asset_path), transform,
                tuple(PrefabLightmap(
                    object_index,
                    _reference(AssetType.TEXTURE, placement.texture, asset_path),
                    placement.scale, placement.offset, placement.rgbm_range)
                    for object_index, placement in placements),
            )
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
            elif classname == "skybox":
                output = (skybox_outputs or {}).get(name)
                if output is None:
                    raise ValueError(f"skybox has no generated HDR panorama: {name}")
                data = SkyboxEntity(
                    _reference(AssetType.TEXTURE, output, asset_path), transform,
                    float(_property(obj, "ce_intensity", 1.0)),
                    float(_property(obj, "ce_rotation_radians", 0.0)),
                    bool(_property(obj, "ce_enabled", True)))
            elif classname == "exponential_height_fog":
                color = tuple(float(value) for value in _property(
                    obj, "ce_inscattering_color", (0.5, 0.6, 0.7)))
                if len(color) != 3:
                    raise ValueError(f"fog inscattering color must have three values: {name}")
                data = ExponentialHeightFogEntity(
                    transform, color,
                    float(_property(obj, "ce_fog_density", 0.02)),
                    float(_property(obj, "ce_height_falloff", 0.2)),
                    float(_property(obj, "ce_start_distance", 0.0)),
                    float(_property(obj, "ce_max_opacity", 1.0)),
                    float(_property(obj, "ce_cutoff_distance", 0.0)),
                    bool(_property(obj, "ce_enabled", True)))
            else:
                raise ValueError(f"unsupported empty entity classname: {classname}")
        else:
            continue
        entities.append(EntityDescription(data, name))
    if not entities:
        raise ValueError("scene contains no exportable entities")
    return SceneDescription(tuple(entities))
