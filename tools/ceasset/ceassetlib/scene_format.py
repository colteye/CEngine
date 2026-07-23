from __future__ import annotations

import struct
from enum import IntEnum, IntFlag

from .game_schema import entity_struct, load_bundled_game

SCENE_MAGIC = b"CSCN"
SCENE_VERSION = 4
ENTITY_CLASS_VERSION = 1
INVALID_INDEX = 0xFFFFFFFF


class EntityFlags(IntFlag):
    ENABLED = 1 << 0


class PropFlags(IntFlag):
    VISIBLE = 1 << 0
    COLLISION_ENABLED = 1 << 1
    DYNAMIC = 1 << 2
    SHADOW_ONLY = 1 << 3


class LightFlags(IntFlag):
    ENABLED = 1 << 0
    CASTS_SHADOW = 1 << 1


class PostProcessFlags(IntFlag):
    BLOOM_ENABLED = 1 << 0
    TONE_MAPPING_ENABLED = 1 << 1
    DEPTH_OF_FIELD_ENABLED = 1 << 2
    SUN_LENS_FLARE_ENABLED = 1 << 3
    SSAO_ENABLED = 1 << 4


class EntityClassBlockFlags(IntFlag):
    REQUIRED = 1 << 0


# All offsets are unsigned 64-bit offsets relative to the start of the scene
# payload. Strings are UTF-8 byte ranges and are not NUL terminated.
SCENE_HEADER = struct.Struct("<4sHHQQIIQIIQIIQIIQQ")
SCENE_SETTINGS = struct.Struct("<3ff3fII3I")
ASSET_REFERENCE = struct.Struct("<16sIIII")
SCENE_ENTITY = struct.Struct("<IIIII")
ENTITY_CLASS_BLOCK = struct.Struct("<IIHHIIQQQQ")
ENTITY_CONNECTION = struct.Struct("<IIIIIIf")
TRANSFORM = struct.Struct("<3f4f3f")
_GAME_SCHEMA = load_bundled_game()


def _entity_record(classname: str) -> struct.Struct | None:
    entity = _GAME_SCHEMA.entity(classname)
    return entity_struct(entity) if entity is not None else None


PROP = _entity_record("prop")
PLAYER_ENTITY = _entity_record("player")
LIGHT_ENTITY = _entity_record("light")
SKYBOX_ENTITY = _entity_record("skybox")
EXPONENTIAL_HEIGHT_FOG_ENTITY = _entity_record("exponential_height_fog")
POST_PROCESS_ENTITY = _entity_record("post_process")

assert PROP is not None
assert LIGHT_ENTITY is not None
assert SKYBOX_ENTITY is not None
assert EXPONENTIAL_HEIGHT_FOG_ENTITY is not None
assert POST_PROCESS_ENTITY is not None

FIXED_STRUCTURE_SIZES = {
    "DiskSceneHeader": SCENE_HEADER.size,
    "DiskSceneSettings": SCENE_SETTINGS.size,
    "DiskAssetReference": ASSET_REFERENCE.size,
    "DiskSceneEntity": SCENE_ENTITY.size,
    "DiskEntityClassBlock": ENTITY_CLASS_BLOCK.size,
    "DiskEntityConnection": ENTITY_CONNECTION.size,
    "DiskTransform": TRANSFORM.size,
    "DiskProp": PROP.size,
    "DiskLightEntity": LIGHT_ENTITY.size,
    "DiskSkyboxEntity": SKYBOX_ENTITY.size,
    "DiskFogEntity": EXPONENTIAL_HEIGHT_FOG_ENTITY.size,
    "DiskPostProcessEntity": POST_PROCESS_ENTITY.size,
}
if PLAYER_ENTITY is not None:
    FIXED_STRUCTURE_SIZES["DiskPlayerEntity"] = PLAYER_ENTITY.size
