#ifndef CENGINE_RENDERER_OPENGL_SHADERS_DEPTH_ONLY_H
#define CENGINE_RENDERER_OPENGL_SHADERS_DEPTH_ONLY_H

#include "renderer/material.h"
#include "shader.h"

#include <glm/glm.hpp>

namespace CEngine::Renderer::OpenGL
{

class DepthOnly
{
  public:
    DepthOnly();

    void Use() const;
    void UpdateFrame(const glm::mat4 &view, const glm::mat4 &projection) const;
    void UpdateObject(const glm::mat4 &model, const Material &material, GLuint albedo_texture) const;

  private:
    void InitializeParameters();

    ShaderProgram shader_program_;
    GLint model_id_ = -1;
    GLint view_id_ = -1;
    GLint projection_id_ = -1;
    GLint albedo_id_ = -1;
    GLint base_color_factor_id_ = -1;
    GLint alpha_cutoff_id_ = -1;
    GLint alpha_test_id_ = -1;
};

} // namespace CEngine::Renderer::OpenGL
#endif
