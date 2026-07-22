from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path

from .assetfile import ASSET_HEADER, AssetWriteDesc, align_up, write_binary_asset
from .formats import AssetType
from .ids import guid_from_stable_name
from .paths import atomic_write_bytes
from .scene_format import (
    ASSET_REFERENCE, ENTITY_CLASS_BLOCK, ENTITY_CLASS_VERSION, ENTITY_CONNECTION,
    EXPONENTIAL_HEIGHT_FOG_ENTITY, SCENE_ENTITY, SCENE_HEADER, SCENE_MAGIC,
    SCENE_SETTINGS, SCENE_VERSION, SKYBOX_ENTITY, TRANSFORM,
)


@dataclass(frozen=True)
class SkyboxPatch:
    panorama_path: str
    intensity: float = 1.0
    rotation_radians: float = 0.0


@dataclass(frozen=True)
class FogPatch:
    base_height: float = 0.0
    inscattering_color: tuple[float, float, float] = (0.55, 0.65, 0.78)
    density: float = 0.015
    height_falloff: float = 0.2
    start_distance: float = 0.0
    max_opacity: float = 0.85
    cutoff_distance: float = 0.0


def _append_aligned(output: bytearray, data: bytes) -> int:
    offset = align_up(len(output), 16)
    output.extend(bytes(offset - len(output)))
    output.extend(data)
    return offset


def patch_environment_entities(path: Path, skybox: SkyboxPatch, fog: FogPatch) -> None:
    file_data = path.read_bytes()
    outer = ASSET_HEADER.unpack_from(file_data)
    if outer[3] != int(AssetType.SCENE):
        raise ValueError("environment patch target is not a scene asset")
    payload = file_data[outer[7]:outer[7] + outer[8]]
    header = SCENE_HEADER.unpack_from(payload)
    if header[0] != SCENE_MAGIC or header[1] != SCENE_VERSION:
        raise ValueError("environment patch target uses an unsupported scene format")

    settings_offset, asset_offset, asset_count, asset_stride = header[3:7]
    entity_offset, entity_count, entity_stride = header[7:10]
    class_offset, class_count, class_stride = header[10:13]
    connection_offset, connection_count, connection_stride = header[13:16]
    string_offset, string_size = header[16:18]
    strings = bytearray(payload[string_offset:string_offset + string_size])

    def text(offset: int, size: int) -> str:
        return bytes(strings[offset:offset + size]).decode("utf-8")

    def add_text(value: str) -> tuple[int, int]:
        encoded = value.encode("utf-8")
        offset = len(strings)
        strings.extend(encoded)
        return offset, len(encoded)

    old_blocks = []
    existing_classes = set()
    for index in range(class_count):
        block = ENTITY_CLASS_BLOCK.unpack_from(payload, class_offset + index * class_stride)
        classname = text(block[0], block[1])
        existing_classes.add(classname)
        old_blocks.append((classname, block,
            bytes(payload[block[6]:block[6] + block[4] * 4]),
            bytes(payload[block[7]:block[7] + block[4] * block[5]]),
            bytes(payload[block[8]:block[8] + block[9]]) if block[9] else b""))
    if "skybox" in existing_classes or "exponential_height_fog" in existing_classes:
        raise ValueError("scene already contains environment entities")

    panorama_string = add_text(skybox.panorama_path)
    sky_class = add_text("skybox")
    sky_name = add_text("CE_Skybox")
    fog_class = add_text("exponential_height_fog")
    fog_name = add_text("CE_ExponentialHeightFog")

    asset_rows = bytearray(payload[asset_offset:asset_offset + asset_count * asset_stride])
    asset_rows.extend(ASSET_REFERENCE.pack(
        guid_from_stable_name(skybox.panorama_path), int(AssetType.TEXTURE), 0,
        panorama_string[0], panorama_string[1]))
    entity_rows = bytearray(payload[entity_offset:entity_offset + entity_count * entity_stride])
    entity_rows.extend(SCENE_ENTITY.pack(*sky_class, *sky_name, 1))
    entity_rows.extend(SCENE_ENTITY.pack(*fog_class, *fog_name, 1))

    identity = (0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0)
    fog_transform = (0.0, 0.0, fog.base_height, *identity[3:])
    new_blocks = [
        ("skybox", (sky_class[0], sky_class[1], ENTITY_CLASS_VERSION, 1, 1,
            SKYBOX_ENTITY.size), (entity_count).to_bytes(4, "little"),
            SKYBOX_ENTITY.pack(*identity, asset_count, skybox.intensity,
                               skybox.rotation_radians, 1), b""),
        ("exponential_height_fog", (fog_class[0], fog_class[1], ENTITY_CLASS_VERSION, 1, 1,
            EXPONENTIAL_HEIGHT_FOG_ENTITY.size), (entity_count + 1).to_bytes(4, "little"),
            EXPONENTIAL_HEIGHT_FOG_ENTITY.pack(
                *fog_transform, *fog.inscattering_color, fog.density, fog.height_falloff,
                fog.start_distance, fog.max_opacity, fog.cutoff_distance, 1), b""),
    ]
    blocks = old_blocks + new_blocks
    blocks.sort(key=lambda item: item[0])

    output = bytearray(SCENE_HEADER.size)
    new_settings_offset = _append_aligned(output,
        bytes(payload[settings_offset:settings_offset + SCENE_SETTINGS.size]))
    new_asset_offset = _append_aligned(output, asset_rows)
    new_entity_offset = _append_aligned(output, entity_rows)
    new_class_offset = align_up(len(output), 16)
    output.extend(bytes(new_class_offset - len(output) + len(blocks) * ENTITY_CLASS_BLOCK.size))
    class_rows = bytearray()
    for _, descriptor, indices, records, auxiliary in blocks:
        if len(descriptor) == 10:
            class_head = descriptor[:6]
        else:
            class_head = descriptor
        indices_offset = _append_aligned(output, indices)
        records_offset = _append_aligned(output, records)
        auxiliary_offset = _append_aligned(output, auxiliary) if auxiliary else 0
        class_rows.extend(ENTITY_CLASS_BLOCK.pack(
            *class_head, indices_offset, records_offset, auxiliary_offset, len(auxiliary)))
    output[new_class_offset:new_class_offset + len(class_rows)] = class_rows

    connection_rows = (bytes(payload[connection_offset:connection_offset +
        connection_count * connection_stride]) if connection_count else b"")
    new_connection_offset = _append_aligned(output, connection_rows) if connection_rows else 0
    new_string_offset = _append_aligned(output, strings)
    SCENE_HEADER.pack_into(output, 0, SCENE_MAGIC, SCENE_VERSION, SCENE_HEADER.size,
        new_settings_offset, new_asset_offset, asset_count + 1, ASSET_REFERENCE.size,
        new_entity_offset, entity_count + 2, SCENE_ENTITY.size,
        new_class_offset, len(blocks), ENTITY_CLASS_BLOCK.size,
        new_connection_offset, connection_count,
        ENTITY_CONNECTION.size if connection_count else 0,
        new_string_offset, len(strings))

    platform = bytes(outer[6]).split(b"\0", 1)[0].decode("utf-8")
    write_binary_asset(path, AssetWriteDesc(
        AssetType.SCENE, outer[4], outer[5], platform, bytes(output)))


def update_fog_entity(path: Path, fog: FogPatch) -> None:
    """Update an existing fog record without recooking any other scene data."""
    file_data = bytearray(path.read_bytes())
    outer = ASSET_HEADER.unpack_from(file_data)
    if outer[3] != int(AssetType.SCENE):
        raise ValueError("environment patch target is not a scene asset")
    payload_offset = outer[7]
    payload = memoryview(file_data)[payload_offset:payload_offset + outer[8]]
    header = SCENE_HEADER.unpack_from(payload)
    if header[0] != SCENE_MAGIC or header[1] != SCENE_VERSION:
        raise ValueError("environment patch target uses an unsupported scene format")

    class_offset, class_count, class_stride = header[10:13]
    string_offset, string_size = header[16:18]
    strings = payload[string_offset:string_offset + string_size]
    for index in range(class_count):
        block = ENTITY_CLASS_BLOCK.unpack_from(payload, class_offset + index * class_stride)
        classname = bytes(strings[block[0]:block[0] + block[1]]).decode("utf-8")
        if classname != "exponential_height_fog":
            continue
        if block[4] != 1 or block[5] != EXPONENTIAL_HEIGHT_FOG_ENTITY.size:
            raise ValueError("scene must contain exactly one exponential height fog entity")
        existing = EXPONENTIAL_HEIGHT_FOG_ENTITY.unpack_from(payload, block[7])
        transform = (*existing[:2], fog.base_height, *existing[3:10])
        EXPONENTIAL_HEIGHT_FOG_ENTITY.pack_into(
            file_data, payload_offset + block[7], *transform, *fog.inscattering_color,
            fog.density, fog.height_falloff, fog.start_distance, fog.max_opacity,
            fog.cutoff_distance, existing[-1])
        atomic_write_bytes(path, file_data)
        return
    raise ValueError("scene does not contain an exponential height fog entity")


def main() -> None:
    parser = argparse.ArgumentParser(description="Add skybox and height-fog entities without recooking a scene")
    parser.add_argument("scene", type=Path)
    parser.add_argument("panorama")
    args = parser.parse_args()
    patch_environment_entities(args.scene, SkyboxPatch(args.panorama), FogPatch())


if __name__ == "__main__":
    main()
