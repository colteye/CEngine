#include <nv_dds/nv_dds.h>

#include <glm/gtc/packing.hpp>

#include "opengl_texture.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace CEngine::Renderer {
namespace {
constexpr std::uint32_t MakeFourCc(char a, char b, char c, char d) {
    return static_cast<std::uint32_t>(a) | (static_cast<std::uint32_t>(b) << 8u) |
           (static_cast<std::uint32_t>(c) << 16u) | (static_cast<std::uint32_t>(d) << 24u);
}

std::uint32_t ReadU32(const std::array<std::uint8_t, 128>& bytes, std::size_t offset) {
    return static_cast<std::uint32_t>(bytes[offset]) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 8u) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 16u) |
           (static_cast<std::uint32_t>(bytes[offset + 3]) << 24u);
}

enum class DdsStorage {
    BlockCompressed,
    RgbExp32,
};

struct DdsInfo {
    DdsStorage storage = DdsStorage::BlockCompressed;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t mip_count = 1;
    std::uint64_t payload_offset = 128;
    std::uint64_t payload_size = 0;
};

bool ValidateDds2D(const std::string& path, DdsInfo& info, std::string& error) {
    std::ifstream input(path, std::ios::binary);
    std::array<std::uint8_t, 128> header{};
    if (!input.read(reinterpret_cast<char*>(header.data()), header.size())) {
        error = "file is smaller than a DDS header";
        return false;
    }
    if (ReadU32(header, 0) != MakeFourCc('D', 'D', 'S', ' ') || ReadU32(header, 4) != 124 ||
        ReadU32(header, 76) != 32) {
        error = "header signature or size is invalid";
        return false;
    }

    const std::uint32_t height = ReadU32(header, 12);
    const std::uint32_t width = ReadU32(header, 16);
    const std::uint32_t depth = ReadU32(header, 24);
    const std::uint32_t declared_mips = ReadU32(header, 28);
    const std::uint32_t four_cc = ReadU32(header, 84);
    const std::uint32_t caps2 = ReadU32(header, 112);
    if (width == 0 || height == 0 || width > 16384 || height > 16384 || depth > 1 || caps2 != 0) {
        error = "only bounded 2D DDS textures are supported";
        return false;
    }
    std::uint32_t max_mips = 1;
    for (std::uint32_t dimension = std::max(width, height); dimension > 1; dimension >>= 1u)
        ++max_mips;
    const std::uint32_t mip_count = declared_mips == 0 ? 1 : declared_mips;
    if (mip_count > max_mips) {
        error = "DDS mip count exceeds its complete mip chain";
        return false;
    }

    info.width = width;
    info.height = height;
    info.mip_count = mip_count;

    const std::uint64_t block_size = four_cc == MakeFourCc('D', 'X', 'T', '1')
                                         ? 8u
                                         : (four_cc == MakeFourCc('D', 'X', 'T', '3') ||
                                                    four_cc == MakeFourCc('D', 'X', 'T', '5')
                                                ? 16u
                                                : 0u);
    std::uint64_t required_size = header.size();
    if (four_cc == MakeFourCc('R', 'G', 'B', 'E') && mip_count == 1u) {
        info.storage = DdsStorage::RgbExp32;
        info.payload_offset = header.size();
        info.payload_size = static_cast<std::uint64_t>(width) * height * 4u;
        required_size += info.payload_size;
    } else if (block_size != 0) {
        info.storage = DdsStorage::BlockCompressed;
        info.payload_offset = header.size();
        std::uint32_t mip_width = width;
        std::uint32_t mip_height = height;
        for (std::uint32_t mip = 0; mip < mip_count; ++mip) {
            info.payload_size += static_cast<std::uint64_t>((mip_width + 3u) / 4u) *
                                 static_cast<std::uint64_t>((mip_height + 3u) / 4u) * block_size;
            mip_width = std::max(1u, mip_width >> 1u);
            mip_height = std::max(1u, mip_height >> 1u);
        }
        required_size += info.payload_size;
    } else {
        error = "only DXT1, DXT3, DXT5, and single-level RGBExp32 DDS textures are supported";
        return false;
    }
    std::error_code filesystem_error;
    const std::uintmax_t file_size = std::filesystem::file_size(path, filesystem_error);
    if (filesystem_error || file_size < required_size) {
        error = "DDS payload is truncated";
        return false;
    }
    return true;
}

void ConfigureAnisotropicFiltering()
{
	if (GLAD_GL_ARB_texture_filter_anisotropic == 0 &&
		GLAD_GL_EXT_texture_filter_anisotropic == 0)
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
		++levels;
	return levels;
}
} // namespace

GLuint OpenGLTexture::LoadDDS(const std::string& image_p, bool flip_image) {
    std::string validation_error;
    DdsInfo info;
    if (!ValidateDds2D(image_p, info, validation_error)) {
        std::cerr << "Failed to load DDS texture '" << image_p << "': " << validation_error << '\n';
        return 0;
    }

    GLuint TextureID = 0;
    if (info.storage == DdsStorage::RgbExp32) {
        std::ifstream input(image_p, std::ios::binary);
        input.seekg(static_cast<std::streamoff>(info.payload_offset));
        std::vector<std::uint8_t> encoded(static_cast<std::size_t>(info.payload_size));
        if (!input.read(reinterpret_cast<char*>(encoded.data()),
                        static_cast<std::streamsize>(info.payload_size))) {
            std::cerr << "Failed to load DDS texture '" << image_p << "': payload read failed\n";
            return 0;
        }

        // Decode before upload so texture filtering happens in linear HDR
        // space, not between mantissas with potentially different exponents.
        std::vector<std::uint16_t> pixels(encoded.size());
        constexpr float kMaxFiniteHalf = 65504.0f;
        for (std::size_t index = 0; index < encoded.size(); index += 4u) {
            const int exponent = static_cast<int>(encoded[index + 3]) - 128;
            const float multiplier = std::ldexp(1.0f, exponent) / 255.0f;
            const auto finite_half = [&](std::uint8_t mantissa) {
                return glm::packHalf1x16(std::min(
                    static_cast<float>(mantissa) * multiplier, kMaxFiniteHalf));
            };
            // A single over-range sun pixel becomes +Inf in RGBA16F and then
            // contaminates the irradiance and specular convolutions with NaNs.
            pixels[index] = finite_half(encoded[index]);
            pixels[index + 1] = finite_half(encoded[index + 1]);
            pixels[index + 2] = finite_half(encoded[index + 2]);
            pixels[index + 3] = glm::packHalf1x16(1.0f);
        }
        if (flip_image) {
            const std::size_t row_values = static_cast<std::size_t>(info.width) * 4u;
            for (std::uint32_t y = 0; y < info.height / 2u; ++y) {
                auto first = pixels.begin() + static_cast<std::ptrdiff_t>(y * row_values);
                auto second = pixels.begin() +
                              static_cast<std::ptrdiff_t>((info.height - 1u - y) * row_values);
                std::swap_ranges(first, first + static_cast<std::ptrdiff_t>(row_values), second);
            }
        }

        while (glGetError() != GL_NO_ERROR) {
        }
        glGenTextures(1, &TextureID);
        glBindTexture(GL_TEXTURE_2D, TextureID);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, static_cast<GLsizei>(info.width),
                     static_cast<GLsizei>(info.height), 0, GL_RGBA, GL_HALF_FLOAT, pixels.data());
		glGenerateMipmap(GL_TEXTURE_2D);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, FullMipLevelCount(info.width, info.height));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		ConfigureAnisotropicFiltering();
        glBindTexture(GL_TEXTURE_2D, 0);
        if (glGetError() != GL_NO_ERROR) {
            glDeleteTextures(1, &TextureID);
            std::cerr << "Failed to upload DDS texture '" << image_p << "'\n";
            return 0;
        }
        return TextureID;
    }

    nv_dds::CDDSImage image;

    try {
        image.load(image_p, flip_image);
    } catch (const std::exception& e) {
        std::cerr << "Failed to load DDS texture '" << image_p << "': " << e.what() << '\n';
        return 0;
    }
    if (!image.is_valid() || !image.is_compressed() || image.get_type() != nv_dds::TextureFlat ||
        image.get_width() == 0 || image.get_height() == 0 || image.get_size() == 0) {
        std::cerr << "Failed to load DDS texture '" << image_p << "': decoded surface is invalid\n";
        return 0;
    }

    while (glGetError() != GL_NO_ERROR) {
    }

    glGenTextures(1, &TextureID);
    glBindTexture(GL_TEXTURE_2D, TextureID);

    glCompressedTexImage2D(GL_TEXTURE_2D, 0, image.get_format(), image.get_width(),
                           image.get_height(), 0, image.get_size(), image);

    for (unsigned int i = 0; i < image.get_num_mipmaps(); i++) {
        nv_dds::CSurface mipmap = image.get_mipmap(i);

        glCompressedTexImage2D(GL_TEXTURE_2D, i + 1, image.get_format(), mipmap.get_width(),
                               mipmap.get_height(), 0, mipmap.get_size(), mipmap);
    }

	const unsigned int source_mipmaps = image.get_num_mipmaps();
	if (source_mipmaps == 0)
	{
		// Most cooked source textures carry only their base DXT level. Generate
		// a full chain once on the GPU to prevent minification moiré.
		glGenerateMipmap(GL_TEXTURE_2D);
	}
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL,
		source_mipmaps == 0 ? FullMipLevelCount(image.get_width(), image.get_height()) : source_mipmaps);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
					GL_LINEAR_MIPMAP_LINEAR);
	ConfigureAnisotropicFiltering();
    glBindTexture(GL_TEXTURE_2D, 0);
    if (glGetError() != GL_NO_ERROR) {
        glDeleteTextures(1, &TextureID);
        std::cerr << "Failed to upload DDS texture '" << image_p << "' to OpenGL\n";
        return 0;
    }
    return TextureID;
}

GLuint OpenGLTexture::CreateSolid(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha) {
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

} // namespace CEngine::Renderer
