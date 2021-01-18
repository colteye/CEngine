#include <nv_dds/nv_dds.h>

#include "Texture.h"

GLuint Texture::LoadDDS(std::string image_p) {

	nv_dds::CDDSImage image;
	GLuint TextureID;

	image.load(image_p);

	glGenTextures(1, &TextureID);
	glBindTexture(GL_TEXTURE_2D, TextureID);

	glCompressedTexImage2DARB(GL_TEXTURE_2D, 0, image.get_format(),
		image.get_width(), image.get_height(), 0, image.get_size(),
		image);

	for (unsigned int i = 0; i < image.get_num_mipmaps(); i++)
	{
		nv_dds::CSurface mipmap = image.get_mipmap(i);

		glCompressedTexImage2DARB(GL_TEXTURE_2D, i + 1, image.get_format(),
			mipmap.get_width(), mipmap.get_height(), 0, mipmap.get_size(),
			mipmap);
	}

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, image.get_num_mipmaps());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	return TextureID;
}