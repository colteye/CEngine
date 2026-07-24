//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/renderer/opengl/shaders/pbr_geometry_pass.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#ifndef CENGINE_RENDERER_OPENGL_SHADERS_PBR_GEOMETRY_PASS_H
#define CENGINE_RENDERER_OPENGL_SHADERS_PBR_GEOMETRY_PASS_H

#include "renderer/material.h"
#include "shader.h"
#include "skinning.h"

#include <glm/glm.hpp>

namespace CEngine::Renderer
{
class RenderSystem;
}

namespace CEngine::Renderer::OpenGL
{
/**
 * @brief TODO: Describe PBRGeometryPass.
 */
class PBRGeometryPass
{
  public:
    /**
     * @brief TODO: Describe PBRGeometryPass.
     */
    PBRGeometryPass();

    /**
     * @brief TODO: Describe Use.
     */
    void Use() const;
    /**
     * @brief TODO: Describe UpdateFrame.
     *
     * @param rendering TODO: Describe this parameter.
     */
    void UpdateFrame(const RenderSystem &rendering) const;
    /**
     * @brief TODO: Describe UpdateObject.
     *
     * @param model TODO: Describe this parameter.
     * @param material TODO: Describe this parameter.
     * @param lightmap_scale TODO: Describe this parameter.
     * @param lightmap_offset TODO: Describe this parameter.
     * @param lightmap_rgbm_range TODO: Describe this parameter.
     */
    void UpdateObject(const glm::mat4 &model, const Material &material, const glm::vec2 &lightmap_scale,
                      const glm::vec2 &lightmap_offset, float lightmap_rgbm_range);
    /**
     * @brief TODO: Describe SetTextures.
     *
     * @param albedo TODO: Describe this parameter.
     * @param normal TODO: Describe this parameter.
     * @param metallic_roughness_ao TODO: Describe this parameter.
     * @param lightmap TODO: Describe this parameter.
     */
    void SetTextures(GLuint albedo, GLuint normal, GLuint metallic_roughness_ao, GLuint lightmap);
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
    GLuint albedo_tex_ = 0;
    GLuint normal_tex_ = 0;
    GLuint metallic_roughness_ao_tex_ = 0;
    GLuint lightmap_tex_ = 0;

    GLint model_id_ = -1;
    GLint view_id_ = -1;
    GLint projection_id_ = -1;
    GLint albedo_id_ = -1;
    GLint normal_id_ = -1;
    GLint metallic_roughness_ao_id_ = -1;
    GLint base_color_factor_id_ = -1;
    GLint metallic_roughness_ao_factors_id_ = -1;
    GLint alpha_cutoff_id_ = -1;
    GLint render_mode_id_ = -1;
    GLint receives_shadows_id_ = -1;
    GLint lightmap_id_ = -1;
    GLint lightmap_scale_offset_id_ = -1;
    GLint lightmap_rgbm_range_id_ = -1;
    GLint has_lightmap_id_ = -1;
    SkinningUniforms skinning_;
};

} // namespace CEngine::Renderer::OpenGL
#endif
