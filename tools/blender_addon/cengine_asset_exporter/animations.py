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

from ceassetlib.assetfile import make_asset_desc, write_binary_asset
from ceassetlib.collection_export import clean_asset_name
from ceassetlib.formats import AssetType
from ceassetlib.game_schema import load_bundled_game
from ceassetlib.ids import guid_from_stable_name, hash_file
from ceassetlib.paths import generic_path, output_dir_for_source
from ceassetlib.wire import pack_record


GAME_SCHEMA = load_bundled_game()
INTERPOLATION_IDS = {
    "CONSTANT": 1,
    "LINEAR": 2,
    "BEZIER": 3,
}


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


def keyframe_record(keyframe: object) -> tuple[float, float, int]:
    """TODO: Describe `keyframe_record`.

    Args:
        keyframe: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    co = getattr(keyframe, "co", (0.0, 0.0))
    interpolation = str(getattr(keyframe, "interpolation", "")).upper()
    return (float(co[0]), float(co[1]), INTERPOLATION_IDS.get(interpolation, 0))


def animation_payload(
    source: Path,
    armature: object,
    action: object,
    fps: float,
    skeleton_path: str,
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
    del source, armature
    tracks: list[dict[str, object]] = []
    fcurves = sorted(
        getattr(action, "fcurves", ()),
        key=lambda item: (str(getattr(item, "data_path", "")), int(getattr(item, "array_index", 0))),
    )
    for fcurve in fcurves:
        keyframes = [keyframe_record(keyframe) for keyframe in getattr(fcurve, "keyframe_points", ())]
        tracks.append({
            "path": str(getattr(fcurve, "data_path", "")),
            "component": int(getattr(fcurve, "array_index", 0)),
            "keys": [
                {"frame": frame, "value": value, "interpolation": interpolation}
                for frame, value, interpolation in keyframes
            ],
        })
    return pack_record(GAME_SCHEMA, "animation", {
        "name": action.name,
        "skeleton": {
            "guid": guid_from_stable_name(skeleton_path),
            "type": int(AssetType.SKELETON),
            "path": skeleton_path,
        },
        "fps": float(fps),
        "start": start,
        "end": end,
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
        animation_payload(blend_source, armature, action, fps, skeleton_path),
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
        ))
    return outputs
