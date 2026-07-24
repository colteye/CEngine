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
from typing import Iterable

from .formats import AssetType
from .paths import ProjectPaths, atomic_write_bytes, generic_path


STATE_VERSION = 1
REGISTRY_MAGIC = b"CEAR"
CACHE_MAGIC = b"CEAC"
STATE_HEADER = struct.Struct("<4sHHII")
REGISTRY_RECORD = struct.Struct("<16sIIIIIQ")
CACHE_RECORD = struct.Struct("<16sQ")


@dataclass
class AssetRecord:
    """TODO: Describe `AssetRecord`."""

    guid: bytes
    source: Path
    output: Path
    asset_type: AssetType
    source_hash: int


@dataclass
class CacheRecord:
    """TODO: Describe `CacheRecord`."""

    guid: bytes
    source_hash: int


def append_string(strings: bytearray, text: str) -> tuple[int, int]:
    """TODO: Describe `append_string`.

    Args:
        strings: TODO: Describe this parameter.
        text: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    encoded = text.encode("utf-8")
    offset = len(strings)
    strings.extend(encoded)
    return offset, len(encoded)


def load_registry(paths: ProjectPaths) -> list[AssetRecord]:
    """TODO: Describe `load_registry`.

    Args:
        paths: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    if not paths.registry_path.exists():
        return []

    data = paths.registry_path.read_bytes()
    if len(data) < STATE_HEADER.size:
        raise ValueError("invalid ceasset registry")

    magic, version, record_size, record_count, string_size = STATE_HEADER.unpack_from(data)
    if magic != REGISTRY_MAGIC or version != STATE_VERSION or record_size != REGISTRY_RECORD.size:
        raise ValueError("invalid ceasset registry")

    records_offset = STATE_HEADER.size
    strings_offset = records_offset + record_count * REGISTRY_RECORD.size
    strings_end = strings_offset + string_size
    if strings_end > len(data):
        raise ValueError("truncated ceasset registry")

    records: list[AssetRecord] = []
    strings = data[strings_offset:strings_end]
    for index in range(record_count):
        offset = records_offset + index * REGISTRY_RECORD.size
        guid, asset_type, source_offset, source_size, output_offset, output_size, source_hash = (
            REGISTRY_RECORD.unpack_from(data, offset)
        )
        source = strings[source_offset : source_offset + source_size].decode("utf-8")
        output = strings[output_offset : output_offset + output_size].decode("utf-8")
        records.append(AssetRecord(guid, Path(source), Path(output), AssetType(asset_type), source_hash))
    return records


def save_registry(paths: ProjectPaths, records: Iterable[AssetRecord]) -> None:
    """TODO: Describe `save_registry`.

    Args:
        paths: TODO: Describe this parameter.
        records: TODO: Describe this parameter.
    """
    strings = bytearray()
    packed_records = bytearray()
    record_list = list(records)
    for record in record_list:
        source_offset, source_size = append_string(strings, generic_path(record.source))
        output_offset, output_size = append_string(strings, generic_path(record.output))
        packed_records.extend(
            REGISTRY_RECORD.pack(
                record.guid,
                int(record.asset_type),
                source_offset,
                source_size,
                output_offset,
                output_size,
                record.source_hash,
            )
        )

    header = STATE_HEADER.pack(
        REGISTRY_MAGIC, STATE_VERSION, REGISTRY_RECORD.size, len(record_list), len(strings)
    )
    atomic_write_bytes(paths.registry_path, header + packed_records + strings)


def load_cache(paths: ProjectPaths) -> list[CacheRecord]:
    """TODO: Describe `load_cache`.

    Args:
        paths: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    if not paths.cache_path.exists():
        return []

    data = paths.cache_path.read_bytes()
    if len(data) < STATE_HEADER.size:
        return []

    magic, version, record_size, record_count, _ = STATE_HEADER.unpack_from(data)
    if magic != CACHE_MAGIC or version != STATE_VERSION or record_size != CACHE_RECORD.size:
        return []

    records_offset = STATE_HEADER.size
    if records_offset + record_count * CACHE_RECORD.size > len(data):
        return []

    records: list[CacheRecord] = []
    for index in range(record_count):
        guid, source_hash = CACHE_RECORD.unpack_from(data, records_offset + index * CACHE_RECORD.size)
        records.append(CacheRecord(guid, source_hash))
    return records


def save_cache(paths: ProjectPaths, records: Iterable[AssetRecord]) -> None:
    """TODO: Describe `save_cache`.

    Args:
        paths: TODO: Describe this parameter.
        records: TODO: Describe this parameter.
    """
    record_list = list(records)
    header = STATE_HEADER.pack(CACHE_MAGIC, STATE_VERSION, CACHE_RECORD.size, len(record_list), 0)
    payload = bytearray()
    for record in record_list:
        payload.extend(CACHE_RECORD.pack(record.guid, record.source_hash))
    atomic_write_bytes(paths.cache_path, header + payload)


def cache_matches(cache: Iterable[CacheRecord], guid: bytes, source_hash: int) -> bool:
    """TODO: Describe `cache_matches`.

    Args:
        cache: TODO: Describe this parameter.
        guid: TODO: Describe this parameter.
        source_hash: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    return any(record.guid == guid and record.source_hash == source_hash for record in cache)


def upsert_records(paths: ProjectPaths, new_records: Iterable[AssetRecord]) -> None:
    """TODO: Describe `upsert_records`.

    Args:
        paths: TODO: Describe this parameter.
        new_records: TODO: Describe this parameter.
    """
    records = load_registry(paths)
    for record in new_records:
        for index, existing in enumerate(records):
            if existing.guid == record.guid or existing.output == record.output:
                records[index] = record
                break
        else:
            records.append(record)
    save_registry(paths, records)
