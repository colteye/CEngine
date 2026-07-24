//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/assets/reader.cpp
 * @brief Shared little-endian reader for cooked asset data.
 * @author Erik Coltey
 */

#include "assets/reader.h"

#include "log.h"

#include <algorithm>
#include <array>
#include <bit>
#include <fstream>
#include <limits>

namespace CEngine::Assets
{
namespace
{
bool CanRead(std::span<const std::byte> bytes, std::size_t offset, std::size_t size)
{
    return offset <= bytes.size() && size <= bytes.size() - offset;
}
} // namespace

bool Reader::U8(std::uint8_t &value)
{
    if (!CanRead(bytes_, offset_, 1u))
    {
        return false;
    }
    value = std::to_integer<std::uint8_t>(bytes_[offset_++]);
    return true;
}

bool Reader::U16(std::uint16_t &value)
{
    if (!CanRead(bytes_, offset_, 2u))
    {
        return false;
    }
    value = static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes_[offset_])) |
            static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes_[offset_ + 1u])) << 8u);
    offset_ += 2u;
    return true;
}

bool Reader::U32(std::uint32_t &value)
{
    if (!CanRead(bytes_, offset_, 4u))
    {
        return false;
    }
    value = static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes_[offset_])) |
            (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes_[offset_ + 1u])) << 8u) |
            (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes_[offset_ + 2u])) << 16u) |
            (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes_[offset_ + 3u])) << 24u);
    offset_ += 4u;
    return true;
}

bool Reader::U64(std::uint64_t &value)
{
    if (!CanRead(bytes_, offset_, 8u))
    {
        return false;
    }
    value = 0;
    for (std::uint32_t index = 0; index < 8u; ++index)
    {
        value |= static_cast<std::uint64_t>(std::to_integer<std::uint8_t>(bytes_[offset_ + index]))
                 << (index * 8u);
    }
    offset_ += 8u;
    return true;
}

bool Reader::I32(std::int32_t &value)
{
    std::uint32_t bits = 0;
    if (!U32(bits))
    {
        return false;
    }
    value = std::bit_cast<std::int32_t>(bits);
    return true;
}

bool Reader::F32(float &value)
{
    std::uint32_t bits = 0;
    if (!U32(bits))
    {
        return false;
    }
    value = std::bit_cast<float>(bits);
    return true;
}

bool Reader::Read(std::span<std::byte> output)
{
    if (!CanRead(bytes_, offset_, output.size()))
    {
        return false;
    }
    std::copy_n(bytes_.begin() + static_cast<std::ptrdiff_t>(offset_), output.size(), output.begin());
    offset_ += output.size();
    return true;
}

bool Reader::Skip(std::size_t size)
{
    if (!CanRead(bytes_, offset_, size))
    {
        return false;
    }
    offset_ += size;
    return true;
}

bool Reader::Seek(std::size_t offset)
{
    if (offset > bytes_.size())
    {
        return false;
    }
    offset_ = offset;
    return true;
}

std::span<const std::byte> Reader::Take(std::size_t size)
{
    const auto result = Slice(offset_, size);
    if (result.size() != size)
    {
        return {};
    }
    offset_ += size;
    return result;
}

std::span<const std::byte> Reader::Slice(std::size_t offset, std::size_t size) const
{
    if (!CanRead(bytes_, offset, size))
    {
        return {};
    }
    return bytes_.subspan(offset, size);
}

std::string_view Reader::String(std::size_t offset, std::size_t size) const
{
    const auto bytes = Slice(offset, size);
    if (bytes.size() != size)
    {
        return {};
    }
    return {reinterpret_cast<const char *>(bytes.data()), bytes.size()};
}

bool ReadFile(const std::filesystem::path &path, std::vector<std::byte> &output)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        Logging::Logger::Get().Error("assets", "failed to open " + path.string());
        return false;
    }

    const std::ifstream::pos_type end = file.tellg();
    if (end < 0 || static_cast<std::uintmax_t>(end) >
                       static_cast<std::uintmax_t>(std::numeric_limits<std::size_t>::max()))
    {
        Logging::Logger::Get().Error("assets", "failed to determine size for " + path.string());
        return false;
    }

    std::vector<std::byte> bytes(static_cast<std::size_t>(end));
    file.seekg(0, std::ios::beg);
    if (!bytes.empty())
    {
        file.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (file.gcount() != static_cast<std::streamsize>(bytes.size()))
        {
            Logging::Logger::Get().Error("assets", "failed while reading " + path.string());
            return false;
        }
    }
    output = std::move(bytes);
    return true;
}

bool ReadAsset(const std::filesystem::path &path, Header &header, std::vector<std::byte> &payload)
{
    std::vector<std::byte> bytes;
    if (!ReadFile(path, bytes))
    {
        return false;
    }

    Reader reader(bytes);
    std::array<std::byte, Magic.size()> magic{};
    std::uint16_t version = 0;
    std::uint16_t header_size = 0;
    std::uint32_t type = 0;
    Header result;
    std::array<char, PlatformSize> platform{};
    std::uint64_t payload_offset = 0;
    std::uint64_t payload_size = 0;
    std::uint64_t file_size = 0;
    if (!reader.Read(magic) || !reader.U16(version) || !reader.U16(header_size) || !reader.U32(type) ||
        !reader.Read(std::as_writable_bytes(std::span(result.guid))) || !reader.U64(result.source_hash) ||
        !reader.Read(std::as_writable_bytes(std::span(platform))) || !reader.U64(payload_offset) ||
        !reader.U64(payload_size) || !reader.U64(file_size))
    {
        Logging::Logger::Get().Error("assets", "asset common header is truncated");
        return false;
    }
    if (magic != Magic)
    {
        Logging::Logger::Get().Error("assets", "asset magic does not match CEngine format");
        return false;
    }
    if (version != Version || header_size != HeaderSize)
    {
        Logging::Logger::Get().Error("assets", "asset format version is not supported");
        return false;
    }
    if (file_size != bytes.size())
    {
        Logging::Logger::Get().Error("assets", "asset file size does not match header");
        return false;
    }
    if (type == 0u || type > static_cast<std::uint32_t>(Type::Particle))
    {
        Logging::Logger::Get().Error("assets", "asset type is invalid");
        return false;
    }
    if (payload_offset > bytes.size() || payload_size > bytes.size() - static_cast<std::size_t>(payload_offset))
    {
        Logging::Logger::Get().Error("assets", "asset payload is outside the file");
        return false;
    }

    result.type = static_cast<Type>(type);
    const auto terminator = std::find(platform.begin(), platform.end(), '\0');
    result.platform.assign(platform.begin(), terminator);
    const auto data = std::span(bytes).subspan(static_cast<std::size_t>(payload_offset),
                                               static_cast<std::size_t>(payload_size));
    std::vector<std::byte> result_payload(data.begin(), data.end());
    header = std::move(result);
    payload = std::move(result_payload);
    return true;
}

} // namespace CEngine::Assets
