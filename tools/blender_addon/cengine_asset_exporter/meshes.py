from __future__ import annotations

import struct
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Iterable

from ceassetlib.assetfile import (
    AssetWriteDesc,
    write_binary_asset,
)
from ceassetlib.collection_export import clean_asset_name
from ceassetlib.formats import AssetType
from ceassetlib.ids import guid_from_stable_name, hash_file
from ceassetlib.paths import generic_path, output_dir_for_source


VERTEX = struct.Struct("<ffffffff")
SKINNED_VERTEX = struct.Struct("<ffffffffHHHHHHHH")
INDEX = struct.Struct("<I")
MESH_METADATA = struct.Struct("<4sHHIIIIIffffffIIIII")
MESH_METADATA_MAGIC = b"CEMH"
MESH_METADATA_VERSION = 1
MESH_FLAG_SKINNED = 1 << 0
MAX_SKIN_INFLUENCES = 4


@dataclass(frozen=True)
class MeshExport:
    source: object
    output: Path


@dataclass(frozen=True)
class MeshBuffers:
    vertex_count: int
    index_count: int
    bounds_min: tuple[float, float, float]
    bounds_max: tuple[float, float, float]
    vertex_stride: int
    skinned: bool
    skeleton: str
    data: bytes


def mesh_output_path(blend_source: Path, output_root: Path, mesh_name: str) -> Path:
    return output_dir_for_source(blend_source, output_root) / "meshes" / f"{clean_asset_name(mesh_name)}.cmesh"


def mesh_objects(objects: Iterable[object]) -> list[object]:
    meshes: list[object] = []
    seen: set[int] = set()
    for obj in objects:
        if getattr(obj, "type", "") != "MESH":
            continue
        key = id(obj)
        if key in seen:
            continue
        seen.add(key)
        meshes.append(obj)
    return sorted(meshes, key=lambda obj: obj.name)


def tuple3(value: object) -> tuple[float, float, float]:
    return (float(value[0]), float(value[1]), float(value[2]))


def tuple2(value: object) -> tuple[float, float]:
    return (float(value[0]), float(value[1]))


def mesh_material_names(obj: object) -> list[str]:
    names: list[str] = []
    for slot in getattr(obj, "material_slots", ()):
        material = getattr(slot, "material", None)
        if material is not None:
            names.append(material.name)
    return names


def mesh_armature(obj: object) -> object | None:
    parent = getattr(obj, "parent", None)
    if getattr(parent, "type", "") == "ARMATURE":
        return parent
    for modifier in getattr(obj, "modifiers", ()):
        if getattr(modifier, "type", "") == "ARMATURE":
            armature = getattr(modifier, "object", None)
            if armature is not None:
                return armature
    return None


def bone_indices(armature: object | None) -> dict[str, int]:
    bones = getattr(getattr(armature, "data", None), "bones", ()) if armature is not None else ()
    indices = {bone.name: index for index, bone in enumerate(bones)}
    if any(index > 0xFFFF for index in indices.values()):
        raise RuntimeError("CEngine mesh export supports up to 65536 bones per skeleton")
    return indices


def vertex_group_name(obj: object, group_index: int) -> str:
    groups = getattr(obj, "vertex_groups", ())
    if group_index < 0 or group_index >= len(groups):
        return ""
    return groups[group_index].name


def skin_influences(obj: object | None, vertex: object, bone_lookup: dict[str, int]) -> tuple[list[int], list[int]]:
    if obj is None or not bone_lookup:
        return [0, 0, 0, 0], [0, 0, 0, 0]

    influences: list[tuple[int, float]] = []
    for group in getattr(vertex, "groups", ()):
        bone_name = vertex_group_name(obj, int(group.group))
        bone_index = bone_lookup.get(bone_name)
        if bone_index is None:
            continue
        weight = float(group.weight)
        if weight > 0.0:
            influences.append((bone_index, weight))

    influences.sort(key=lambda item: item[1], reverse=True)
    influences = influences[:MAX_SKIN_INFLUENCES]
    total_weight = sum(weight for _, weight in influences)
    if total_weight <= 0.0:
        return [0, 0, 0, 0], [0, 0, 0, 0]

    indices = [bone_index for bone_index, _ in influences]
    weights = [int(round((weight / total_weight) * 65535.0)) for _, weight in influences]
    if weights:
        weights[0] = max(0, min(65535, weights[0] + (65535 - sum(weights))))

    while len(indices) < MAX_SKIN_INFLUENCES:
        indices.append(0)
        weights.append(0)
    return indices, weights


def active_uv_data(mesh: object):
    uv_layers = getattr(mesh, "uv_layers", None)
    active = getattr(uv_layers, "active", None) if uv_layers is not None else None
    return getattr(active, "data", ()) if active is not None else ()


def polygon_loop_indices(polygon: object) -> list[int]:
    return [int(index) for index in getattr(polygon, "loop_indices", ())]


def polygon_triangles(polygon: object) -> list[tuple[int, int, int]]:
    loop_indices = polygon_loop_indices(polygon)
    if len(loop_indices) < 3:
        return []
    if len(loop_indices) == 3:
        return [(loop_indices[0], loop_indices[1], loop_indices[2])]
    return [
        (loop_indices[0], loop_indices[index], loop_indices[index + 1])
        for index in range(1, len(loop_indices) - 1)
    ]


def mesh_buffers(mesh: object, obj: object | None = None, armature: object | None = None) -> MeshBuffers:
    vertices = getattr(mesh, "vertices", ())
    loops = getattr(mesh, "loops", ())
    polygons = getattr(mesh, "polygons", ())
    uv_data = active_uv_data(mesh)

    packed_vertices = bytearray()
    packed_indices = bytearray()
    bounds_min = [float("inf"), float("inf"), float("inf")]
    bounds_max = [float("-inf"), float("-inf"), float("-inf")]
    vertex_count = 0
    bone_lookup = bone_indices(armature)
    skinned = armature is not None
    vertex_stride = SKINNED_VERTEX.size if skinned else VERTEX.size

    for polygon in polygons:
        for triangle in polygon_triangles(polygon):
            for loop_index in triangle:
                loop = loops[loop_index]
                vertex = vertices[int(loop.vertex_index)]
                position = tuple3(vertex.co)
                normal = tuple3(getattr(loop, "normal", (0.0, 0.0, 1.0)))
                uv = tuple2(uv_data[loop_index].uv) if loop_index < len(uv_data) else (0.0, 0.0)

                for axis in range(3):
                    bounds_min[axis] = min(bounds_min[axis], position[axis])
                    bounds_max[axis] = max(bounds_max[axis], position[axis])

                if skinned:
                    skin_indices, skin_weights = skin_influences(obj, vertex, bone_lookup)
                    packed_vertices.extend(SKINNED_VERTEX.pack(*position, *normal, *uv, *skin_indices, *skin_weights))
                else:
                    packed_vertices.extend(VERTEX.pack(*position, *normal, *uv))
                packed_indices.extend(INDEX.pack(vertex_count))
                vertex_count += 1

    if vertex_count == 0:
        bounds = (0.0, 0.0, 0.0)
        return MeshBuffers(0, 0, bounds, bounds, vertex_stride, skinned, getattr(armature, "name", ""), b"")

    return MeshBuffers(
        vertex_count,
        vertex_count,
        tuple(bounds_min),
        tuple(bounds_max),
        vertex_stride,
        skinned,
        getattr(armature, "name", ""),
        bytes(packed_vertices + packed_indices),
    )


def mesh_metadata_payload(
    buffers: MeshBuffers,
    material_slot_count: int,
    geometry_offset: int,
) -> bytes:
    flags = MESH_FLAG_SKINNED if buffers.skinned else 0
    return MESH_METADATA.pack(
        MESH_METADATA_MAGIC,
        MESH_METADATA_VERSION,
        MESH_METADATA.size,
        flags,
        buffers.vertex_count,
        buffers.index_count,
        buffers.vertex_stride,
        INDEX.size,
        *buffers.bounds_min,
        *buffers.bounds_max,
        material_slot_count,
        geometry_offset,
        0,
        0,
        0,
    )


def write_mesh_asset(
    blend_source: Path,
    output_root: Path,
    obj: object,
    material_output_path_for_name: Callable[[str], Path | None],
    skeleton_output_path_for_name: Callable[[str], Path | None],
    asset_path: Callable[[Path], str] = generic_path,
) -> MeshExport:
    material_names = mesh_material_names(obj)
    if len(material_names) != 1:
        raise RuntimeError(f"CEngine mesh export expects exactly one material slot: {obj.name}")

    armature = mesh_armature(obj)
    buffers = mesh_buffers(obj.data, obj, armature)
    output = mesh_output_path(blend_source, output_root, obj.name)
    source_hash = hash_file(blend_source)
    del material_output_path_for_name
    del skeleton_output_path_for_name
    desc = AssetWriteDesc(
        asset_type=AssetType.MESH,
        guid=guid_from_stable_name(asset_path(output)),
        source_hash=source_hash,
        platform_target="generic",
        payload=mesh_metadata_payload(buffers, len(material_names), MESH_METADATA.size) + buffers.data,
    )
    write_binary_asset(output, desc)
    return MeshExport(obj, output)


def write_mesh_assets(
    blend_source: Path,
    output_root: Path,
    objects: Iterable[object],
    material_output_path_for_name: Callable[[str], Path | None],
    skeleton_output_path_for_name: Callable[[str], Path | None],
    asset_path: Callable[[Path], str] = generic_path,
) -> list[MeshExport]:
    return [
        write_mesh_asset(
            blend_source,
            output_root,
            obj,
            material_output_path_for_name,
            skeleton_output_path_for_name,
            asset_path,
        )
        for obj in mesh_objects(objects)
    ]
