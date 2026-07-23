#ifndef CENGINE_RENDERER_OPENGL_TEXTURE_H
#define CENGINE_RENDERER_OPENGL_TEXTURE_H

#include "renderer/texture.h"

#include <glad/glad.h>

namespace CEngine::Renderer::OpenGL
{

class TextureLoader
{
  public:
    static GLuint Load(const Texture &source);
    static GLuint CreateSolid(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha = 255);
};

} // namespace CEngine::Renderer::OpenGL
#endif
