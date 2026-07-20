from __future__ import annotations

import sys
import time
from dataclasses import dataclass
from pathlib import Path

from .formats import (
    ASSET_TYPE_NAMES,
    PASSTHROUGH_TARGET_EXTENSIONS,
    SOURCE_TEXTURE_EXTENSIONS,
    AssetType,
    asset_type_for_extension,
)
from .ids import guid_from_stable_name, guid_to_string, hash_file
from .paths import (
    ProjectPaths,
    atomic_copyfile,
    generic_path,
    output_for_source,
    project_path,
    stored_path,
)
from .state import (
    AssetRecord,
    cache_matches,
    load_cache,
    load_registry,
    save_cache,
    save_registry,
    upsert_records,
)
from .texture import convert_texture_to_dds


@dataclass
class BuildOptions:
    force: bool = False
    dds_format: str = "DXT5"


def import_asset(paths: ProjectPaths, source_arg: Path) -> int:
    source = source_arg.resolve()
    if not source.exists():
        print(f"source file does not exist: {source}", file=sys.stderr)
        return 1

    asset_type = asset_type_for_extension(source.suffix)
    if asset_type == AssetType.UNKNOWN:
        print(f"no tested conversion path for {source.suffix or source.name}", file=sys.stderr)
        return 1

    records = load_registry(paths)
    stored_source = stored_path(paths.root, source)
    output = stored_path(paths.root, output_for_source(paths, source))
    guid = guid_from_stable_name(generic_path(output))
    source_hash = hash_file(source)

    record = AssetRecord(guid, stored_source, output, asset_type, source_hash)
    for index, existing in enumerate(records):
        if existing.guid == guid or existing.output == output:
            records[index] = record
            break
    else:
        records.append(record)

    save_registry(paths, records)
    print(f"imported {generic_path(stored_source)} as {ASSET_TYPE_NAMES[asset_type]} {guid_to_string(guid)}")
    return 0


def register_conversion(paths: ProjectPaths, source: Path, asset_type: AssetType, output: Path) -> None:
    records = load_registry(paths)
    stored_source = stored_path(paths.root, source)
    stored_output = stored_path(paths.root, output)
    guid = guid_from_stable_name(generic_path(stored_output))
    source_hash = hash_file(source)

    record = AssetRecord(guid, stored_source, stored_output, asset_type, source_hash)
    for index, existing in enumerate(records):
        if existing.guid == guid or existing.output == stored_output:
            records[index] = record
            break
    else:
        records.append(record)
    save_registry(paths, records)
    print(f"imported {generic_path(stored_source)} as {ASSET_TYPE_NAMES[asset_type]} {guid_to_string(guid)}")


def build_asset(paths: ProjectPaths, record: AssetRecord, options: BuildOptions) -> None:
    source = project_path(paths, record.source)
    output = project_path(paths, record.output)
    source_ext = source.suffix.lower()
    output_ext = output.suffix.lower()

    if source_ext in PASSTHROUGH_TARGET_EXTENSIONS:
        atomic_copyfile(source, output)
        return
    if record.asset_type == AssetType.TEXTURE and source_ext in SOURCE_TEXTURE_EXTENSIONS:
        if output_ext != ".dds":
            raise RuntimeError(f"texture target must be .dds: {generic_path(record.output)}")
        convert_texture_to_dds(source, output, options.dds_format)
        return
    raise RuntimeError(f"registered source no longer has a conversion path: {generic_path(record.source)}")


def build(paths: ProjectPaths, options: BuildOptions) -> int:
    records = load_registry(paths)
    if not records:
        print("no assets registered; use ceasset import <file> first")
        return 0

    cache = load_cache(paths)
    built_count = 0
    skipped_count = 0
    registry_changed = False

    for record in records:
        source = project_path(paths, record.source)
        source_hash = hash_file(source)
        if record.source_hash != source_hash:
            record.source_hash = source_hash
            registry_changed = True

        output = project_path(paths, record.output)
        if not options.force and output.exists() and cache_matches(cache, record.guid, source_hash):
            skipped_count += 1
            continue

        build_asset(paths, record, options)
        print(f"built {generic_path(record.output)}")
        built_count += 1

    if registry_changed:
        save_registry(paths, records)
    save_cache(paths, records)
    print(f"build complete: {built_count} built, {skipped_count} skipped")
    return 0


def convert_source(paths: ProjectPaths, source: Path) -> int:
    result = import_asset(paths, source)
    if result != 0:
        return result
    return build(paths, BuildOptions(force=True))


def convert_texture_source(paths: ProjectPaths, source_arg: Path, dds_format: str) -> int:
    source = source_arg.resolve()
    if not source.exists():
        print(f"source file does not exist: {source}", file=sys.stderr)
        return 1
    if source.suffix.lower() not in SOURCE_TEXTURE_EXTENSIONS:
        print(f"{source.suffix or source.name} is not a source texture extension", file=sys.stderr)
        return 1

    output = output_for_source(paths, source, ".dds")
    register_conversion(paths, source, AssetType.TEXTURE, output)
    return build(paths, BuildOptions(force=True, dds_format=dds_format))


def watch(paths: ProjectPaths, options: BuildOptions, interval_ms: int) -> int:
    print(f"watching {paths.registry_path} every {interval_ms}ms")
    while True:
        build(paths, options)
        time.sleep(interval_ms / 1000.0)
