#ifndef CENGINE_TESTS_ASSETS_TEST_ASSET_WRITER_H
#define CENGINE_TESTS_ASSETS_TEST_ASSET_WRITER_H

#include "assets/asset_format.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <vector>

namespace CEngine::Tests
{

inline Assets::Guid TestGuid(std::string_view name)
{
    constexpr std::uint64_t Offset = 14695981039346656037ull;
    constexpr std::uint64_t Prime = 1099511628211ull;
    const auto append = [](std::uint64_t hash, std::string_view value) {
        for (unsigned char byte : value)
        {
            hash ^= byte;
            hash *= Prime;
        }
        return hash;
    };
    const std::uint64_t first = append(Offset, name);
    const std::uint64_t second = append(append(Offset, "CEngineAssetGuid"), name);
    Assets::Guid guid = {};
    for (std::size_t index = 0; index < 8; ++index)
    {
        guid[index] = static_cast<std::uint8_t>(first >> (index * 8));
        guid[index + 8] = static_cast<std::uint8_t>(second >> (index * 8));
    }
    return guid;
}

inline bool WriteTestAsset(const std::filesystem::path &path, Assets::AssetType type,
                           const std::vector<std::uint8_t> &payload, const Assets::Guid &guid = {},
                           std::uint64_t source_hash = 0, std::string_view platform = "generic")
{
    if (type == Assets::AssetType::Unknown || payload.empty())
    {
        return false;
    }

    constexpr std::size_t PayloadOffset = 80;
    std::vector<std::uint8_t> bytes(PayloadOffset, 0);
    const auto write_u16 = [&bytes](std::size_t offset, std::uint16_t value) {
        bytes[offset] = static_cast<std::uint8_t>(value);
        bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8u);
    };
    const auto write_u32 = [&bytes](std::size_t offset, std::uint32_t value) {
        for (std::uint32_t byte = 0; byte < 4; ++byte)
        {
            bytes[offset + byte] = static_cast<std::uint8_t>(value >> (byte * 8u));
        }
    };
    const auto write_u64 = [&bytes](std::size_t offset, std::uint64_t value) {
        for (std::uint32_t byte = 0; byte < 8; ++byte)
        {
            bytes[offset + byte] = static_cast<std::uint8_t>(value >> (byte * 8u));
        }
    };

    std::copy(Assets::AssetMagic.begin(), Assets::AssetMagic.end(), bytes.begin());
    write_u16(4, Assets::AssetFormatVersion);
    write_u16(6, sizeof(Assets::DiskAssetHeader));
    write_u32(8, static_cast<std::uint32_t>(type));
    std::copy(guid.begin(), guid.end(), bytes.begin() + 12);
    write_u64(28, source_hash);
    std::copy_n(platform.begin(), std::min(platform.size(), Assets::PlatformTargetSize - 1), bytes.begin() + 36);
    write_u64(52, PayloadOffset);
    write_u64(60, payload.size());
    write_u64(68, PayloadOffset + payload.size());
    bytes.insert(bytes.end(), payload.begin(), payload.end());

    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return file.good();
}

} // namespace CEngine::Tests

#endif
