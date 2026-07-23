#ifndef CENGINE_RENDERER_OPENGL_SHADERS_SSAO_H
#define CENGINE_RENDERER_OPENGL_SHADERS_SSAO_H

#include "shader.h"

namespace CEngine::Renderer
{
class RenderSystem;
}

namespace CEngine::Renderer::OpenGL
{
class SSAO
{
  public:
    SSAO();
    void UseCompute() const;
    void UpdateCompute(const RenderSystem &rendering) const;
    void UseComposite() const;
    void UpdateComposite(const RenderSystem &rendering) const;
    void SetTextures(GLuint render, GLuint depth, GLuint normal_roughness, GLuint ao, int width, int height);

  private:
    void InitializeParameters();
    ShaderProgram compute_program_;
    ShaderProgram composite_program_;
    GLuint render_tex_ = 0;
    GLuint depth_tex_ = 0;
    GLuint normal_roughness_tex_ = 0;
    GLuint ao_tex_ = 0;
    GLint depth_id_ = -1;
    GLint normal_roughness_id_ = -1;
    GLint projection_id_ = -1;
    GLint inverse_projection_id_ = -1;
    GLint view_id_ = -1;
    GLint radius_id_ = -1;
    GLint bias_id_ = -1;
    GLint intensity_id_ = -1;
    GLint contrast_id_ = -1;
    GLint composite_render_id_ = -1;
    GLint composite_depth_id_ = -1;
    GLint composite_ao_id_ = -1;
    GLint texel_size_id_ = -1;
    GLint composite_inverse_projection_id_ = -1;
    GLint fog_enabled_id_ = -1;
    GLint fog_density_id_ = -1;
    GLint fog_start_distance_id_ = -1;

    int texture_width_{1};
    int texture_height_{1};
};

} // namespace CEngine::Renderer::OpenGL
#endif
