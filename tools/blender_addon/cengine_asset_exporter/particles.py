#   _____ ______             _
#  / ____|  ____|           (_)
# | |    | |__   _ __   __ _ _ _ __   ___
# | |    |  __| | '_ \ / _` | | '_ \ / _ |
# | |____| |____| | | | (_| | | | | |  __/
#  \_____|______|_| |_|\__, |_|_| |_|\___|
#                       __/ |
#                      |___/

"""Export Blender particle settings through the shared wire schema.

Author:
    Erik Coltey"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Iterable

from ceassetlib.assetfile import make_asset_desc, write_binary_asset
from ceassetlib.collection_export import clean_asset_name
from ceassetlib.formats import AssetType
from ceassetlib.game_schema import load_bundled_game
from ceassetlib.ids import guid_from_stable_name
from ceassetlib.paths import generic_path, output_dir_for_source
from ceassetlib.wire import pack_record


GAME = load_bundled_game()


@dataclass(frozen=True)
class ParticleExport:
    source: object
    output: Path


def _get(value: object, name: str, default: object) -> object:
    getter = getattr(value, "get", None)
    return getter(name, default) if callable(getter) else default


def particle_objects(objects: Iterable[object]) -> list[object]:
    return sorted(
        (obj for obj in objects if bool(getattr(obj, "particle_systems", ())) or
         bool(_get(obj, "ce_particle", False))),
        key=lambda obj: str(getattr(obj, "name", "")),
    )


def particle_payload(obj: object, asset_path: Callable[[Path], str] = generic_path) -> bytes:
    systems = getattr(obj, "particle_systems", ())
    settings = getattr(systems[0], "settings", obj) if systems else obj
    lifetime = float(_get(obj, "ce_particle_lifetime", getattr(settings, "lifetime", 1.0)))
    random_lifetime = float(getattr(settings, "lifetime_random", 0.0))
    speed = float(_get(obj, "ce_particle_speed", getattr(settings, "normal_factor", 1.0)))
    random_speed = float(getattr(settings, "factor_random", 0.0))
    count = float(getattr(settings, "count", 10.0))
    start = float(getattr(settings, "frame_start", 1.0))
    end = float(getattr(settings, "frame_end", start + 1.0))
    texture_value = str(_get(obj, "ce_particle_texture", "") or "")
    textures = []
    if texture_value:
        stored = asset_path(Path(texture_value))
        textures.append({
            "guid": guid_from_stable_name(stored),
            "type": int(AssetType.TEXTURE),
            "path": stored,
        })
    start_color = tuple(float(value) for value in
                        _get(obj, "ce_particle_start_color", (1.0, 1.0, 1.0, 1.0)))
    end_color = tuple(float(value) for value in
                      _get(obj, "ce_particle_end_color", (1.0, 1.0, 1.0, 0.0)))
    return pack_record(GAME, "particle", {
        "name": str(getattr(obj, "name", "Particle")),
        "textures": textures,
        "flags": (1 if bool(_get(obj, "ce_particle_loop", True)) else 0) |
                 (2 if bool(_get(obj, "ce_particle_world_space", False)) else 0),
        "blend_mode": int(_get(obj, "ce_particle_blend", 1)),
        "rate": float(_get(obj, "ce_particle_rate", count / max(end - start, 1.0))),
        "lifetime": (max(0.001, lifetime * (1.0 - random_lifetime)), max(0.001, lifetime)),
        "speed": (max(0.0, speed * (1.0 - random_speed)), max(0.0, speed)),
        "spread": float(_get(obj, "ce_particle_spread", 0.25)),
        "gravity": tuple(float(value) for value in
                         _get(obj, "ce_particle_gravity", (0.0, 0.0, -9.81))),
        "size": (float(_get(obj, "ce_particle_start_size", getattr(settings, "particle_size", 1.0))),
                 float(_get(obj, "ce_particle_end_size", 0.0))),
        "start_color": start_color,
        "end_color": end_color,
    })


def write_particle_assets(
    blend_source: Path,
    output_root: Path,
    objects: Iterable[object],
    asset_path: Callable[[Path], str] = generic_path,
    source_hash: int = 0,
) -> list[ParticleExport]:
    exports = []
    for obj in particle_objects(objects):
        output = output_dir_for_source(blend_source, output_root) / "particles" / \
            f"{clean_asset_name(str(obj.name))}.cparticle"
        payload = particle_payload(obj, asset_path)
        write_binary_asset(
            output,
            make_asset_desc(AssetType.PARTICLE, asset_path(output), source_hash, payload),
        )
        exports.append(ParticleExport(obj, output))
    return exports
