from __future__ import annotations

import json
import struct
from dataclasses import dataclass
from pathlib import Path
from typing import Any


GAME_FORMAT_VERSION = 1


@dataclass(frozen=True)
class GameSchema:
    path: Path
    game: dict[str, Any]
    content: dict[str, Any]
    asset_types: tuple[dict[str, Any], ...]
    entities: tuple[dict[str, Any], ...]

    def entity(self, classname: str) -> dict[str, Any] | None:
        return next(
            (entity for entity in self.entities
             if entity.get("classname") == classname),
            None,
        )


@dataclass(frozen=True)
class SchemaEntity:
    schema: dict[str, Any]
    values: dict[str, Any]

    @property
    def classname(self) -> str:
        return str(self.schema["classname"])


def make_schema_entity(
    game: GameSchema, classname: str, **overrides: object
) -> SchemaEntity:
    schema = game.entity(classname)
    if schema is None:
        raise ValueError(f"game does not define entity {classname}")
    values: dict[str, object] = {}
    for field in schema["fields"]:
        name = str(field["name"])
        field_type = field["type"]
        if field_type == "flags":
            for member in field["members"]:
                member_name = str(member["name"])
                values[member_name] = overrides.pop(
                    member_name, member.get("default", False))
        elif field_type == "asset_list":
            values[name] = overrides.pop(name, ())
        elif field_type == "transform":
            values[name] = overrides.pop(name, None)
        elif name in overrides:
            values[name] = overrides.pop(name)
        elif "default" in field:
            values[name] = field["default"]
        elif field.get("optional", False):
            values[name] = None
        else:
            raise ValueError(f"{classname}.{name} requires a value")
    if overrides:
        raise ValueError(
            f"{classname} has no field named {next(iter(overrides))}")
    return SchemaEntity(schema, values)


WIRE_FORMATS = {
    "f32": "f",
    "u32": "I",
    "bool": "I",
    "enum": "I",
    "vec2": "2f",
    "vec3": "3f",
    "transform": "3f4f3f",
    "asset": "I",
    "asset_list": "II",
    "flags": "I",
}


def entity_struct(entity: dict[str, Any]) -> struct.Struct:
    try:
        return struct.Struct(
            "<" + "".join(WIRE_FORMATS[field["type"]] for field in entity["fields"]))
    except (KeyError, TypeError) as error:
        raise ValueError(
            f"entity schema has an unsupported wire field: {entity.get('classname')}") from error


def _read_document(path: Path) -> dict[str, Any]:
    try:
        document = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ValueError(f"cannot read CEngine game file {path}: {error}") from error
    if not isinstance(document, dict) or \
            document.get("format_version") != GAME_FORMAT_VERSION:
        raise ValueError(f"unsupported CEngine game file: {path}")
    return document


def load_game_schema(path: Path) -> GameSchema:
    loaded: set[Path] = set()
    visiting: set[Path] = set()
    documents: list[dict[str, Any]] = []

    def visit(candidate: Path) -> None:
        resolved = candidate.resolve()
        if resolved in visiting:
            raise ValueError(f"CEngine game file import cycle contains {resolved}")
        if resolved in loaded:
            return
        visiting.add(resolved)
        document = _read_document(resolved)
        imports = document.get("imports", [])
        if not isinstance(imports, list):
            raise ValueError(f"CEngine game imports are invalid: {resolved}")
        for imported in imports:
            if not isinstance(imported, str):
                raise ValueError(f"CEngine game import is invalid: {resolved}")
            visit(resolved.parent / imported)
        visiting.remove(resolved)
        loaded.add(resolved)
        documents.append(document)

    visit(path)
    root = documents[-1]
    game = root.get("game", root.get("module"))
    entities = [
        entity
        for document in documents
        for entity in document.get("entities", [])
    ]
    asset_types = [
        asset_type
        for document in documents
        for asset_type in document.get("asset_types", [])
    ]
    if not isinstance(game, dict) or not isinstance(entities, list) or \
            not isinstance(asset_types, list):
        raise ValueError(f"CEngine game file is incomplete: {path}")
    classnames = [entity.get("classname") for entity in entities]
    if any(not isinstance(name, str) or not name for name in classnames) or \
            len(classnames) != len(set(classnames)):
        raise ValueError(f"CEngine game file has invalid entity names: {path}")
    return GameSchema(
        path=path.resolve(),
        game=game,
        content=root.get("content", {}),
        asset_types=tuple(asset_types),
        entities=tuple(entities),
    )


def bundled_game_path() -> Path:
    packaged = Path(__file__).with_name("game.json")
    if packaged.is_file():
        return packaged
    repository = Path(__file__).resolve().parents[3]
    return repository / "samples" / "viewer" / "game.json"


def load_bundled_game() -> GameSchema:
    return load_game_schema(bundled_game_path())
