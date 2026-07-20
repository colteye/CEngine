#ifndef OPENGL_TEXTURE_H
#define OPENGL_TEXTURE_H

#include <string>
#include <glad/glad.h>

namespace CEngine::Renderer {

class OpenGLTexture
{
public:
	static GLuint LoadDDS(const std::string& imagepath);
};

} // namespace CEngine::Renderer
#endif
