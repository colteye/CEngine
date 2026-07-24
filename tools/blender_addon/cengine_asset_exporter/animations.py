#   _____ ______             _
#  / ____|  ____|           (_)
# | |    | |__   _ __   __ _ _ _ __   ___
# | |    |  __| | '_ \ / _` | | '_ \ / _ |
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
import math
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Iterable

from ceassetlib.assetfile import make_asset_desc, write_binary_asset
from ceassetlib.collection_export import clean_asset_name
from ceassetlib.formats import AssetType
from ceassetlib.game_schema import load_bundled_game
from ceassetlib.ids import fnv1a, guid_from_stable_name, hash_file
from ceassetlib.paths import generic_path, output_dir_for_source
from ceassetlib.wire import pack_record

from .skeletons import (
    canonical_bones,
    engine_matrix,
    matrix_inverse,
    matrix_multiply,
    transform_record,
)

GAME_SCHEMA = load_bundled_game()
EVENT_PREFIX = "ce_event:"


@dataclass(frozen=True)
class AnimationExport:
    """TODO: Describe `AnimationExport`."""

    source: object
    armature: object
    output: Path


def elapsed(start: float) -> str:
    """TODO: Describe `elapsed`.

    Args:
        start: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    return f"{time.perf_counter() - start:.2f}s"


def animation_output_path(blend_source: Path, output_root: Path, armature_name: str, action_name: str) -> Path:
    """TODO: Describe `animation_output_path`.

    Args:
        blend_source: TODO: Describe this parameter.
        output_root: TODO: Describe this parameter.
        armature_name: TODO: Describe this parameter.
        action_name: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    name = f"{clean_asset_name(armature_name)}_{clean_asset_name(action_name)}"
    return output_dir_for_source(blend_source, output_root) / "animations" / f"{name}.canim"


def armature_actions(armature: object) -> list[object]:
    """TODO: Describe `armature_actions`.

    Args:
        armature: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    actions: list[object] = []
    seen: set[int] = set()

    animation_data = getattr(armature, "animation_data", None)
    active_action = getattr(animation_data, "action", None) if animation_data is not None else None
    if active_action is not None:
        seen.add(id(active_action))
        actions.append(active_action)

    nla_tracks = getattr(animation_data, "nla_tracks", ()) if animation_data is not None else ()
    for track in nla_tracks:
        for strip in getattr(track, "strips", ()):
            action = getattr(strip, "action", None)
            if action is None:
                continue
            key = id(action)
            if key in seen:
                continue
            seen.add(key)
            actions.append(action)

    return sorted(actions, key=lambda action: action.name)


def action_targets(objects: Iterable[object]) -> list[tuple[object, object]]:
    """TODO: Describe `action_targets`.

    Args:
        objects: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    targets: list[tuple[object, object]] = []
    for obj in sorted(objects, key=lambda item: item.name):
        if getattr(obj, "type", "") != "ARMATURE":
            continue
        for action in armature_actions(obj):
            targets.append((obj, action))
    return targets


def frame_range(action: object) -> tuple[float, float]:
    """TODO: Describe `frame_range`.

    Args:
        action: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    value = getattr(action, "frame_range", (0.0, 0.0))
    return (float(value[0]), float(value[1]))


def _scene_or_context(scene: object | None) -> object:
    if scene is not None:
        return scene
    try:
        import bpy
    except ImportError as error:
        raise RuntimeError("animation export requires a Blender scene") from error
    return bpy.context.scene


def _pose_bone(armature: object, name: str) -> object:
    bones = getattr(getattr(armature, "pose", None), "bones", ())
    getter = getattr(bones, "get", None)
    bone = getter(name) if callable(getter) else next(
        (value for value in bones if getattr(value, "name", "") == name), None)
    if bone is None:
        raise RuntimeError(f"armature pose has no bone named {name}")
    return bone


def _sample_frames(start: float, end: float) -> list[float]:
    values = [start + float(index) for index in range(int(math.floor(end - start)) + 1)]
    if not values or abs(values[-1] - end) > 1.0e-6:
        values.append(end)
    return values


def evaluated_tracks(
    armature: object,
    action: object,
    fps: float,
    scene: object | None = None,
) -> tuple[float, list[dict[str, object]]]:
    if not math.isfinite(fps) or fps <= 0.0:
        raise RuntimeError("animation FPS must be finite and positive")
    start, end = frame_range(action)
    if end < start:
        raise RuntimeError(f"animation has an invalid frame range: {action.name}")
    duration = max((end - start) / fps, 1.0 / fps)
    ordered = canonical_bones(armature)
    scene = _scene_or_context(scene)
    animation_data = getattr(armature, "animation_data", None)
    if animation_data is None:
        raise RuntimeError(f"armature has no animation data: {armature.name}")
    previous_action = getattr(animation_data, "action", None)
    previous_frame = int(getattr(scene, "frame_current", 0))
    previous_subframe = float(getattr(scene, "frame_subframe", 0.0))
    tracks: list[dict[str, object]] = [
        {"samples": []} for _ in ordered
    ]
    previous_rotations: list[tuple[float, ...] | None] = [None] * len(ordered)
    try:
        animation_data.action = action
        for frame in _sample_frames(start, end):
            whole = math.floor(frame)
            scene.frame_set(int(whole), subframe=float(frame - whole))
            absolute = [
                engine_matrix(getattr(_pose_bone(armature, bone.name), "matrix", None))
                for bone in ordered
            ]
            for index, bone in enumerate(ordered):
                parent = getattr(bone, "parent", None)
                if parent is None:
                    local = absolute[index]
                else:
                    parent_index = next(
                        (candidate for candidate, value in enumerate(ordered)
                         if value.name == parent.name), -1)
                    if parent_index < 0:
                        raise RuntimeError("pose hierarchy differs from skeleton hierarchy")
                    local = matrix_multiply(
                        matrix_inverse(absolute[parent_index]), absolute[index])
                value = transform_record(local)
                rotation = tuple(value["rotation"])
                previous = previous_rotations[index]
                if previous is not None and sum(
                        previous[item] * rotation[item] for item in range(4)) < 0.0:
                    rotation = tuple(-component for component in rotation)
                    value["rotation"] = rotation
                previous_rotations[index] = rotation
                tracks[index]["samples"].append({
                    "time": min((frame - start) / fps, duration),
                    "value": value,
                })
    finally:
        animation_data.action = previous_action
        scene.frame_set(previous_frame, subframe=previous_subframe)
    return duration, tracks


def animation_events(action: object, fps: float, start: float,
                     duration: float) -> list[dict[str, object]]:
    events = []
    ids: dict[int, str] = {}
    for marker in getattr(action, "pose_markers", ()):
        marker_name = str(getattr(marker, "name", ""))
        if not marker_name.startswith(EVENT_PREFIX):
            continue
        name = marker_name[len(EVENT_PREFIX):].strip()
        if not name:
            raise RuntimeError("animation event marker name must not be empty")
        event_id = fnv1a(name.encode("utf-8"))
        previous = ids.get(event_id)
        if previous is not None and previous != name:
            raise RuntimeError("animation event names have a stable-ID collision")
        ids[event_id] = name
        time_seconds = (float(getattr(marker, "frame", start)) - start) / fps
        if time_seconds < 0.0 or time_seconds > duration:
            raise RuntimeError(f"animation event is outside the clip: {name}")
        events.append({"time": time_seconds, "id": event_id, "name": name})
    return sorted(events, key=lambda value: (value["time"], value["id"]))


def animation_payload(
    source: Path,
    armature: object,
    action: object,
    fps: float,
    skeleton_path: str,
    scene: object | None = None,
) -> bytes:
    """TODO: Describe `animation_payload`.

    Args:
        source: TODO: Describe this parameter.
        armature: TODO: Describe this parameter.
        action: TODO: Describe this parameter.
        fps: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    start, end = frame_range(action)
    del source, end
    duration, tracks = evaluated_tracks(armature, action, fps, scene)
    return pack_record(GAME_SCHEMA, "animation", {
        "name": action.name,
        "skeleton": {
            "guid": guid_from_stable_name(skeleton_path),
            "type": int(AssetType.SKELETON),
            "path": skeleton_path,
        },
        "duration": duration,
        "events": animation_events(action, fps, start, duration),
        "tracks": tracks,
    })


def write_animation_asset(
    blend_source: Path,
    output_root: Path,
    armature: object,
    action: object,
    fps: float,
    skeleton_output_path_for_name: Callable[[str], Path | None],
    asset_path: Callable[[Path], str] = generic_path,
    logger: Callable[[str], None] | None = None,
    source_hash: int | None = None,
    scene: object | None = None,
) -> AnimationExport:
    """TODO: Describe `write_animation_asset`.

    Args:
        blend_source: TODO: Describe this parameter.
        output_root: TODO: Describe this parameter.
        armature: TODO: Describe this parameter.
        action: TODO: Describe this parameter.
        fps: TODO: Describe this parameter.
        skeleton_output_path_for_name: TODO: Describe this parameter.
        asset_path: TODO: Describe this parameter.
        logger: TODO: Describe this parameter.
        source_hash: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    start = time.perf_counter()
    output = animation_output_path(blend_source, output_root, armature.name, action.name)
    skeleton_output = skeleton_output_path_for_name(armature.name)
    if skeleton_output is None:
        raise RuntimeError(f"animation has no exported skeleton: {armature.name}")
    skeleton_path = asset_path(skeleton_output)
    if logger is not None:
        logger(
            f"Animation {armature.name}/{action.name}: "
            f"{len(getattr(action, 'fcurves', ()))} f-curve(s) -> {output}"
        )

    desc = make_asset_desc(
        AssetType.ANIMATION,
        asset_path(output),
        source_hash if source_hash is not None else hash_file(blend_source),
        animation_payload(
            blend_source, armature, action, fps, skeleton_path, scene),
    )
    write_binary_asset(output, desc)
    if logger is not None:
        logger(f"Animation {armature.name}/{action.name}: wrote {output.name} in {elapsed(start)}")
    return AnimationExport(action, armature, output)


def write_animation_assets(
    blend_source: Path,
    output_root: Path,
    objects: Iterable[object],
    fps: float,
    skeleton_output_path_for_name: Callable[[str], Path | None],
    asset_path: Callable[[Path], str] = generic_path,
    logger: Callable[[str], None] | None = None,
    source_hash: int | None = None,
    scene: object | None = None,
) -> list[AnimationExport]:
    """TODO: Describe `write_animation_assets`.

    Args:
        blend_source: TODO: Describe this parameter.
        output_root: TODO: Describe this parameter.
        objects: TODO: Describe this parameter.
        fps: TODO: Describe this parameter.
        skeleton_output_path_for_name: TODO: Describe this parameter.
        asset_path: TODO: Describe this parameter.
        logger: TODO: Describe this parameter.
        source_hash: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    targets = action_targets(objects)
    if logger is not None:
        logger(f"Animation export queue: {len(targets)} armature/action target(s)")
    outputs: list[AnimationExport] = []
    for index, (armature, action) in enumerate(targets, start=1):
        if logger is not None:
            logger(f"Animation {index}/{len(targets)}: {armature.name}/{action.name}")
        outputs.append(write_animation_asset(
            blend_source,
            output_root,
            armature,
            action,
            fps,
            skeleton_output_path_for_name,
            asset_path,
            logger,
            source_hash,
            scene,
        ))
    return outputs
