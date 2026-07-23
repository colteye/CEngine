//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/renderer/opengl/shaders/depth_only.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#ifndef CENGINE_RENDERER_OPENGL_SHADERS_DEPTH_ONLY_H
#define CENGINE_RENDERER_OPENGL_SHADERS_DEPTH_ONLY_H

#include "renderer/material.h"
#include "shader.h"

#include <glm/glm.hpp>

namespace CEngine::Renderer::OpenGL
{

/**
 * @brief TODO: Describe DepthOnly.
 */
class DepthOnly
{
  public:
    /**
     * @brief TODO: Describe DepthOnly.
     */
    DepthOnly();

    /**
     * @brief TODO: Describe Use.
     */
    void Use() const;
    /**
     * @brief TODO: Describe UpdateFrame.
     *
     * @param view TODO: Describe this parameter.
     * @param projection TODO: Describe this parameter.
     */
    void UpdateFrame(const glm::mat4 &view, const glm::mat4 &projection) const;
    /**
     * @brief TODO: Describe UpdateObject.
     *
     * @param model TODO: Describe this parameter.
     * @param material TODO: Describe this parameter.
     * @param albedo_texture TODO: Describe this parameter.
     */
    void UpdateObject(const glm::mat4 &model, const Material &material, GLuint albedo_texture) const;

  private:
    /**
     * @brief TODO: Describe InitializeParameters.
     */
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
