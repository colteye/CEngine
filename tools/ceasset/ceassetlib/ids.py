from __future__ import annotations

from pathlib import Path


FNV_OFFSET = 14695981039346656037
FNV_PRIME = 1099511628211


def fnv1a(data: bytes, seed: int = FNV_OFFSET) -> int:
    value = seed
    for byte in data:
        value ^= byte
        value = (value * FNV_PRIME) & 0xFFFFFFFFFFFFFFFF
    return value


def hash_file(path: Path) -> int:
    value = FNV_OFFSET
    with path.open("rb") as handle:
        while True:
            chunk = handle.read(1024 * 1024)
            if not chunk:
                return value
            value = fnv1a(chunk, value)


def guid_from_stable_name(name: str) -> bytes:
    name_bytes = name.encode("utf-8")
    first = fnv1a(name_bytes)
    salted = fnv1a(b"CEngineAssetGuid")
    second = fnv1a(name_bytes, salted)
    return first.to_bytes(8, "little") + second.to_bytes(8, "little")


def guid_to_string(guid: bytes) -> str:
    raw = guid.hex()
    return f"{raw[0:8]}-{raw[8:12]}-{raw[12:16]}-{raw[16:20]}-{raw[20:32]}"
