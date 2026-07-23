#ifndef CENGINE_RENDERER_TEXTURE_H
#define CENGINE_RENDERER_TEXTURE_H

#include <cstdint>
#include <vector>

namespace CEngine::Renderer
{

enum class TextureFormat : std::uint8_t
{
    Dxt1,
    Dxt3,
    Dxt5,
    Rgbe8,
};

struct TextureMip
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> data;
};

struct Texture
{
    TextureFormat format = TextureFormat::Dxt1;
    std::vector<TextureMip> mips;

    [[nodiscard]] bool Empty() const
    {
        return mips.empty();
    }
};

} // namespace CEngine::Renderer

#endif
