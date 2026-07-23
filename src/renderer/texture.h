//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/renderer/texture.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#ifndef CENGINE_RENDERER_TEXTURE_H
#define CENGINE_RENDERER_TEXTURE_H

#include <cstdint>
#include <vector>

namespace CEngine::Renderer
{

/**
 * @brief TODO: Describe TextureFormat.
 */
enum class TextureFormat : std::uint8_t
{
    Dxt1,
    Dxt3,
    Dxt5,
    Rgbe8,
};

/**
 * @brief TODO: Describe TextureMip.
 */
struct TextureMip
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> data;
};

/**
 * @brief TODO: Describe Texture.
 */
struct Texture
{
    TextureFormat format = TextureFormat::Dxt1;
    std::vector<TextureMip> mips;

    /**
     * @brief TODO: Describe Empty.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] bool Empty() const
    {
        return mips.empty();
    }
};

} // namespace CEngine::Renderer

#endif
