#include "assets/asset_io.h"

#include "assets/asset_error.h"
#include "assets/binary.h"

#include <algorithm>
#include <cstring>
#include <fstream>

namespace CEngine::Assets {
namespace {

constexpr std::uint64_t PayloadAlignment = 16;

std::uint64_t AlignUp(std::uint64_t value, std::uint64_t alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

void WriteU16LE(
    std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint16_t value)
{
    bytes[offset] = static_cast<std::uint8_t>(value);
    bytes[offset + 1u] = static_cast<std::uint8_t>(value >> 8u);
}

void WriteU32LE(
    std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value)
{
    for (std::uint32_t byte = 0; byte < 4u; ++byte)
        bytes[offset + byte] =
            static_cast<std::uint8_t>(value >> (byte * 8u));
}

void WriteU64LE(
    std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint64_t value)
{
    for (std::uint32_t byte = 0; byte < 8u; ++byte)
        bytes[offset + byte] =
            static_cast<std::uint8_t>(value >> (byte * 8u));
}

} // namespace

bool AssetFile::Load(const std::filesystem::path& path)
{
    bytes.clear();
    header = {};

    if (!LoadFileBytes(path, bytes))
    {
        return false;
    }
    return Validate();
}

std::string_view AssetFile::PlatformTarget() const
{
    const char* begin = header.platform_target.data();
    const char* end = std::find(begin, begin + header.platform_target.size(), '\0');
    return std::string_view(begin, static_cast<std::size_t>(end - begin));
}

ByteView AssetFile::Payload() const
{
    if (header.payload_offset > bytes.size() ||
        header.payload_size > bytes.size() - static_cast<std::size_t>(header.payload_offset))
    {
        return {};
    }
    return {bytes.data() + header.payload_offset, static_cast<std::size_t>(header.payload_size)};
}

bool AssetFile::Validate()
{
    if (bytes.size() < sizeof(DiskAssetHeader))
    {
        return AssetError("asset is smaller than the common header");
    }
    const ByteView view{bytes.data(), bytes.size()};
    std::size_t offset = 0;
    std::memcpy(header.magic.data(), bytes.data(), header.magic.size());
    offset += header.magic.size();
    if (!ReadU16LE(view, offset, header.version) ||
        !ReadU16LE(view, offset, header.header_size) ||
        !ReadU32LE(view, offset, header.asset_type))
    {
        return AssetError("asset common header is truncated");
    }
    std::memcpy(header.guid.data(), bytes.data() + offset, header.guid.size());
    offset += header.guid.size();
    if (!ReadU64LE(view, offset, header.source_hash))
    {
        return AssetError("asset common header is truncated");
    }
    std::memcpy(header.platform_target.data(), bytes.data() + offset,
        header.platform_target.size());
    offset += header.platform_target.size();
    if (!ReadU64LE(view, offset, header.payload_offset) ||
        !ReadU64LE(view, offset, header.payload_size) ||
        !ReadU64LE(view, offset, header.file_size))
    {
        return AssetError("asset common header is truncated");
    }

    if (header.magic != AssetMagic)
    {
        return AssetError("asset magic does not match CEngine format");
    }
    if (header.version != AssetFormatVersion)
    {
        return AssetError("asset format version is not supported");
    }
    if (header.header_size != sizeof(DiskAssetHeader))
    {
        return AssetError("asset header size is not supported");
    }
    if (header.file_size != bytes.size())
    {
        return AssetError("asset file size does not match header");
    }

    if (header.payload_offset > bytes.size() ||
        header.payload_size > bytes.size() - static_cast<std::size_t>(header.payload_offset))
    {
        return AssetError("asset payload is outside the file");
    }
    return true;
}

bool LoadFileBytes(const std::filesystem::path& path,
    std::vector<std::uint8_t>& out_bytes)
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
        file.read(reinterpret_cast<char*>(out_bytes.data()), static_cast<std::streamsize>(out_bytes.size()));
    }

    if (!file.good() && !file.eof())
    {
        return AssetError("failed while reading " + path.string());
    }
    return true;
}

bool WriteBinaryAsset(
    const std::filesystem::path& path, const AssetWriteDesc& desc)
{
    if (desc.type == AssetType::Unknown)
    {
        return AssetError("asset type must be known before writing");
    }
    if (desc.payload.empty())
    {
        return AssetError("asset payload must not be empty");
    }

    DiskAssetHeader header;
    header.header_size = sizeof(DiskAssetHeader);
    header.asset_type = static_cast<std::uint32_t>(desc.type);
    header.guid = desc.guid;
    header.source_hash = desc.source_hash;
    const std::size_t platform_size =
        std::min(desc.platform_target.size(), header.platform_target.size() - 1);
    std::copy_n(desc.platform_target.begin(), platform_size, header.platform_target.begin());
    header.payload_offset = AlignUp(sizeof(DiskAssetHeader), PayloadAlignment);
    header.payload_size = desc.payload.size();
    header.file_size = header.payload_offset + header.payload_size;

    std::vector<std::uint8_t> bytes;
    bytes.resize(static_cast<std::size_t>(header.payload_offset));
    std::copy(header.magic.begin(), header.magic.end(), bytes.begin());
    WriteU16LE(bytes, 4u, header.version);
    WriteU16LE(bytes, 6u, header.header_size);
    WriteU32LE(bytes, 8u, header.asset_type);
    std::copy(header.guid.begin(), header.guid.end(), bytes.begin() + 12u);
    WriteU64LE(bytes, 28u, header.source_hash);
    std::copy(header.platform_target.begin(), header.platform_target.end(),
        bytes.begin() + 36u);
    WriteU64LE(bytes, 52u, header.payload_offset);
    WriteU64LE(bytes, 60u, header.payload_size);
    WriteU64LE(bytes, 68u, header.file_size);
    bytes.insert(bytes.end(), desc.payload.begin(), desc.payload.end());

    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open())
    {
        return AssetError("failed to open output " + path.string());
    }
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!file.good())
    {
        return AssetError("failed while writing " + path.string());
    }
    return true;
}

bool HashFile(const std::filesystem::path& path, std::uint64_t& hash)
{
    std::vector<std::uint8_t> bytes;
    if (!LoadFileBytes(path, bytes))
    {
        return false;
    }
    hash = HashBytes(bytes.data(), bytes.size());
    return true;
}

} // namespace CEngine::Assets
