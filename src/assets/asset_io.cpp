#include "assets/asset_io.h"

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

void SetError(std::string* error, const std::string& message)
{
    if (error != nullptr)
    {
        *error = message;
    }
}

} // namespace

bool AssetFile::Load(const std::filesystem::path& path, std::string* error)
{
    bytes.clear();
    header = {};

    if (!LoadFileBytes(path, bytes, error))
    {
        return false;
    }
    return Validate(error);
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

bool AssetFile::Validate(std::string* error)
{
    if (bytes.size() < sizeof(DiskAssetHeader))
    {
        SetError(error, "asset is smaller than the common header");
        return false;
    }
    std::memcpy(&header, bytes.data(), sizeof(header));

    if (header.magic != AssetMagic)
    {
        SetError(error, "asset magic does not match CEngine format");
        return false;
    }
    if (header.version != AssetFormatVersion)
    {
        SetError(error, "asset format version is not supported");
        return false;
    }
    if (header.header_size != sizeof(DiskAssetHeader))
    {
        SetError(error, "asset header size is not supported");
        return false;
    }
    if (header.file_size != bytes.size())
    {
        SetError(error, "asset file size does not match header");
        return false;
    }

    if (header.payload_offset > bytes.size() ||
        header.payload_size > bytes.size() - static_cast<std::size_t>(header.payload_offset))
    {
        SetError(error, "asset payload is outside the file");
        return false;
    }
    return true;
}

bool LoadFileBytes(const std::filesystem::path& path, std::vector<std::uint8_t>& out_bytes,
    std::string* error)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        SetError(error, "failed to open " + path.string());
        return false;
    }

    const std::ifstream::pos_type end = file.tellg();
    if (end < 0)
    {
        SetError(error, "failed to determine size for " + path.string());
        return false;
    }

    out_bytes.resize(static_cast<std::size_t>(end));
    file.seekg(0, std::ios::beg);
    if (!out_bytes.empty())
    {
        file.read(reinterpret_cast<char*>(out_bytes.data()), static_cast<std::streamsize>(out_bytes.size()));
    }

    if (!file.good() && !file.eof())
    {
        SetError(error, "failed while reading " + path.string());
        return false;
    }
    return true;
}

bool WriteBinaryAsset(const std::filesystem::path& path, const AssetWriteDesc& desc, std::string* error)
{
    if (desc.type == AssetType::Unknown)
    {
        SetError(error, "asset type must be known before writing");
        return false;
    }
    if (desc.payload.empty())
    {
        SetError(error, "asset payload must not be empty");
        return false;
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
    std::memcpy(bytes.data(), &header, sizeof(header));
    bytes.insert(bytes.end(), desc.payload.begin(), desc.payload.end());

    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open())
    {
        SetError(error, "failed to open output " + path.string());
        return false;
    }
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!file.good())
    {
        SetError(error, "failed while writing " + path.string());
        return false;
    }
    return true;
}

bool HashFile(const std::filesystem::path& path, std::uint64_t& hash, std::string* error)
{
    std::vector<std::uint8_t> bytes;
    if (!LoadFileBytes(path, bytes, error))
    {
        return false;
    }
    hash = HashBytes(bytes.data(), bytes.size());
    return true;
}

} // namespace CEngine::Assets
