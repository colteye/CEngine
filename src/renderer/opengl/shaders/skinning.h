// Copyright (c) CEngine contributors.

#ifndef CENGINE_RENDERER_OPENGL_SHADERS_SKINNING_H
#define CENGINE_RENDERER_OPENGL_SHADERS_SKINNING_H

#include "shader.h"

#include <cstdint>

namespace CEngine::Renderer::OpenGL
{

class SkinningUniforms
{
  public:
    void Initialize(GLuint program)
    {
        palette_id_ = glGetUniformLocation(program, "joint_palette");
        joint_count_id_ = glGetUniformLocation(program, "joint_count");
    }

    void Bind(GLuint texture, std::uint32_t joint_count) const
    {
        constexpr GLint TextureUnit = 15;
        glActiveTexture(GL_TEXTURE0 + TextureUnit);
        glBindTexture(GL_TEXTURE_BUFFER, texture);
        glUniform1i(palette_id_, TextureUnit);
        glUniform1i(joint_count_id_, static_cast<GLint>(joint_count));
    }

  private:
    GLint palette_id_ = -1;
    GLint joint_count_id_ = -1;
};

} // namespace CEngine::Renderer::OpenGL

#endif
