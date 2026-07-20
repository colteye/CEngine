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
    asset_type: AssetType
    guid: bytes
    source_hash: int
    platform_target: str
    payload: bytes


def align_up(value: int, alignment: int) -> int:
    return (value + alignment - 1) & ~(alignment - 1)


def platform_bytes(platform_target: str) -> bytes:
    encoded = platform_target.encode("utf-8")[: PLATFORM_TARGET_SIZE - 1]
    return encoded + bytes(PLATFORM_TARGET_SIZE - len(encoded))


def write_binary_asset(path: Path, desc: AssetWriteDesc) -> None:
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


def make_asset_desc(
    asset_type: AssetType,
    stable_name: str,
    source_hash: int,
    payload: bytes,
) -> AssetWriteDesc:
    return AssetWriteDesc(
        asset_type=asset_type,
        guid=guid_from_stable_name(stable_name),
        source_hash=source_hash,
        platform_target="generic",
        payload=payload,
    )
