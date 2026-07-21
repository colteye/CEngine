from __future__ import annotations

import struct
from enum import IntEnum, IntFlag

SCENE_MAGIC = b"CSCN"
SCENE_VERSION = 1
ENTITY_CLASS_VERSION = 1
INVALID_INDEX = 0xFFFFFFFF


class EntityFlags(IntFlag):
    ENABLED = 1 << 0
    STATIC = 1 << 1


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
STATIC_PROP = struct.Struct("<3f4f3fIIII2f2ffI")
DYNAMIC_PROP = struct.Struct("<3f4f3fIIII3ff")
CAMERA_ENTITY = struct.Struct("<3f4f3fIffffI")
LIGHT_ENTITY = struct.Struct("<3f4f3fII3f4f2fI")
PREFAB_ENTITY = struct.Struct("<3f4f3fII")
TRIGGER_ENTITY = struct.Struct("<3f4f3f3fI")
PLAYER_START = struct.Struct("<3f4f3fI")

FIXED_STRUCTURE_SIZES = {
    "DiskSceneHeader": SCENE_HEADER.size,
    "DiskSceneSettings": SCENE_SETTINGS.size,
    "DiskAssetReference": ASSET_REFERENCE.size,
    "DiskSceneEntity": SCENE_ENTITY.size,
    "DiskEntityClassBlock": ENTITY_CLASS_BLOCK.size,
    "DiskEntityConnection": ENTITY_CONNECTION.size,
    "DiskTransform": TRANSFORM.size,
    "DiskStaticProp": STATIC_PROP.size,
    "DiskDynamicProp": DYNAMIC_PROP.size,
    "DiskCameraEntity": CAMERA_ENTITY.size,
    "DiskLightEntity": LIGHT_ENTITY.size,
    "DiskPrefabEntity": PREFAB_ENTITY.size,
    "DiskTriggerEntity": TRIGGER_ENTITY.size,
    "DiskPlayerStart": PLAYER_START.size,
}
