#ifndef OPENGL_TEXTURE_H
#define OPENGL_TEXTURE_H

#include <string>
#include <glad/glad.h>

namespace CEngine::Renderer {

class OpenGLTexture
{
public:
	static GLuint LoadDDS(const std::string& imagepath);
	static GLuint CreateSolid(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha = 255);
};

} // namespace CEngine::Renderer
#endif
