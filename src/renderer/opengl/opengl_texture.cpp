#include <nv_dds/nv_dds.h>

#include "opengl_texture.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace CEngine::Renderer {
namespace {
constexpr std::uint32_t MakeFourCc(char a, char b, char c, char d)
{
	return static_cast<std::uint32_t>(a) |
		(static_cast<std::uint32_t>(b) << 8u) |
		(static_cast<std::uint32_t>(c) << 16u) |
		(static_cast<std::uint32_t>(d) << 24u);
}

std::uint32_t ReadU32(const std::array<std::uint8_t, 128>& bytes, std::size_t offset)
{
	return static_cast<std::uint32_t>(bytes[offset]) |
		(static_cast<std::uint32_t>(bytes[offset + 1]) << 8u) |
		(static_cast<std::uint32_t>(bytes[offset + 2]) << 16u) |
		(static_cast<std::uint32_t>(bytes[offset + 3]) << 24u);
}

bool ValidateDds2D(const std::string& path, std::string& error)
{
	std::ifstream input(path, std::ios::binary);
	std::array<std::uint8_t, 128> header {};
	if (!input.read(reinterpret_cast<char*>(header.data()), header.size()))
	{
		error = "file is smaller than a DDS header";
		return false;
	}
	if (ReadU32(header, 0) != MakeFourCc('D', 'D', 'S', ' ') ||
		ReadU32(header, 4) != 124 || ReadU32(header, 76) != 32)
	{
		error = "header signature or size is invalid";
		return false;
	}

	const std::uint32_t height = ReadU32(header, 12);
	const std::uint32_t width = ReadU32(header, 16);
	const std::uint32_t depth = ReadU32(header, 24);
	const std::uint32_t declared_mips = ReadU32(header, 28);
	const std::uint32_t four_cc = ReadU32(header, 84);
	const std::uint32_t caps2 = ReadU32(header, 112);
	if (width == 0 || height == 0 || width > 16384 || height > 16384 || depth > 1 || caps2 != 0)
	{
		error = "only bounded 2D DDS textures are supported";
		return false;
	}
	const std::uint64_t block_size = four_cc == MakeFourCc('D', 'X', 'T', '1') ? 8u :
		(four_cc == MakeFourCc('D', 'X', 'T', '3') || four_cc == MakeFourCc('D', 'X', 'T', '5') ? 16u : 0u);
	if (block_size == 0)
	{
		error = "only DXT1, DXT3, and DXT5 DDS textures are supported";
		return false;
	}

	std::uint32_t max_mips = 1;
	for (std::uint32_t dimension = std::max(width, height); dimension > 1; dimension >>= 1u)
		++max_mips;
	const std::uint32_t mip_count = declared_mips == 0 ? 1 : declared_mips;
	if (mip_count > max_mips)
	{
		error = "DDS mip count exceeds its complete mip chain";
		return false;
	}

	std::uint64_t required_size = header.size();
	std::uint32_t mip_width = width;
	std::uint32_t mip_height = height;
	for (std::uint32_t mip = 0; mip < mip_count; ++mip)
	{
		required_size += static_cast<std::uint64_t>((mip_width + 3u) / 4u) *
			static_cast<std::uint64_t>((mip_height + 3u) / 4u) * block_size;
		mip_width = std::max(1u, mip_width >> 1u);
		mip_height = std::max(1u, mip_height >> 1u);
	}
	std::error_code filesystem_error;
	const std::uintmax_t file_size = std::filesystem::file_size(path, filesystem_error);
	if (filesystem_error || file_size < required_size)
	{
		error = "DDS payload is truncated";
		return false;
	}
	return true;
}
} // namespace

GLuint OpenGLTexture::LoadDDS(const std::string& image_p) {
	std::string validation_error;
	if (!ValidateDds2D(image_p, validation_error))
	{
		std::cerr << "Failed to load DDS texture '" << image_p << "': " << validation_error << '\n';
		return 0;
	}

	nv_dds::CDDSImage image;
	GLuint TextureID = 0;

	try
	{
		image.load(image_p);
	}
	catch (const std::exception& e)
	{
		std::cerr << "Failed to load DDS texture '" << image_p << "': " << e.what() << '\n';
		return 0;
	}
	if (!image.is_valid() || !image.is_compressed() || image.get_type() != nv_dds::TextureFlat ||
		image.get_width() == 0 || image.get_height() == 0 || image.get_size() == 0)
	{
		std::cerr << "Failed to load DDS texture '" << image_p << "': decoded surface is invalid\n";
		return 0;
	}

	while (glGetError() != GL_NO_ERROR) {}

	glGenTextures(1, &TextureID);
	glBindTexture(GL_TEXTURE_2D, TextureID);

	glCompressedTexImage2D(GL_TEXTURE_2D, 0, image.get_format(),
		image.get_width(), image.get_height(), 0, image.get_size(),
		image);

	for (unsigned int i = 0; i < image.get_num_mipmaps(); i++)
	{
		nv_dds::CSurface mipmap = image.get_mipmap(i);

			glCompressedTexImage2D(GL_TEXTURE_2D, i + 1, image.get_format(),
			mipmap.get_width(), mipmap.get_height(), 0, mipmap.get_size(),
			mipmap);
	}

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, image.get_num_mipmaps());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
		image.get_num_mipmaps() == 0 ? GL_LINEAR : GL_LINEAR_MIPMAP_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);
	if (glGetError() != GL_NO_ERROR)
	{
		glDeleteTextures(1, &TextureID);
		std::cerr << "Failed to upload DDS texture '" << image_p << "' to OpenGL\n";
		return 0;
	}
	return TextureID;
}

GLuint OpenGLTexture::CreateSolid(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha)
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

} // namespace CEngine::Renderer
