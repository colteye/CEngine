#ifndef PBR_STANDARD_H
#define PBR_STANDARD_H

#include "lighting_bindings.h"
#include "shader.h"

#include "renderer/material.h"

namespace CEngine::Renderer
{

class RenderSystem;

class PBRStandard
{
  public:
    GLuint albedo_tex{0};
    GLuint normal_tex{0};
    GLuint metallic_roughness_ao_tex{0};
    GLuint lightmap_tex{0};

    PBRStandard();
    void Use() const;
    void UpdateFrame(RenderSystem &rendering, const OpenGLShadowGpuData &shadow_data, GLuint shadow_atlas,
                     const std::array<GLuint, OpenGLShadows::KMaxPointShadows> &point_shadow_maps,
                     GLuint irradiance_map, GLuint prefiltered_map);
    void UpdateObject(const glm::mat4 &model, const Material &material, const glm::vec2 &lightmap_scale,
                      const glm::vec2 &lightmap_offset, float lightmap_rgbm_range);
    void SetTextures(GLuint albedo, GLuint normal, GLuint metallic_roughness_ao, GLuint lightmap);

  private:
    void InitializeParameters();
    void SetParametersStatic();
    void SetMaterialParameters(const Material &material) const;

    ShaderProgram shader_program_;
    OpenGLDirectLightBuffer direct_lights_;
    OpenGLShadowBuffer shadow_buffer_;
    OpenGLShadowSamplers shadow_samplers_;
    OpenGLAmbientUniforms ambient_uniforms_;
    OpenGLEnvironmentUniforms environment_uniforms_;
    GLuint cam_pos_id_{};
    GLuint m_id_{}, v_id_{}, p_id_{};
    GLuint albedo_id_{}, normal_id_{}, metallic_roughness_ao_id_{};
    GLuint base_color_factor_id_{};
    GLuint metallic_roughness_ao_factors_id_{};
    GLuint alpha_cutoff_id_{};
    GLuint render_mode_id_{};
    GLuint receives_shadows_id_{};
    GLuint lightmap_id_{};
    GLuint lightmap_scale_offset_id_{};
    GLuint lightmap_rgbm_range_id_{};
    GLuint has_lightmap_id_{};
};

} // namespace CEngine::Renderer
#endif
