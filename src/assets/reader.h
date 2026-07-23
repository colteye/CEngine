//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/assets/reader.h
 * @brief Shared little-endian reader for cooked asset data.
 * @author Erik Coltey
 */

#ifndef CENGINE_ASSETS_READER_H
#define CENGINE_ASSETS_READER_H

#include "assets/format.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace CEngine::Assets
{

/**
 * A bounded cursor over immutable little-endian bytes. Generated wire readers
 * use this class so bounds checks and primitive decoding have one owner.
 */
class Reader
{
  public:
    explicit Reader(std::span<const std::byte> bytes) : bytes_(bytes)
    {
    }

    explicit Reader(std::span<const std::uint8_t> bytes)
        : bytes_(reinterpret_cast<const std::byte *>(bytes.data()), bytes.size())
    {
    }

    bool U8(std::uint8_t &value);
    bool U16(std::uint16_t &value);
    bool U32(std::uint32_t &value);
    bool U64(std::uint64_t &value);
    bool I32(std::int32_t &value);
    bool F32(float &value);
    bool Read(std::span<std::byte> output);
    bool Skip(std::size_t size);
    bool Seek(std::size_t offset);

    [[nodiscard]] std::span<const std::byte> Take(std::size_t size);
    [[nodiscard]] std::span<const std::byte> Slice(std::size_t offset, std::size_t size) const;
    [[nodiscard]] std::string_view String(std::size_t offset, std::size_t size) const;
    [[nodiscard]] std::size_t Offset() const
    {
        return offset_;
    }
    [[nodiscard]] std::size_t Size() const
    {
        return bytes_.size();
    }
    [[nodiscard]] std::size_t Remaining() const
    {
        return offset_ <= bytes_.size() ? bytes_.size() - offset_ : 0u;
    }
    [[nodiscard]] bool Done() const
    {
        return offset_ == bytes_.size();
    }

  private:
    std::span<const std::byte> bytes_;
    std::size_t offset_ = 0;
};

/**
 * Reads a complete file and commits the output only after the read succeeds.
 */
bool ReadFile(const std::filesystem::path &path, std::vector<std::byte> &output);

/**
 * Reads and validates the common envelope, returning an owned payload.
 */
bool ReadAsset(const std::filesystem::path &path, Header &header, std::vector<std::byte> &payload);

/**
 * Reads the common envelope and dispatches payload decoding through the
 * generated Read(span, T&) overload associated with T.
 */
template <typename T> bool Decode(const std::filesystem::path &path, Type type, T &output)
{
    Header header;
    std::vector<std::byte> payload;
    T decoded;
    if (!ReadAsset(path, header, payload) || header.type != type || !Read(payload, decoded))
    {
        return false;
    }
    output = std::move(decoded);
    return true;
}

} // namespace CEngine::Assets

#endif
