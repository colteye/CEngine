from __future__ import annotations

from enum import IntEnum


TARGET_TEXTURE_EXTENSIONS = {".dds"}
TARGET_AUDIO_EXTENSIONS = {".ogg", ".opus"}
PASSTHROUGH_TARGET_EXTENSIONS = {*TARGET_TEXTURE_EXTENSIONS, *TARGET_AUDIO_EXTENSIONS}
SOURCE_TEXTURE_EXTENSIONS = {".png", ".tga"}


class AssetType(IntEnum):
    UNKNOWN = 0
    TEXTURE = 1
    MATERIAL = 2
    MESH = 3
    SKELETON = 4
    ANIMATION = 5
    PHYSICS = 6
    PREFAB = 7
    SCENE = 8
    AUDIO = 9
    VFX = 10
    NAVIGATION = 11
    SHADER = 12
    PACKAGE = 13
    ASSET = 14


ASSET_TYPE_NAMES = {
    AssetType.TEXTURE: "texture",
    AssetType.MATERIAL: "material",
    AssetType.MESH: "mesh",
    AssetType.SKELETON: "skeleton",
    AssetType.ANIMATION: "animation",
    AssetType.PHYSICS: "physics",
    AssetType.PREFAB: "prefab",
    AssetType.SCENE: "scene",
    AssetType.AUDIO: "audio",
    AssetType.VFX: "vfx",
    AssetType.NAVIGATION: "navigation",
    AssetType.SHADER: "shader",
    AssetType.PACKAGE: "package",
    AssetType.ASSET: "asset",
}

RUNTIME_EXTENSIONS = {
    ".dds": AssetType.TEXTURE,
    ".ogg": AssetType.AUDIO,
    ".opus": AssetType.AUDIO,
    ".cmat": AssetType.MATERIAL,
    ".cmesh": AssetType.MESH,
    ".cskel": AssetType.SKELETON,
    ".canim": AssetType.ANIMATION,
    ".cphys": AssetType.PHYSICS,
    ".cvfx": AssetType.VFX,
    ".cnav": AssetType.NAVIGATION,
    ".cshader": AssetType.SHADER,
    ".cpak": AssetType.PACKAGE,
    ".casset": AssetType.ASSET,
    ".cscene": AssetType.SCENE,
}


def asset_type_for_extension(extension: str) -> AssetType:
    return RUNTIME_EXTENSIONS.get(extension.lower(), AssetType.UNKNOWN)


def source_asset_type_for_extension(extension: str) -> AssetType:
    if extension.lower() in SOURCE_TEXTURE_EXTENSIONS:
        return AssetType.TEXTURE
    return asset_type_for_extension(extension)


def asset_type_for_name(name: str) -> AssetType:
    lowered = name.lower()
    for asset_type, asset_name in ASSET_TYPE_NAMES.items():
        if lowered == asset_name:
            return asset_type
    return AssetType.UNKNOWN
