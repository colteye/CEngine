//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/assets/asset_io.cpp
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#include "assets/asset_io.h"

#include "assets/asset_error.h"
#include "assets/binary.h"

#include <algorithm>
#include <cstring>
#include <fstream>

namespace CEngine::Assets
{
/**
 * @brief TODO: Describe AssetFile::Load.
 *
 * @param path TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool AssetFile::Load(const std::filesystem::path &path)
{
    bytes_.clear();
    header_ = {};

    if (!LoadFileBytes(path, bytes_))
    {
        return false;
    }
    return Validate();
}

/**
 * @brief TODO: Describe AssetFile::PlatformTarget.
 *
 * @return TODO: Describe the return value.
 */
std::string_view AssetFile::PlatformTarget() const
{
    const char *begin = header_.platform_target.data();
    const char *end = std::find(begin, begin + header_.platform_target.size(), '\0');
    return {begin, static_cast<std::size_t>(end - begin)};
}

/**
 * @brief TODO: Describe AssetFile::Payload.
 *
 * @return TODO: Describe the return value.
 */
ByteView AssetFile::Payload() const
{
    if (header_.payload_offset > bytes_.size() ||
        header_.payload_size > bytes_.size() - static_cast<std::size_t>(header_.payload_offset))
    {
        return {};
    }
    return {bytes_.data() + header_.payload_offset, static_cast<std::size_t>(header_.payload_size)};
}

/**
 * @brief TODO: Describe AssetFile::Validate.
 *
 * @return TODO: Describe the return value.
 */
bool AssetFile::Validate()
{
    if (bytes_.size() < sizeof(DiskAssetHeader))
    {
        return AssetError("asset is smaller than the common header");
    }
    const ByteView view{bytes_.data(), bytes_.size()};
    std::size_t offset = 0;
    std::memcpy(header_.magic.data(), bytes_.data(), header_.magic.size());
    offset += header_.magic.size();
    if (!ReadU16LE(view, offset, header_.version) || !ReadU16LE(view, offset, header_.header_size) ||
        !ReadU32LE(view, offset, header_.asset_type))
    {
        return AssetError("asset common header is truncated");
    }
    std::memcpy(header_.guid.data(), bytes_.data() + offset, header_.guid.size());
    offset += header_.guid.size();
    if (!ReadU64LE(view, offset, header_.source_hash))
    {
        return AssetError("asset common header is truncated");
    }
    std::memcpy(header_.platform_target.data(), bytes_.data() + offset, header_.platform_target.size());
    offset += header_.platform_target.size();
    if (!ReadU64LE(view, offset, header_.payload_offset) || !ReadU64LE(view, offset, header_.payload_size) ||
        !ReadU64LE(view, offset, header_.file_size))
    {
        return AssetError("asset common header is truncated");
    }

    if (header_.magic != AssetMagic)
    {
        return AssetError("asset magic does not match CEngine format");
    }
    if (header_.version != AssetFormatVersion)
    {
        return AssetError("asset format version is not supported");
    }
    if (header_.header_size != sizeof(DiskAssetHeader))
    {
        return AssetError("asset header size is not supported");
    }
    if (header_.file_size != bytes_.size())
    {
        return AssetError("asset file size does not match header");
    }

    if (header_.payload_offset > bytes_.size() ||
        header_.payload_size > bytes_.size() - static_cast<std::size_t>(header_.payload_offset))
    {
        return AssetError("asset payload is outside the file");
    }
    return true;
}

/**
 * @brief TODO: Describe LoadFileBytes.
 *
 * @param path TODO: Describe this parameter.
 * @param out_bytes TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool LoadFileBytes(const std::filesystem::path &path, std::vector<std::uint8_t> &out_bytes)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        return AssetError("failed to open " + path.string());
    }

    const std::ifstream::pos_type end = file.tellg();
    if (end < 0)
    {
        return AssetError("failed to determine size for " + path.string());
    }

    out_bytes.resize(static_cast<std::size_t>(end));
    file.seekg(0, std::ios::beg);
    if (!out_bytes.empty())
    {
        file.read(reinterpret_cast<char *>(out_bytes.data()), static_cast<std::streamsize>(out_bytes.size()));
    }

    if (!file.good() && !file.eof())
    {
        return AssetError("failed while reading " + path.string());
    }
    return true;
}

} // namespace CEngine::Assets
