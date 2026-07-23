#ifndef CENGINE_ASSET_FORMAT_H
#define CENGINE_ASSET_FORMAT_H

#include <array>
#include <cstddef>
#include <cstdint>

namespace CEngine::Assets
{

using Guid = std::array<std::uint8_t, 16>;

constexpr std::array<char, 4> AssetMagic = {'C', 'E', 'A', 'F'};
constexpr std::uint16_t AssetFormatVersion = 1;
constexpr std::size_t PlatformTargetSize = 16;

enum class AssetType : std::uint32_t
{
    Unknown = 0,
    Texture,
    Material,
    Mesh,
    Skeleton,
    Animation,
    Physics,
    Prefab,
    Scene,
    Audio,
    Vfx,
    Navigation,
    Shader,
    Package,
    Asset,
};

#pragma pack(push, 1)
struct DiskAssetHeader
{
    std::array<char, 4> magic = AssetMagic;
    std::uint16_t version = AssetFormatVersion;
    std::uint16_t header_size = 0;
    std::uint32_t asset_type = 0;
    Guid guid = {};
    std::uint64_t source_hash = 0;
    std::array<char, PlatformTargetSize> platform_target = {};
    std::uint64_t payload_offset = 0;
    std::uint64_t payload_size = 0;
    std::uint64_t file_size = 0;
};

#pragma pack(pop)

static_assert(sizeof(DiskAssetHeader) == 76, "DiskAssetHeader must stay packed and stable.");

struct ByteView
{
    const std::uint8_t *data = nullptr;
    std::size_t size = 0;
};

} // namespace CEngine::Assets

#endif
