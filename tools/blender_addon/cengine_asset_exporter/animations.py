from __future__ import annotations

import struct
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Iterable

from ceassetlib.assetfile import make_asset_desc, write_binary_asset
from ceassetlib.collection_export import clean_asset_name
from ceassetlib.formats import AssetType
from ceassetlib.ids import hash_file
from ceassetlib.paths import generic_path, output_dir_for_source


ANIMATION_HEADER = struct.Struct("<4sHHfffIIIIIIIIIII")
ANIMATION_TRACK = struct.Struct("<IIiII")
ANIMATION_KEYFRAME = struct.Struct("<ffI")
ANIMATION_MAGIC = b"CEAN"
ANIMATION_VERSION = 1
INTERPOLATION_IDS = {
    "CONSTANT": 1,
    "LINEAR": 2,
    "BEZIER": 3,
}


@dataclass(frozen=True)
class AnimationExport:
    source: object
    armature: object
    output: Path


def elapsed(start: float) -> str:
    return f"{time.perf_counter() - start:.2f}s"


def animation_output_path(blend_source: Path, output_root: Path, armature_name: str, action_name: str) -> Path:
    name = f"{clean_asset_name(armature_name)}_{clean_asset_name(action_name)}"
    return output_dir_for_source(blend_source, output_root) / "animations" / f"{name}.canim"


def armature_actions(armature: object) -> list[object]:
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
    targets: list[tuple[object, object]] = []
    for obj in sorted(objects, key=lambda item: item.name):
        if getattr(obj, "type", "") != "ARMATURE":
            continue
        for action in armature_actions(obj):
            targets.append((obj, action))
    return targets


def frame_range(action: object) -> tuple[float, float]:
    value = getattr(action, "frame_range", (0.0, 0.0))
    return (float(value[0]), float(value[1]))


def keyframe_record(keyframe: object) -> tuple[float, float, int]:
    co = getattr(keyframe, "co", (0.0, 0.0))
    interpolation = str(getattr(keyframe, "interpolation", "")).upper()
    return (float(co[0]), float(co[1]), INTERPOLATION_IDS.get(interpolation, 0))


def append_string(strings: bytearray, text: str) -> tuple[int, int]:
    encoded = text.encode("utf-8")
    offset = len(strings)
    strings.extend(encoded)
    return offset, len(encoded)


def animation_payload(source: Path, armature: object, action: object, fps: float) -> bytes:
    start, end = frame_range(action)
    strings = bytearray()
    source_offset, source_size = append_string(strings, generic_path(source))
    name_offset, name_size = append_string(strings, action.name)
    armature_offset, armature_size = append_string(strings, armature.name)

    track_rows = bytearray()
    keyframe_rows = bytearray()
    fcurves = sorted(
        getattr(action, "fcurves", ()),
        key=lambda item: (str(getattr(item, "data_path", "")), int(getattr(item, "array_index", 0))),
    )
    for fcurve in fcurves:
        data_path_offset, data_path_size = append_string(strings, str(getattr(fcurve, "data_path", "")))
        keyframe_offset = len(keyframe_rows) // ANIMATION_KEYFRAME.size
        keyframes = [keyframe_record(keyframe) for keyframe in getattr(fcurve, "keyframe_points", ())]
        for keyframe in keyframes:
            keyframe_rows.extend(ANIMATION_KEYFRAME.pack(*keyframe))
        track_rows.extend(
            ANIMATION_TRACK.pack(
                data_path_offset,
                data_path_size,
                int(getattr(fcurve, "array_index", 0)),
                keyframe_offset,
                len(keyframes),
            )
        )

    keyframe_table_offset = ANIMATION_HEADER.size + len(track_rows)
    string_table_offset = keyframe_table_offset + len(keyframe_rows)
    header = ANIMATION_HEADER.pack(
        ANIMATION_MAGIC,
        ANIMATION_VERSION,
        ANIMATION_HEADER.size,
        float(fps),
        start,
        end,
        len(fcurves),
        ANIMATION_HEADER.size,
        keyframe_table_offset,
        string_table_offset,
        len(strings),
        source_offset,
        source_size,
        name_offset,
        name_size,
        armature_offset,
        armature_size,
    )
    return bytes(header + track_rows + keyframe_rows + strings)


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
    start = time.perf_counter()
    output = animation_output_path(blend_source, output_root, armature.name, action.name)
    del skeleton_output_path_for_name
    if logger is not None:
        logger(
            f"Animation {armature.name}/{action.name}: "
            f"{len(getattr(action, 'fcurves', ()))} f-curve(s) -> {output}"
        )

    desc = make_asset_desc(
        AssetType.ANIMATION,
        asset_path(output),
        source_hash if source_hash is not None else hash_file(blend_source),
        animation_payload(blend_source, armature, action, fps),
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
