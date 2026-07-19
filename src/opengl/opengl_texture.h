#ifndef OPENGL_TEXTURE_H
#define OPENGL_TEXTURE_H

#include <string>
#include <glad/glad.h>

class OpenGLTexture
{
public:
	static GLuint LoadDDS(const std::string& imagepath);
};
#endif
