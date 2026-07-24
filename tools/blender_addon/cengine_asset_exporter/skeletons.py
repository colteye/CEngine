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

import time
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Iterable

from ceassetlib.assetfile import AssetWriteDesc, write_binary_asset
from ceassetlib.collection_export import clean_asset_name
from ceassetlib.formats import AssetType
from ceassetlib.game_schema import load_bundled_game
from ceassetlib.ids import guid_from_stable_name, hash_file
from ceassetlib.paths import generic_path, output_dir_for_source
from ceassetlib.wire import pack_record


GAME_SCHEMA = load_bundled_game()
IDENTITY_MATRIX = (
    (1.0, 0.0, 0.0, 0.0),
    (0.0, 1.0, 0.0, 0.0),
    (0.0, 0.0, 1.0, 0.0),
    (0.0, 0.0, 0.0, 1.0),
)
BLENDER_TO_ENGINE = (
    (0.0, 1.0, 0.0, 0.0),
    (-1.0, 0.0, 0.0, 0.0),
    (0.0, 0.0, 1.0, 0.0),
    (0.0, 0.0, 0.0, 1.0),
)
ENGINE_TO_BLENDER = (
    (0.0, -1.0, 0.0, 0.0),
    (1.0, 0.0, 0.0, 0.0),
    (0.0, 0.0, 1.0, 0.0),
    (0.0, 0.0, 0.0, 1.0),
)


@dataclass(frozen=True)
class SkeletonExport:
    """TODO: Describe `SkeletonExport`."""

    source: object
    output: Path


def elapsed(start: float) -> str:
    """TODO: Describe `elapsed`.

    Args:
        start: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    return f"{time.perf_counter() - start:.2f}s"


def skeleton_output_path(blend_source: Path, output_root: Path, armature_name: str) -> Path:
    """TODO: Describe `skeleton_output_path`.

    Args:
        blend_source: TODO: Describe this parameter.
        output_root: TODO: Describe this parameter.
        armature_name: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    return output_dir_for_source(blend_source, output_root) / "skeletons" / f"{clean_asset_name(armature_name)}.cskel"


def armature_objects(objects: Iterable[object]) -> list[object]:
    """TODO: Describe `armature_objects`.

    Args:
        objects: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    armatures: list[object] = []
    seen: set[int] = set()
    for obj in objects:
        if getattr(obj, "type", "") != "ARMATURE":
            continue
        key = id(obj)
        if key in seen:
            continue
        seen.add(key)
        armatures.append(obj)
    return sorted(armatures, key=lambda obj: obj.name)


def matrix_rows(matrix: object | None) -> list[float]:
    """TODO: Describe `matrix_rows`.

    Args:
        matrix: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    rows = matrix_values(matrix)
    return [rows[row][column] for row in range(4) for column in range(4)]


def matrix_values(matrix: object | None) -> tuple[tuple[float, ...], ...]:
    if matrix is None:
        return IDENTITY_MATRIX
    return tuple(
        tuple(float(matrix[row][column]) for column in range(4))
        for row in range(4)
    )


def matrix_multiply(
    first: tuple[tuple[float, ...], ...],
    second: tuple[tuple[float, ...], ...],
) -> tuple[tuple[float, ...], ...]:
    return tuple(tuple(
        sum(first[row][item] * second[item][column] for item in range(4))
        for column in range(4)
    ) for row in range(4))


def matrix_inverse(
    matrix: tuple[tuple[float, ...], ...],
) -> tuple[tuple[float, ...], ...]:
    rows = [
        [*matrix[row], *(1.0 if row == column else 0.0 for column in range(4))]
        for row in range(4)
    ]
    for column in range(4):
        pivot = max(range(column, 4), key=lambda row: abs(rows[row][column]))
        if abs(rows[pivot][column]) <= 1.0e-12:
            raise RuntimeError("skeleton contains a singular joint matrix")
        rows[column], rows[pivot] = rows[pivot], rows[column]
        scale = rows[column][column]
        rows[column] = [value / scale for value in rows[column]]
        for row in range(4):
            if row == column:
                continue
            scale = rows[row][column]
            rows[row] = [
                rows[row][item] - scale * rows[column][item]
                for item in range(8)
            ]
    return tuple(tuple(rows[row][4:]) for row in range(4))


def engine_matrix(matrix: object | None) -> tuple[tuple[float, ...], ...]:
    return matrix_multiply(
        matrix_multiply(BLENDER_TO_ENGINE, matrix_values(matrix)),
        ENGINE_TO_BLENDER,
    )


def transform_record(
    matrix: tuple[tuple[float, ...], ...],
) -> dict[str, tuple[float, ...]]:
    translation = (matrix[0][3], matrix[1][3], matrix[2][3])
    columns = tuple(
        (matrix[0][column], matrix[1][column], matrix[2][column])
        for column in range(3)
    )
    scales = tuple(sum(value * value for value in column) ** 0.5
                   for column in columns)
    if min(scales) <= 1.0e-8:
        raise RuntimeError("skeleton contains a zero-scale joint")
    if max(scales) - min(scales) > 1.0e-4:
        raise RuntimeError("CEngine animation currently requires uniform joint scale")
    rotation = tuple(tuple(
        matrix[row][column] / scales[column] for column in range(3)
    ) for row in range(3))
    trace = rotation[0][0] + rotation[1][1] + rotation[2][2]
    if trace > 0.0:
        root = (trace + 1.0) ** 0.5 * 2.0
        quaternion = (
            (rotation[2][1] - rotation[1][2]) / root,
            (rotation[0][2] - rotation[2][0]) / root,
            (rotation[1][0] - rotation[0][1]) / root,
            0.25 * root,
        )
    elif rotation[0][0] > rotation[1][1] and rotation[0][0] > rotation[2][2]:
        root = (1.0 + rotation[0][0] - rotation[1][1] - rotation[2][2]) ** 0.5 * 2.0
        quaternion = (
            0.25 * root,
            (rotation[0][1] + rotation[1][0]) / root,
            (rotation[0][2] + rotation[2][0]) / root,
            (rotation[2][1] - rotation[1][2]) / root,
        )
    elif rotation[1][1] > rotation[2][2]:
        root = (1.0 + rotation[1][1] - rotation[0][0] - rotation[2][2]) ** 0.5 * 2.0
        quaternion = (
            (rotation[0][1] + rotation[1][0]) / root,
            0.25 * root,
            (rotation[1][2] + rotation[2][1]) / root,
            (rotation[0][2] - rotation[2][0]) / root,
        )
    else:
        root = (1.0 + rotation[2][2] - rotation[0][0] - rotation[1][1]) ** 0.5 * 2.0
        quaternion = (
            (rotation[0][2] + rotation[2][0]) / root,
            (rotation[1][2] + rotation[2][1]) / root,
            0.25 * root,
            (rotation[1][0] - rotation[0][1]) / root,
        )
    return {
        "translation": translation,
        "rotation": quaternion,
        "scale": scales,
    }


def canonical_bones(armature: object) -> list[object]:
    bones = list(getattr(getattr(armature, "data", None), "bones", ()))
    if not bones:
        raise RuntimeError(f"armature has no bones: {getattr(armature, 'name', '')}")
    if len(bones) > 1024:
        raise RuntimeError("CEngine animation supports at most 1024 joints")
    names = [str(bone.name) for bone in bones]
    if len(names) != len(set(names)):
        raise RuntimeError("armature bone names must be unique")
    roots = [bone for bone in bones if getattr(bone, "parent", None) is None]
    if len(roots) != 1:
        raise RuntimeError("CEngine animation requires exactly one root joint")
    children: dict[int, list[object]] = {id(bone): [] for bone in bones}
    for bone in bones:
        parent = getattr(bone, "parent", None)
        if parent is not None:
            if id(parent) not in children:
                raise RuntimeError("armature bone parent is outside the armature")
            children[id(parent)].append(bone)
    ordered: list[object] = []

    def visit(bone: object) -> None:
        ordered.append(bone)
        for child in sorted(children[id(bone)], key=lambda value: value.name):
            visit(child)

    visit(roots[0])
    if len(ordered) != len(bones):
        raise RuntimeError("armature hierarchy is disconnected")
    return ordered


def skeleton_payload(
    source: Path,
    armature: object,
) -> bytes:
    """TODO: Describe `skeleton_payload`.

    Args:
        source: TODO: Describe this parameter.
        armature: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    bones = canonical_bones(armature)
    bone_indices = {bone.name: index for index, bone in enumerate(bones)}
    del source
    absolute = [engine_matrix(getattr(bone, "matrix_local", None)) for bone in bones]
    metadata = []
    for index, bone in enumerate(bones):
        parent = bone_indices.get(
            getattr(getattr(bone, "parent", None), "name", ""), -1
        )
        local = absolute[index] if parent < 0 else matrix_multiply(
            matrix_inverse(absolute[parent]), absolute[index]
        )
        metadata.append({
            "name": bone.name,
            "parent": parent,
            "rest": transform_record(local),
            "joint_from_armature_bind": matrix_rows(matrix_inverse(absolute[index])),
        })
    return pack_record(GAME_SCHEMA, "skeleton", {
        "name": armature.name,
        "bones": metadata,
    })


def write_skeleton_asset(
    blend_source: Path,
    output_root: Path,
    armature: object,
    asset_path: Callable[[Path], str] = generic_path,
    logger: Callable[[str], None] | None = None,
    source_hash: int | None = None,
) -> SkeletonExport:
    """TODO: Describe `write_skeleton_asset`.

    Args:
        blend_source: TODO: Describe this parameter.
        output_root: TODO: Describe this parameter.
        armature: TODO: Describe this parameter.
        asset_path: TODO: Describe this parameter.
        logger: TODO: Describe this parameter.
        source_hash: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    start = time.perf_counter()
    output = skeleton_output_path(blend_source, output_root, armature.name)
    bone_count = len(getattr(getattr(armature, "data", None), "bones", ()))
    if logger is not None:
        logger(f"Skeleton {armature.name}: {bone_count} bone(s) -> {output}")
    desc = AssetWriteDesc(
        asset_type=AssetType.SKELETON,
        guid=guid_from_stable_name(asset_path(output)),
        source_hash=source_hash if source_hash is not None else hash_file(blend_source),
        platform_target="generic",
        payload=skeleton_payload(blend_source, armature),
    )
    write_binary_asset(output, desc)
    if logger is not None:
        logger(f"Skeleton {armature.name}: wrote {output.name} in {elapsed(start)}")
    return SkeletonExport(armature, output)


def write_skeleton_assets(
    blend_source: Path,
    output_root: Path,
    objects: Iterable[object],
    asset_path: Callable[[Path], str] = generic_path,
    logger: Callable[[str], None] | None = None,
    source_hash: int | None = None,
) -> list[SkeletonExport]:
    """TODO: Describe `write_skeleton_assets`.

    Args:
        blend_source: TODO: Describe this parameter.
        output_root: TODO: Describe this parameter.
        objects: TODO: Describe this parameter.
        asset_path: TODO: Describe this parameter.
        logger: TODO: Describe this parameter.
        source_hash: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    armatures = armature_objects(objects)
    if logger is not None:
        logger(f"Skeleton export queue: {len(armatures)} armature object(s)")
    outputs: list[SkeletonExport] = []
    for index, armature in enumerate(armatures, start=1):
        if logger is not None:
            logger(f"Skeleton {index}/{len(armatures)}: {armature.name}")
        outputs.append(write_skeleton_asset(
            blend_source, output_root, armature, asset_path, logger, source_hash))
    return outputs
