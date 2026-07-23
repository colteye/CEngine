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
    if matrix is None:
        return [
            1.0, 0.0, 0.0, 0.0,
            0.0, 1.0, 0.0, 0.0,
            0.0, 0.0, 1.0, 0.0,
            0.0, 0.0, 0.0, 1.0,
        ]
    return [float(matrix[row][column]) for row in range(4) for column in range(4)]


def skeleton_payload(source: Path, armature: object) -> bytes:
    """TODO: Describe `skeleton_payload`.

    Args:
        source: TODO: Describe this parameter.
        armature: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    bones = list(getattr(getattr(armature, "data", None), "bones", ()))
    bone_indices = {bone.name: index for index, bone in enumerate(bones)}
    del source

    return pack_record(GAME_SCHEMA, "skeleton", {
        "name": armature.name,
        "bones": [
            {
                "name": bone.name,
                "parent": bone_indices.get(
                    getattr(getattr(bone, "parent", None), "name", ""), -1
                ),
                "armature_from_bone": matrix_rows(
                    getattr(bone, "matrix_local", None)
                ),
            }
            for bone in bones
        ],
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
        outputs.append(write_skeleton_asset(blend_source, output_root, armature, asset_path, logger, source_hash))
    return outputs
