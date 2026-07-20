from __future__ import annotations

import re
import struct
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Iterable, Protocol

from .assetfile import make_asset_desc, write_binary_asset
from .formats import AssetType
from .ids import hash_file
from .paths import generic_path, output_dir_for_source, stored_path


COLLECTION_PREFIXES = {
    "PREFAB_": AssetType.PREFAB,
    "SCENE_": AssetType.SCENE,
}

RUNTIME_EXTENSIONS = {
    AssetType.PREFAB: ".casset",
    AssetType.SCENE: ".casset",
}

OBJECT_ROLE_PREFIXES = (
    ("SM_", "static_mesh"),
    ("SK_", "skeletal_mesh"),
    ("ARM_", "armature"),
    ("LOD0_", "lod"),
    ("LOD1_", "lod"),
    ("COL_", "collision"),
    ("UCX_", "convex_collision"),
    ("TRI_", "triangle_collision"),
    ("SOCKET_", "socket"),
    ("PHYS_", "physics"),
    ("JIGGLE_", "spring_chain"),
    ("FLEX_", "morph_target"),
    ("NAV_", "nav_mesh"),
    ("OCC_", "occluder"),
)

CASSET_HEADER = struct.Struct("<4sHHIIIIIIIIIII")
CASSET_OBJECT = struct.Struct("<IIIIIIII16f")
CASSET_COMPONENT = struct.Struct("<III")
CASSET_MAGIC = b"CECA"
CASSET_VERSION = 1
COMPOSITION_TYPE_IDS = {
    AssetType.PREFAB: 1,
    AssetType.SCENE: 2,
}
COMPONENT_KIND_IDS = {
    "mesh": 1,
    "materials": 2,
    "material": 2,
    "skeleton": 3,
    "animations": 4,
    "animation": 4,
}
OBJECT_ROLE_IDS = {
    "object": 0,
    "static_mesh": 1,
    "skeletal_mesh": 2,
    "armature": 3,
    "lod": 4,
    "collision": 5,
    "convex_collision": 6,
    "triangle_collision": 7,
    "socket": 8,
    "physics": 9,
    "spring_chain": 10,
    "morph_target": 11,
    "nav_mesh": 12,
    "occluder": 13,
}
OBJECT_TYPE_IDS = {
    "MESH": 1,
    "ARMATURE": 2,
    "EMPTY": 3,
    "LIGHT": 4,
    "CAMERA": 5,
}


class BlenderObjectLike(Protocol):
    name: str
    type: str

    def get(self, key: str, default: object = None) -> object:
        ...


class BlenderCollectionLike(Protocol):
    name: str
    objects: Iterable[BlenderObjectLike]

    def get(self, key: str, default: object = None) -> object:
        ...


@dataclass(frozen=True)
class CollectionExportSpec:
    asset_type: AssetType
    asset_name: str
    collection_name: str


AssetPath = Callable[[Path], str]
ObjectAssets = Callable[[BlenderObjectLike], dict[str, object]]
Logger = Callable[[str], None]


def elapsed(start: float) -> str:
    return f"{time.perf_counter() - start:.2f}s"


def clean_asset_name(name: str) -> str:
    cleaned = re.sub(r"[^A-Za-z0-9_.-]+", "_", name.strip())
    cleaned = cleaned.strip("._-")
    if not cleaned:
        raise RuntimeError("collection asset name is empty")
    return cleaned


def asset_type_from_text(text: str) -> AssetType:
    lowered = text.lower()
    if lowered == "prefab":
        return AssetType.PREFAB
    if lowered == "scene":
        return AssetType.SCENE
    return AssetType.UNKNOWN


def collection_export_spec(
    collection: BlenderCollectionLike,
    default_asset_type: AssetType = AssetType.UNKNOWN,
    default_asset_name: str | None = None,
) -> CollectionExportSpec | None:
    custom_type = str(collection.get("ce_asset_type", "") or "")
    if custom_type:
        asset_type = asset_type_from_text(custom_type)
        if asset_type not in RUNTIME_EXTENSIONS:
            raise RuntimeError(f"unsupported collection asset type: {custom_type}")
        asset_name = str(collection.get("ce_asset_name", "") or collection.name)
        return CollectionExportSpec(asset_type, clean_asset_name(asset_name), collection.name)

    for prefix, asset_type in COLLECTION_PREFIXES.items():
        if collection.name.startswith(prefix):
            return CollectionExportSpec(
                asset_type,
                clean_asset_name(collection.name[len(prefix) :]),
                collection.name,
            )
    if default_asset_type in RUNTIME_EXTENSIONS:
        return CollectionExportSpec(
            default_asset_type,
            clean_asset_name(default_asset_name or collection.name),
            collection.name,
        )
    return None


def collection_objects(collection: BlenderCollectionLike) -> list[BlenderObjectLike]:
    objects = getattr(collection, "all_objects", None)
    if objects is None:
        objects = collection.objects
    return sorted(list(objects), key=lambda obj: obj.name)


def object_role(obj: BlenderObjectLike) -> str:
    custom_role = str(obj.get("ce_role", "") or "")
    if custom_role:
        return clean_asset_name(custom_role).lower()

    for prefix, role in OBJECT_ROLE_PREFIXES:
        if obj.name.startswith(prefix):
            return role
    return "object"


def matrix_multiply(left: list[list[float]], right: list[list[float]]) -> list[list[float]]:
    return [
        [sum(left[row][index] * right[index][column] for index in range(4)) for column in range(4)]
        for row in range(4)
    ]


def blender_to_engine_matrix_rows(matrix: object | None) -> list[float]:
    if matrix is None:
        matrix_rows = [
            [1.0, 0.0, 0.0, 0.0],
            [0.0, 1.0, 0.0, 0.0],
            [0.0, 0.0, 1.0, 0.0],
            [0.0, 0.0, 0.0, 1.0],
        ]
    else:
        matrix_rows = [[float(matrix[row][column]) for column in range(4)] for row in range(4)]

    blender_to_engine = [
        [1.0, 0.0, 0.0, 0.0],
        [0.0, 0.0, 1.0, 0.0],
        [0.0, -1.0, 0.0, 0.0],
        [0.0, 0.0, 0.0, 1.0],
    ]
    engine_to_blender = [
        [1.0, 0.0, 0.0, 0.0],
        [0.0, 0.0, -1.0, 0.0],
        [0.0, 1.0, 0.0, 0.0],
        [0.0, 0.0, 0.0, 1.0],
    ]
    converted = matrix_multiply(matrix_multiply(blender_to_engine, matrix_rows), engine_to_blender)
    return [converted[row][column] for row in range(4) for column in range(4)]


def matrix_rows(obj: BlenderObjectLike) -> list[float]:
    matrix = getattr(obj, "matrix_world", None)
    return blender_to_engine_matrix_rows(matrix)


def parent_name(obj: BlenderObjectLike, object_names: set[str]) -> str:
    parent = getattr(obj, "parent", None)
    if parent is None or parent.name not in object_names:
        return ""
    return parent.name


def material_names(obj: BlenderObjectLike) -> list[str]:
    names: list[str] = []
    for slot in getattr(obj, "material_slots", ()):
        material = getattr(slot, "material", None)
        if material is not None:
            names.append(material.name)
    return names


def empty_object_assets(_obj: BlenderObjectLike) -> dict[str, object]:
    return {}


def path_parts(path: str | Path) -> list[str]:
    return [
        part
        for part in generic_path(Path(path)).replace("\\", "/").split("/")
        if part and part != "."
    ]


def suffix_relative_path(base: Path, path: str) -> str | None:
    base_parts = path_parts(base)
    value_parts = path_parts(path)
    if not base_parts or len(value_parts) <= len(base_parts):
        return None

    for index in range(len(base_parts)):
        suffix = base_parts[index:]
        if value_parts[: len(suffix)] == suffix:
            return "/".join(value_parts[len(suffix) :])
    return None


def bundle_relative_path(path: str, bundle_dir: Path | None) -> str:
    if bundle_dir is None:
        return path

    value = Path(path)
    if value.is_absolute():
        return generic_path(stored_path(bundle_dir, value))

    relative = suffix_relative_path(bundle_dir, path)
    return relative if relative is not None else generic_path(value)


def object_record(
    obj: BlenderObjectLike,
    object_names: set[str],
    object_assets: ObjectAssets = empty_object_assets,
) -> dict[str, object]:
    return {
        "name": obj.name,
        "role": object_role(obj),
        "type": obj.type,
        "parent": parent_name(obj, object_names),
        "materials": material_names(obj),
        "assets": object_assets(obj),
        "world_from_local": matrix_rows(obj),
    }


def collection_payload(
    source: Path,
    spec: CollectionExportSpec,
    objects: Iterable[BlenderObjectLike],
    object_assets: ObjectAssets = empty_object_assets,
    bundle_dir: Path | None = None,
) -> bytes:
    object_list = sorted(list(objects), key=lambda obj: obj.name)
    object_names = {obj.name for obj in object_list}
    strings = bytearray()

    def append_string(text: str) -> tuple[int, int]:
        encoded = text.encode("utf-8")
        offset = len(strings)
        strings.extend(encoded)
        return offset, len(encoded)

    source_offset, source_size = append_string(generic_path(source))
    collection_offset, collection_size = append_string(spec.collection_name)
    object_rows = bytearray()
    component_rows = bytearray()

    for obj in object_list:
        assets = object_assets(obj)
        first_component = len(component_rows) // CASSET_COMPONENT.size
        for key in sorted(assets):
            kind = COMPONENT_KIND_IDS.get(key, 0)
            values = assets[key]
            paths = values if isinstance(values, list) else [values]
            for path in paths:
                if not isinstance(path, str):
                    continue
                path_offset, path_size = append_string(bundle_relative_path(path, bundle_dir))
                component_rows.extend(CASSET_COMPONENT.pack(kind, path_offset, path_size))

        component_count = len(component_rows) // CASSET_COMPONENT.size - first_component
        name_offset, name_size = append_string(obj.name)
        parent_offset, parent_size = append_string(parent_name(obj, object_names))
        role = object_role(obj)
        object_rows.extend(
            CASSET_OBJECT.pack(
                name_offset,
                name_size,
                OBJECT_ROLE_IDS.get(role, 0),
                OBJECT_TYPE_IDS.get(obj.type, 0),
                parent_offset,
                parent_size,
                first_component,
                component_count,
                *matrix_rows(obj),
            )
        )

    component_table_offset = CASSET_HEADER.size + len(object_rows)
    string_table_offset = component_table_offset + len(component_rows)
    header = CASSET_HEADER.pack(
        CASSET_MAGIC,
        CASSET_VERSION,
        CASSET_HEADER.size,
        COMPOSITION_TYPE_IDS[spec.asset_type],
        len(object_list),
        CASSET_HEADER.size,
        len(component_rows) // CASSET_COMPONENT.size,
        component_table_offset,
        string_table_offset,
        len(strings),
        source_offset,
        source_size,
        collection_offset,
        collection_size,
    )
    return bytes(header + object_rows + component_rows + strings)


def write_collection_asset(
    source: Path,
    output_root: Path,
    collection: BlenderCollectionLike,
    object_assets: ObjectAssets = empty_object_assets,
    asset_path: AssetPath = generic_path,
    default_asset_type: AssetType = AssetType.UNKNOWN,
    default_asset_name: str | None = None,
    payload_source: Path | None = None,
    logger: Logger | None = None,
    export_objects: Iterable[BlenderObjectLike] | None = None,
    source_hash: int | None = None,
) -> Path | None:
    start = time.perf_counter()
    spec = collection_export_spec(collection, default_asset_type, default_asset_name)
    if spec is None:
        if logger is not None:
            logger(f"Collection skipped: {collection.name} is not tagged as a CEngine asset")
        return None

    extension = RUNTIME_EXTENSIONS[spec.asset_type]
    output = output_dir_for_source(source, output_root) / f"{spec.asset_name}{extension}"
    objects = collection_objects(collection) if export_objects is None else sorted(list(export_objects), key=lambda obj: obj.name)
    if logger is not None:
        logger(f"Collection {collection.name}: writing {len(objects)} object row(s) -> {output}")
    payload = collection_payload(payload_source or source, spec, objects, object_assets, output.parent)
    desc = make_asset_desc(
        AssetType.ASSET,
        asset_path(output),
        source_hash if source_hash is not None else hash_file(source),
        payload,
    )
    write_binary_asset(output, desc)
    if logger is not None:
        logger(f"Collection {collection.name}: wrote {output.name} ({len(payload)} payload bytes) in {elapsed(start)}")
    return output


def write_collection_assets(
    source: Path,
    output_root: Path,
    collections: Iterable[BlenderCollectionLike],
    object_assets: ObjectAssets = empty_object_assets,
    asset_path: AssetPath = generic_path,
    default_asset_type: AssetType = AssetType.UNKNOWN,
    default_asset_name: str | None = None,
    payload_source: Path | None = None,
    logger: Logger | None = None,
    export_objects: Iterable[BlenderObjectLike] | None = None,
    source_hash: int | None = None,
) -> list[Path]:
    outputs: list[Path] = []
    for collection in collections:
        output = write_collection_asset(
            source,
            output_root,
            collection,
            object_assets,
            asset_path,
            default_asset_type,
            default_asset_name,
            payload_source,
            logger,
            export_objects,
            source_hash,
        )
        if output is not None:
            outputs.append(output)
    return outputs
