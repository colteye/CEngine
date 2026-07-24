//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/renderer/opengl/shaders/point_shadow_depth.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#ifndef CENGINE_RENDERER_OPENGL_SHADERS_POINT_SHADOW_DEPTH_H
#define CENGINE_RENDERER_OPENGL_SHADERS_POINT_SHADOW_DEPTH_H

#include "renderer/material.h"
#include "shader.h"
#include "skinning.h"

#include <glm/glm.hpp>

namespace CEngine::Renderer::OpenGL
{

/**
 * @brief TODO: Describe PointShadowDepth.
 */
class PointShadowDepth
{
  public:
    /**
     * @brief TODO: Describe PointShadowDepth.
     */
    PointShadowDepth();

    /**
     * @brief TODO: Describe Use.
     */
    void Use() const;
    /**
     * @brief TODO: Describe UpdateFrame.
     *
     * @param view TODO: Describe this parameter.
     * @param projection TODO: Describe this parameter.
     * @param light_position TODO: Describe this parameter.
     * @param far_plane TODO: Describe this parameter.
     */
    void UpdateFrame(const glm::mat4 &view, const glm::mat4 &projection, const glm::vec3 &light_position,
                     float far_plane) const;
    /**
     * @brief TODO: Describe UpdateObject.
     *
     * @param model TODO: Describe this parameter.
     * @param material TODO: Describe this parameter.
     * @param albedo_texture TODO: Describe this parameter.
     */
    void UpdateObject(const glm::mat4 &model, const Material &material, GLuint albedo_texture) const;
    void UpdateSkinning(GLuint texture, std::uint32_t joint_count) const
    {
        skinning_.Bind(texture, joint_count);
    }

  private:
    /**
     * @brief TODO: Describe InitializeParameters.
     */
    void InitializeParameters();

    ShaderProgram shader_program_;
    GLint model_id_ = -1;
    GLint view_id_ = -1;
    GLint projection_id_ = -1;
    GLint light_position_id_ = -1;
    GLint far_plane_id_ = -1;
    GLint albedo_id_ = -1;
    GLint base_color_factor_id_ = -1;
    GLint alpha_cutoff_id_ = -1;
    GLint alpha_test_id_ = -1;
    SkinningUniforms skinning_;
};

} // namespace CEngine::Renderer::OpenGL
#endif
