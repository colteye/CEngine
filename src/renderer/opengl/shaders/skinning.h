//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/renderer/opengl/shaders/skinning.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#ifndef CENGINE_RENDERER_OPENGL_SHADERS_SKINNING_H
#define CENGINE_RENDERER_OPENGL_SHADERS_SKINNING_H

#include "shader.h"

#include <cstdint>

namespace CEngine::Renderer::OpenGL
{

class SkinningUniforms
{
  public:
    SkinningUniforms() = default;
    SkinningUniforms(const SkinningUniforms &) = delete;
    SkinningUniforms &operator=(const SkinningUniforms &) = delete;

    ~SkinningUniforms()
    {
        if (fallback_palette_texture_ != 0)
        {
            glDeleteTextures(1, &fallback_palette_texture_);
        }
        if (fallback_palette_buffer_ != 0)
        {
            glDeleteBuffers(1, &fallback_palette_buffer_);
        }
    }

    void Initialize(GLuint program)
    {
        palette_id_ = glGetUniformLocation(program, "joint_palette");
        joint_count_id_ = glGetUniformLocation(program, "joint_count");

        constexpr GLfloat IdentityPalette[] = {
            1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
        };
        glGenBuffers(1, &fallback_palette_buffer_);
        glBindBuffer(GL_TEXTURE_BUFFER, fallback_palette_buffer_);
        glBufferData(GL_TEXTURE_BUFFER, sizeof(IdentityPalette), IdentityPalette, GL_STATIC_DRAW);
        glGenTextures(1, &fallback_palette_texture_);
        glBindTexture(GL_TEXTURE_BUFFER, fallback_palette_texture_);
        glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, fallback_palette_buffer_);
        glBindTexture(GL_TEXTURE_BUFFER, 0);
        glBindBuffer(GL_TEXTURE_BUFFER, 0);
    }

    void Bind(GLuint texture, std::uint32_t joint_count) const
    {
        // Units 14 and 15 are the IBL cubemaps. Keep the texture-buffer
        // sampler on a distinct unit so forward PBR never aliases sampler
        // types within one program.
        constexpr GLint TextureUnit = 10;
        glActiveTexture(GL_TEXTURE0 + TextureUnit);
        glBindTexture(GL_TEXTURE_BUFFER, texture != 0 ? texture : fallback_palette_texture_);
        glUniform1i(palette_id_, TextureUnit);
        glUniform1i(joint_count_id_, static_cast<GLint>(joint_count));
    }

  private:
    GLint palette_id_ = -1;
    GLint joint_count_id_ = -1;
    GLuint fallback_palette_buffer_ = 0;
    GLuint fallback_palette_texture_ = 0;
};

} // namespace CEngine::Renderer::OpenGL

#endif
