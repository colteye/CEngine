#   _____ ______             _
#  / ____|  ____|           (_)
# | |    | |__   _ __   __ _ _ _ __   ___
# | |    |  __| | '_ \ / _` | | '_ \ / _ |
# | |____| |____| | | | (_| | | | | |  __/
#  \_____|______|_| |_|\__, |_|_| |_|\___|
#                       __/ |
#                      |___/

"""Schema-driven serializer for cooked CEngine payloads.

Author:
    Erik Coltey
"""

from __future__ import annotations

import math
import struct
from collections.abc import Mapping, Sequence
from typing import Any

from .game_schema import GameSchema


SCALARS = {
    "u8": ("<B", 0, 0xFF),
    "u16": ("<H", 0, 0xFFFF),
    "u32": ("<I", 0, 0xFFFFFFFF),
    "u64": ("<Q", 0, 0xFFFFFFFFFFFFFFFF),
    "i32": ("<i", -0x80000000, 0x7FFFFFFF),
    "f32": ("<f", None, None),
}


def _value(source: object, name: str) -> Any:
    if isinstance(source, Mapping):
        if name not in source:
            raise ValueError(f"wire value has no field {name}")
        return source[name]
    if not hasattr(source, name):
        raise ValueError(f"wire value has no field {name}")
    return getattr(source, name)


def _check(field: dict[str, Any], value: int | float, label: str) -> None:
    if isinstance(value, float) and not math.isfinite(value):
        raise ValueError(f"{label} must be finite")
    if "value" in field and value != field["value"]:
        raise ValueError(f"{label} must equal {field['value']}")
    if "min" in field and value < field["min"]:
        raise ValueError(f"{label} is below its minimum")
    if "max" in field and value > field["max"]:
        raise ValueError(f"{label} exceeds its maximum")
    if "mask" in field and int(value) & ~int(field["mask"]):
        raise ValueError(f"{label} contains unknown flag bits")
    if "values" in field and value not in field["values"]:
        raise ValueError(f"{label} is not a declared value")


def _pack_value(
    game: GameSchema,
    field: dict[str, Any],
    value: object,
    label: str,
) -> bytes:
    field_type = str(field["type"])
    if field_type in SCALARS:
        layout, minimum, maximum = SCALARS[field_type]
        if field_type == "f32":
            number: int | float = float(value)
        else:
            if not isinstance(value, int):
                raise ValueError(f"{label} must be an integer")
            number = value
            if number < int(minimum) or number > int(maximum):
                raise ValueError(f"{label} is outside {field_type}")
        _check(field, number, label)
        return struct.pack(layout, number)
    if field_type == "guid":
        encoded = bytes(value)
        if len(encoded) != 16:
            raise ValueError(f"{label} must contain 16 bytes")
        return encoded
    if field_type == "string":
        if not isinstance(value, str):
            raise ValueError(f"{label} must be a string")
        encoded = value.encode("utf-8")
        if len(encoded) > int(field["max_size"]) or (
            field.get("non_empty", False) and not encoded
        ):
            raise ValueError(f"{label} has an invalid encoded size")
        return struct.pack("<I", len(encoded)) + encoded
    if field_type == "bytes":
        encoded = bytes(value)
        if len(encoded) > int(field["max_size"]):
            raise ValueError(f"{label} is too large")
        return struct.pack("<I", len(encoded)) + encoded
    if field_type == "record":
        return pack_record(game, str(field["record"]), value)
    if field_type == "array":
        if not isinstance(value, Sequence) or len(value) != int(field["count"]):
            raise ValueError(f"{label} must contain {field['count']} items")
        item_field = {**field, "type": field["item"]}
        return b"".join(
            _pack_value(game, item_field, item, f"{label}[{index}]")
            for index, item in enumerate(value)
        )
    if field_type == "list":
        if not isinstance(value, Sequence):
            raise ValueError(f"{label} must be a sequence")
        if len(value) < int(field.get("min_count", 0)) or \
                len(value) > int(field["max_count"]):
            raise ValueError(f"{label} has an invalid item count")
        item_field = {**field, "type": field["item"]}
        return struct.pack("<I", len(value)) + b"".join(
            _pack_value(game, item_field, item, f"{label}[{index}]")
            for index, item in enumerate(value)
        )
    record = game.wire(field_type)
    if record is not None:
        return pack_record(game, field_type, value)
    raise ValueError(f"{label} uses unsupported wire type {field_type}")


def pack_record(game: GameSchema, name: str, value: object) -> bytes:
    """Serialize one owned record using the flattened game schema."""
    record = game.wire(name)
    if record is None:
        raise ValueError(f"game schema has no wire record {name}")
    return b"".join(
        _pack_value(
            game,
            field,
            _value(value, str(field["name"])),
            f"{name}.{field['name']}",
        )
        for field in record["fields"]
    )


class _Reader:
    def __init__(self, data: bytes) -> None:
        self.data = memoryview(data)
        self.offset = 0

    def take(self, size: int) -> bytes:
        if size < 0 or self.offset > len(self.data) - size:
            raise ValueError("wire payload is truncated")
        result = bytes(self.data[self.offset:self.offset + size])
        self.offset += size
        return result


def _unpack_value(
    game: GameSchema,
    reader: _Reader,
    field: dict[str, Any],
    label: str,
) -> object:
    field_type = str(field["type"])
    if field_type in SCALARS:
        layout, _, _ = SCALARS[field_type]
        value, = struct.unpack(layout, reader.take(struct.calcsize(layout)))
        _check(field, value, label)
        return value
    if field_type == "guid":
        return reader.take(16)
    if field_type in {"string", "bytes"}:
        size, = struct.unpack("<I", reader.take(4))
        if size > int(field["max_size"]):
            raise ValueError(f"{label} is too large")
        value = reader.take(size)
        if field_type == "bytes":
            return value
        decoded = value.decode("utf-8")
        if field.get("non_empty", False) and not decoded:
            raise ValueError(f"{label} must not be empty")
        return decoded
    if field_type == "record":
        return _unpack_record(game, reader, str(field["record"]))
    if field_type == "array":
        item = {**field, "type": field["item"]}
        return [
            _unpack_value(game, reader, item, f"{label}[{index}]")
            for index in range(int(field["count"]))
        ]
    if field_type == "list":
        count, = struct.unpack("<I", reader.take(4))
        if count < int(field.get("min_count", 0)) or \
                count > int(field["max_count"]):
            raise ValueError(f"{label} has an invalid item count")
        item = {**field, "type": field["item"]}
        return [
            _unpack_value(game, reader, item, f"{label}[{index}]")
            for index in range(count)
        ]
    if game.wire(field_type) is not None:
        return _unpack_record(game, reader, field_type)
    raise ValueError(f"{label} uses unsupported wire type {field_type}")


def _unpack_record(
    game: GameSchema, reader: _Reader, name: str
) -> dict[str, object]:
    record = game.wire(name)
    if record is None:
        raise ValueError(f"game schema has no wire record {name}")
    return {
        str(field["name"]): _unpack_value(
            game, reader, field, f"{name}.{field['name']}"
        )
        for field in record["fields"]
    }


def unpack_record(game: GameSchema, name: str, data: bytes) -> dict[str, object]:
    """Deserialize one schema record and reject trailing bytes."""
    reader = _Reader(data)
    value = _unpack_record(game, reader, name)
    if reader.offset != len(reader.data):
        raise ValueError("wire payload contains trailing bytes")
    return value
