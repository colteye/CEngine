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

import struct

from .game_schema import entity_struct, load_bundled_game

SCENE_MAGIC = b"CSCN"
SCENE_VERSION = 4
ENTITY_CLASS_VERSION = 1
INVALID_INDEX = 0xFFFFFFFF
ENTITY_CLASS_BLOCK_REQUIRED = 1 << 0


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
    """TODO: Describe `_entity_record`.

    Args:
        classname: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    entity = _GAME_SCHEMA.entity(classname)
    return entity_struct(entity) if entity is not None else None


SKYBOX_ENTITY = _entity_record("skybox")
EXPONENTIAL_HEIGHT_FOG_ENTITY = _entity_record("exponential_height_fog")

assert SKYBOX_ENTITY is not None
assert EXPONENTIAL_HEIGHT_FOG_ENTITY is not None
