//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/renderer/opengl/shaders/ssao.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#ifndef CENGINE_RENDERER_OPENGL_SHADERS_SSAO_H
#define CENGINE_RENDERER_OPENGL_SHADERS_SSAO_H

#include "shader.h"

namespace CEngine::Renderer
{
class RenderSystem;
}

namespace CEngine::Renderer::OpenGL
{
/**
 * @brief TODO: Describe SSAO.
 */
class SSAO
{
  public:
    /**
     * @brief TODO: Describe SSAO.
     */
    SSAO();
    /**
     * @brief TODO: Describe UseCompute.
     */
    void UseCompute() const;
    /**
     * @brief TODO: Describe UpdateCompute.
     *
     * @param rendering TODO: Describe this parameter.
     */
    void UpdateCompute(const RenderSystem &rendering) const;
    /**
     * @brief TODO: Describe UseComposite.
     */
    void UseComposite() const;
    /**
     * @brief TODO: Describe UpdateComposite.
     *
     * @param rendering TODO: Describe this parameter.
     */
    void UpdateComposite(const RenderSystem &rendering) const;
    /**
     * @brief TODO: Describe SetTextures.
     *
     * @param render TODO: Describe this parameter.
     * @param depth TODO: Describe this parameter.
     * @param normal_roughness TODO: Describe this parameter.
     * @param ao TODO: Describe this parameter.
     * @param width TODO: Describe this parameter.
     * @param height TODO: Describe this parameter.
     */
    void SetTextures(GLuint render, GLuint depth, GLuint normal_roughness, GLuint ao, int width, int height);

  private:
    /**
     * @brief TODO: Describe InitializeParameters.
     */
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
