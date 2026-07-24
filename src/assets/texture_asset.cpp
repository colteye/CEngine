//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/assets/texture_asset.cpp
 * @brief Loads a DDS texture asset into its renderer value.
 * @author Erik Coltey
 */

#include "assets/texture_asset.h"

#include "assets/reader.h"
#include "log.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace CEngine::Assets
{
namespace
{

/**
 * @brief TODO: Describe MakeFourCc.
 *
 * @param a TODO: Describe this parameter.
 * @param b TODO: Describe this parameter.
 * @param c TODO: Describe this parameter.
 * @param d TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
constexpr std::uint32_t MakeFourCc(char a, char b, char c, char d)
{
    return static_cast<std::uint32_t>(a) | (static_cast<std::uint32_t>(b) << 8u) |
           (static_cast<std::uint32_t>(c) << 16u) | (static_cast<std::uint32_t>(d) << 24u);
}

constexpr std::uint32_t DdsMagic = MakeFourCc('D', 'D', 'S', ' ');
constexpr std::uint32_t Dxt1 = MakeFourCc('D', 'X', 'T', '1');
constexpr std::uint32_t Dxt3 = MakeFourCc('D', 'X', 'T', '3');
constexpr std::uint32_t Dxt5 = MakeFourCc('D', 'X', 'T', '5');
constexpr std::uint32_t Rgbe = MakeFourCc('R', 'G', 'B', 'E');
constexpr std::size_t DdsHeaderSize = 128;

/**
 * @brief TODO: Describe ReadU32At.
 *
 * @param bytes TODO: Describe this parameter.
 * @param offset TODO: Describe this parameter.
 * @param value TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool ReadU32At(std::span<const std::byte> bytes, std::size_t offset, std::uint32_t &value)
{
    Reader reader(bytes.subspan(offset));
    return reader.U32(value);
}

/**
 * @brief TODO: Describe ReverseBytes.
 *
 * @param bytes TODO: Describe this parameter.
 * @param count TODO: Describe this parameter.
 */
void ReverseBytes(std::uint8_t *bytes, std::uint32_t count)
{
    for (std::uint32_t first = 0, last = count - 1; first < last; ++first, --last)
    {
        std::swap(bytes[first], bytes[last]);
    }
}

/**
 * @brief TODO: Describe ReversePairs.
 *
 * @param bytes TODO: Describe this parameter.
 * @param count TODO: Describe this parameter.
 */
void ReversePairs(std::uint8_t *bytes, std::uint32_t count)
{
    for (std::uint32_t first = 0, last = count - 1; first < last; ++first, --last)
    {
        std::swap(bytes[static_cast<size_t>(first * 2)], bytes[static_cast<size_t>(last * 2)]);
        std::swap(bytes[(first * 2) + 1], bytes[(last * 2) + 1]);
    }
}

/**
 * @brief TODO: Describe FlipDxt5Alpha.
 *
 * @param block TODO: Describe this parameter.
 * @param row_count TODO: Describe this parameter.
 */
void FlipDxt5Alpha(std::uint8_t *block, std::uint32_t row_count)
{
    std::array<std::uint8_t, 16> indices{};
    std::uint64_t packed = 0;
    for (std::uint32_t byte = 0; byte < 6; ++byte)
    {
        packed |= static_cast<std::uint64_t>(block[2 + byte]) << (byte * 8u);
    }
    for (std::uint32_t index = 0; index < indices.size(); ++index)
    {
        indices[index] = static_cast<std::uint8_t>((packed >> (index * 3u)) & 7u);
    }

    for (std::uint32_t first = 0, last = row_count - 1; first < last; ++first, --last)
    {
        for (std::uint32_t column = 0; column < 4; ++column)
        {
            std::swap(indices[(first * 4) + column], indices[(last * 4) + column]);
        }
    }

    packed = 0;
    for (std::uint32_t index = 0; index < indices.size(); ++index)
    {
        packed |= static_cast<std::uint64_t>(indices[index]) << (index * 3u);
    }
    for (std::uint32_t byte = 0; byte < 6; ++byte)
    {
        block[2 + byte] = static_cast<std::uint8_t>(packed >> (byte * 8u));
    }
}

/**
 * @brief TODO: Describe FlipBlock.
 *
 * @param block TODO: Describe this parameter.
 * @param format TODO: Describe this parameter.
 * @param row_count TODO: Describe this parameter.
 */
void FlipBlock(std::uint8_t *block, Renderer::TextureFormat format, std::uint32_t row_count)
{
    if (format == Renderer::TextureFormat::Dxt1)
    {
        ReverseBytes(block + 4, row_count);
        return;
    }

    if (format == Renderer::TextureFormat::Dxt3)
    {
        ReversePairs(block, row_count);
    }
    else
    {
        FlipDxt5Alpha(block, row_count);
    }
    ReverseBytes(block + 12, row_count);
}

/**
 * @brief TODO: Describe FlipDxt.
 *
 * @param mip TODO: Describe this parameter.
 * @param format TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool FlipDxt(Renderer::TextureMip &mip, Renderer::TextureFormat format)
{
    if (mip.height > 4 && mip.height % 4 != 0)
    {
        Logging::Logger::Get().Error("assets", "vertically flipped DDS height must be a multiple of four");
        return false;
    }

    const std::uint32_t block_size = format == Renderer::TextureFormat::Dxt1 ? 8u : 16u;
    const std::uint32_t blocks_wide = (mip.width + 3u) / 4u;
    const std::uint32_t blocks_high = (mip.height + 3u) / 4u;
    const std::size_t row_size = static_cast<std::size_t>(blocks_wide) * block_size;

    for (std::uint32_t block_y = 0; block_y < blocks_high; ++block_y)
    {
        const std::uint32_t rows = std::min(4u, mip.height - (block_y * 4u));
        for (std::uint32_t block_x = 0; block_x < blocks_wide; ++block_x)
        {
            FlipBlock(mip.data.data() + (block_y * row_size) + (static_cast<std::size_t>(block_x) * block_size), format,
                      rows);
        }
    }

    std::vector<std::uint8_t> row(row_size);
    for (std::uint32_t top = 0, bottom = blocks_high - 1; top < bottom; ++top, --bottom)
    {
        std::uint8_t *top_data = mip.data.data() + (top * row_size);
        std::uint8_t *bottom_data = mip.data.data() + (bottom * row_size);
        std::copy_n(top_data, row_size, row.data());
        std::copy_n(bottom_data, row_size, top_data);
        std::copy_n(row.data(), row_size, bottom_data);
    }
    return true;
}

/**
 * @brief TODO: Describe FlipRgbe.
 *
 * @param mip TODO: Describe this parameter.
 */
void FlipRgbe(Renderer::TextureMip &mip)
{
    const std::size_t row_size = static_cast<std::size_t>(mip.width) * 4u;
    std::vector<std::uint8_t> row(row_size);
    for (std::uint32_t top = 0, bottom = mip.height - 1; top < bottom; ++top, --bottom)
    {
        std::uint8_t *top_data = mip.data.data() + (top * row_size);
        std::uint8_t *bottom_data = mip.data.data() + (bottom * row_size);
        std::copy_n(top_data, row_size, row.data());
        std::copy_n(bottom_data, row_size, top_data);
        std::copy_n(row.data(), row_size, bottom_data);
    }
}

} // namespace

/**
 * @brief TODO: Describe LoadTextureAsset.
 *
 * @param path TODO: Describe this parameter.
 * @param texture TODO: Describe this parameter.
 * @param flip_vertical TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool LoadTextureAsset(const std::filesystem::path &path, Renderer::Texture &texture, bool flip_vertical)
{
    std::vector<std::byte> bytes;
    if (!ReadFile(path, bytes))
    {
        return false;
    }
    if (bytes.size() < DdsHeaderSize)
    {
        Logging::Logger::Get().Error("assets", "texture is smaller than a DDS header");
        return false;
    }

    const std::span<const std::byte> view = bytes;
    Reader reader(view);
    std::uint32_t magic = 0;
    std::uint32_t header_size = 0;
    std::uint32_t pixel_header_size = 0;
    if (!reader.U32(magic) || !reader.U32(header_size) ||
        !ReadU32At(view, 76, pixel_header_size) || magic != DdsMagic || header_size != 124 || pixel_header_size != 32)
    {
        Logging::Logger::Get().Error("assets", "DDS header signature or size is invalid");
        return false;
    }

    std::uint32_t height = 0;
    std::uint32_t width = 0;
    std::uint32_t depth = 0;
    std::uint32_t declared_mips = 0;
    std::uint32_t four_cc = 0;
    std::uint32_t caps2 = 0;
    if (!ReadU32At(view, 12, height) || !ReadU32At(view, 16, width) || !ReadU32At(view, 24, depth) ||
        !ReadU32At(view, 28, declared_mips) || !ReadU32At(view, 84, four_cc) || !ReadU32At(view, 112, caps2))
    {
        Logging::Logger::Get().Error("assets", "DDS header is truncated");
        return false;
    }
    if (width == 0 || height == 0 || width > 16384 || height > 16384 || depth > 1 || caps2 != 0)
    {
        Logging::Logger::Get().Error("assets", "only bounded 2D DDS textures are supported");
        return false;
    }

    std::uint32_t max_mips = 1;
    for (std::uint32_t dimension = std::max(width, height); dimension > 1; dimension >>= 1u)
    {
        ++max_mips;
    }
    const std::uint32_t mip_count = declared_mips == 0 ? 1 : declared_mips;
    if (mip_count > max_mips)
    {
        Logging::Logger::Get().Error("assets", "DDS mip count exceeds its complete mip chain");
        return false;
    }

    Renderer::Texture loaded;
    std::uint32_t block_size = 0;
    if (four_cc == Dxt1)
    {
        loaded.format = Renderer::TextureFormat::Dxt1;
        block_size = 8;
    }
    else if (four_cc == Dxt3)
    {
        loaded.format = Renderer::TextureFormat::Dxt3;
        block_size = 16;
    }
    else if (four_cc == Dxt5)
    {
        loaded.format = Renderer::TextureFormat::Dxt5;
        block_size = 16;
    }
    else if (four_cc == Rgbe)
    {
        loaded.format = Renderer::TextureFormat::Rgbe8;
    }
    else
    {
        Logging::Logger::Get().Error(
            "assets", "only DXT1, DXT3, DXT5, and RGBExp32 DDS textures are supported");
        return false;
    }

    std::size_t payload_offset = DdsHeaderSize;
    std::uint32_t mip_width = width;
    std::uint32_t mip_height = height;
    loaded.mips.reserve(mip_count);
    for (std::uint32_t mip_index = 0; mip_index < mip_count; ++mip_index)
    {
        const std::uint64_t data_size =
            block_size == 0 ? static_cast<std::uint64_t>(mip_width) * mip_height * 4u
                            : static_cast<std::uint64_t>((mip_width + 3u) / 4u) * ((mip_height + 3u) / 4u) * block_size;
        if (data_size > bytes.size() - std::min(payload_offset, bytes.size()))
        {
            Logging::Logger::Get().Error("assets", "DDS payload is truncated");
            return false;
        }

        Renderer::TextureMip mip;
        mip.width = mip_width;
        mip.height = mip_height;
        const auto source = view.subspan(payload_offset, static_cast<std::size_t>(data_size));
        mip.data.resize(source.size());
        std::transform(source.begin(), source.end(), mip.data.begin(),
                       [](std::byte value) { return std::to_integer<std::uint8_t>(value); });
        if (flip_vertical)
        {
            if (loaded.format == Renderer::TextureFormat::Rgbe8)
            {
                FlipRgbe(mip);
            }
            else if (!FlipDxt(mip, loaded.format))
            {
                return false;
            }
        }
        loaded.mips.push_back(std::move(mip));
        payload_offset += static_cast<std::size_t>(data_size);
        mip_width = std::max(1u, mip_width >> 1u);
        mip_height = std::max(1u, mip_height >> 1u);
    }

    texture = std::move(loaded);
    return true;
}

} // namespace CEngine::Assets
