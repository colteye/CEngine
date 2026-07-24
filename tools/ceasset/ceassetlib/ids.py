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

from pathlib import Path


FNV_OFFSET = 14695981039346656037
FNV_PRIME = 1099511628211


def fnv1a(data: bytes, seed: int = FNV_OFFSET) -> int:
    """TODO: Describe `fnv1a`.

    Args:
        data: TODO: Describe this parameter.
        seed: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    value = seed
    for byte in data:
        value ^= byte
        value = (value * FNV_PRIME) & 0xFFFFFFFFFFFFFFFF
    return value


def hash_file(path: Path) -> int:
    """TODO: Describe `hash_file`.

    Args:
        path: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    value = FNV_OFFSET
    with path.open("rb") as handle:
        while True:
            chunk = handle.read(1024 * 1024)
            if not chunk:
                return value
            value = fnv1a(chunk, value)


def guid_from_stable_name(name: str) -> bytes:
    """TODO: Describe `guid_from_stable_name`.

    Args:
        name: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    name_bytes = name.encode("utf-8")
    first = fnv1a(name_bytes)
    salted = fnv1a(b"CEngineAssetGuid")
    second = fnv1a(name_bytes, salted)
    return first.to_bytes(8, "little") + second.to_bytes(8, "little")


def guid_to_string(guid: bytes) -> str:
    """TODO: Describe `guid_to_string`.

    Args:
        guid: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    raw = guid.hex()
    return f"{raw[0:8]}-{raw[8:12]}-{raw[12:16]}-{raw[16:20]}-{raw[20:32]}"
