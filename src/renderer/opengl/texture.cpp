#include "renderer/opengl/texture.h"

#include "logging/logger.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/gtc/packing.hpp>

namespace CEngine::Renderer::OpenGL
{
namespace
{

void ConfigureAnisotropicFiltering()
{
    if (GLAD_GL_ARB_texture_filter_anisotropic == 0 && GLAD_GL_EXT_texture_filter_anisotropic == 0)
    {
        return;
    }

    GLfloat maximum = 1.0f;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maximum);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, std::min(maximum, 8.0f));
}

GLint FullMipLevelCount(std::uint32_t width, std::uint32_t height)
{
    GLint levels = 0;
    for (std::uint32_t dimension = std::max(width, height); dimension > 1; dimension >>= 1u)
    {
        ++levels;
    }
    return levels;
}

GLenum CompressedFormat(TextureFormat format)
{
    switch (format)
    {
    case TextureFormat::Dxt1:
        return GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
    case TextureFormat::Dxt3:
        return GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
    case TextureFormat::Dxt5:
        return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
    case TextureFormat::Rgbe8:
        return 0;
    }
    return 0;
}

GLuint UploadRgbe(const TextureMip &mip)
{
    std::vector<std::uint16_t> pixels(mip.data.size());
    static const std::array<std::uint16_t, static_cast<size_t>(256u * 256u)> LinearHalf = [] {
        constexpr float MaxFiniteHalf = 65504.0f;
        std::array<std::uint16_t, static_cast<size_t>(256u * 256u)> values{};
        for (std::size_t exponent_byte = 0; exponent_byte < 256u; ++exponent_byte)
        {
            const int exponent = static_cast<int>(exponent_byte) - 128;
            const float multiplier = std::ldexp(1.0f, exponent) / 255.0f;
            for (std::size_t mantissa = 0; mantissa < 256u; ++mantissa)
            {
                values[(exponent_byte * 256u) + mantissa] =
                    glm::packHalf1x16(std::min(static_cast<float>(mantissa) * multiplier, MaxFiniteHalf));
            }
        }
        return values;
    }();
    const std::uint16_t opaque = glm::packHalf1x16(1.0f);
    for (std::size_t index = 0; index < mip.data.size(); index += 4u)
    {
        const std::size_t row = static_cast<std::size_t>(mip.data[index + 3]) * 256u;
        pixels[index] = LinearHalf[row + mip.data[index]];
        pixels[index + 1] = LinearHalf[row + mip.data[index + 1]];
        pixels[index + 2] = LinearHalf[row + mip.data[index + 2]];
        pixels[index + 3] = opaque;
    }

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, static_cast<GLsizei>(mip.width), static_cast<GLsizei>(mip.height), 0,
                 GL_RGBA, GL_HALF_FLOAT, pixels.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, FullMipLevelCount(mip.width, mip.height));
    return texture;
}

} // namespace

GLuint TextureLoader::Load(const Texture &source)
{
    if (source.Empty())
    {
        return 0;
    }

    while (glGetError() != GL_NO_ERROR)
    {
    }

    GLuint texture = 0;
    if (source.format == TextureFormat::Rgbe8)
    {
        if (source.mips.size() != 1 ||
            source.mips.front().data.size() !=
                static_cast<std::size_t>(source.mips.front().width) * source.mips.front().height * 4u)
        {
            Logging::Logger::Get().Error("renderer", "RGBExp32 texture data is invalid");
            return 0;
        }
        texture = UploadRgbe(source.mips.front());
    }
    else
    {
        const GLenum format = CompressedFormat(source.format);
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        for (std::size_t level = 0; level < source.mips.size(); ++level)
        {
            const TextureMip &mip = source.mips[level];
            glCompressedTexImage2D(GL_TEXTURE_2D, static_cast<GLint>(level), format, static_cast<GLsizei>(mip.width),
                                   static_cast<GLsizei>(mip.height), 0, static_cast<GLsizei>(mip.data.size()),
                                   mip.data.data());
        }
        if (source.mips.size() == 1)
        {
            glGenerateMipmap(GL_TEXTURE_2D);
        }
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL,
                        source.mips.size() == 1
                            ? FullMipLevelCount(source.mips.front().width, source.mips.front().height)
                            : static_cast<GLint>(source.mips.size() - 1));
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    ConfigureAnisotropicFiltering();
    glBindTexture(GL_TEXTURE_2D, 0);
    if (glGetError() != GL_NO_ERROR)
    {
        glDeleteTextures(1, &texture);
        Logging::Logger::Get().Error("renderer", "texture upload failed");
        return 0;
    }
    return texture;
}

GLuint TextureLoader::CreateSolid(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha)
{
    const GLubyte pixel[4] = {red, green, blue, alpha};
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    return texture;
}

} // namespace CEngine::Renderer::OpenGL
