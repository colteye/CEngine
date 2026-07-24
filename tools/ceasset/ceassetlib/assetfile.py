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

import struct
from dataclasses import dataclass
from pathlib import Path

from .formats import AssetType
from .ids import guid_from_stable_name
from .paths import atomic_write_bytes


ASSET_MAGIC = b"CEAF"
ASSET_VERSION = 1
PAYLOAD_ALIGNMENT = 16
PLATFORM_TARGET_SIZE = 16

ASSET_HEADER = struct.Struct("<4sHHI16sQ16sQQQ")


@dataclass
class AssetWriteDesc:
    """TODO: Describe `AssetWriteDesc`."""

    asset_type: AssetType
    guid: bytes
    source_hash: int
    platform_target: str
    payload: bytes


def align_up(value: int, alignment: int) -> int:
    """TODO: Describe `align_up`.

    Args:
        value: TODO: Describe this parameter.
        alignment: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    return (value + alignment - 1) & ~(alignment - 1)


def platform_bytes(platform_target: str) -> bytes:
    """TODO: Describe `platform_bytes`.

    Args:
        platform_target: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    encoded = platform_target.encode("utf-8")[: PLATFORM_TARGET_SIZE - 1]
    return encoded + bytes(PLATFORM_TARGET_SIZE - len(encoded))


def write_binary_asset(path: Path, desc: AssetWriteDesc) -> None:
    """TODO: Describe `write_binary_asset`.

    Args:
        path: TODO: Describe this parameter.
        desc: TODO: Describe this parameter.
    """
    if desc.asset_type == AssetType.UNKNOWN:
        raise RuntimeError("asset type must be known before writing")
    if not desc.payload:
        raise RuntimeError("asset payload must not be empty")

    payload_offset = align_up(ASSET_HEADER.size, PAYLOAD_ALIGNMENT)
    file_size = payload_offset + len(desc.payload)
    header = ASSET_HEADER.pack(
        ASSET_MAGIC,
        ASSET_VERSION,
        ASSET_HEADER.size,
        int(desc.asset_type),
        desc.guid,
        desc.source_hash,
        platform_bytes(desc.platform_target),
        payload_offset,
        len(desc.payload),
        file_size,
    )

    out = bytearray()
    out.extend(header)
    out.extend(bytes(payload_offset - len(out)))
    out.extend(desc.payload)

    atomic_write_bytes(path, out)


def read_binary_asset(
    path: Path, expected_type: AssetType | None = None
) -> tuple[AssetWriteDesc, bytes]:
    """Read and validate the one shared cooked-asset envelope."""
    data = path.read_bytes()
    if len(data) < ASSET_HEADER.size:
        raise ValueError("asset header is truncated")
    header = ASSET_HEADER.unpack_from(data)
    magic, version, size, type_value = header[:4]
    payload_offset, payload_size, file_size = header[7:10]
    if magic != ASSET_MAGIC or version != ASSET_VERSION or size != ASSET_HEADER.size:
        raise ValueError("asset header is unsupported")
    try:
        asset_type = AssetType(type_value)
    except ValueError as error:
        raise ValueError("asset type is invalid") from error
    if expected_type is not None and asset_type != expected_type:
        raise ValueError("asset type does not match")
    if file_size != len(data) or payload_offset > len(data) or \
            payload_size > len(data) - payload_offset:
        raise ValueError("asset payload range is invalid")
    platform = bytes(header[6]).split(b"\0", 1)[0].decode("utf-8")
    desc = AssetWriteDesc(asset_type, header[4], header[5], platform, b"")
    return desc, data[payload_offset:payload_offset + payload_size]


def make_asset_desc(
    asset_type: AssetType,
    stable_name: str,
    source_hash: int,
    payload: bytes,
) -> AssetWriteDesc:
    """TODO: Describe `make_asset_desc`.

    Args:
        asset_type: TODO: Describe this parameter.
        stable_name: TODO: Describe this parameter.
        source_hash: TODO: Describe this parameter.
        payload: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    return AssetWriteDesc(
        asset_type=asset_type,
        guid=guid_from_stable_name(stable_name),
        source_hash=source_hash,
        platform_target="generic",
        payload=payload,
    )
