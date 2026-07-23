#include "pbr_standard.h"

#include "renderer/render_system.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace CEngine::Renderer
{

PBRStandard::PBRStandard()
{
    shader_program_.Load("shaders/opengl/pbr_standard.vert", "shaders/opengl/pbr_standard.frag");
    InitializeParameters();
}

void PBRStandard::Use() const
{
    shader_program_.Use();
}

void PBRStandard::UpdateFrame(RenderSystem &rendering, const OpenGLShadowGpuData &shadow_data, GLuint shadow_atlas,
                              const std::array<GLuint, OpenGLShadows::KMaxPointShadows> &point_shadow_maps,
                              GLuint irradiance_map, GLuint prefiltered_map)
{
    const RenderFrameConstants &constants = rendering.GetFrameConstants();
    glUniform3fv(cam_pos_id_, 1, glm::value_ptr(constants.camera_position));
    glUniformMatrix4fv(v_id_, 1, GL_FALSE, glm::value_ptr(constants.view));
    glUniformMatrix4fv(p_id_, 1, GL_FALSE, glm::value_ptr(constants.proj));
    ambient_uniforms_.Upload(rendering);
    direct_lights_.BindAndUploadIfNeeded(rendering);
    shadow_buffer_.Upload(shadow_data);
    shadow_samplers_.Bind(shadow_atlas, point_shadow_maps);
    environment_uniforms_.BindAndUpload(rendering, irradiance_map, prefiltered_map);
}

void PBRStandard::UpdateObject(const glm::mat4 &model, const Material &material, const glm::vec2 &lightmap_scale,
                               const glm::vec2 &lightmap_offset, float lightmap_rgbm_range)
{
    SetParametersStatic();
    SetMaterialParameters(material);
    glUniformMatrix4fv(m_id_, 1, GL_FALSE, glm::value_ptr(model));
    glUniform4f(lightmap_scale_offset_id_, lightmap_scale.x, lightmap_scale.y, lightmap_offset.x, lightmap_offset.y);
    glUniform1f(lightmap_rgbm_range_id_, lightmap_rgbm_range);
    glUniform1i(has_lightmap_id_, lightmap_tex != 0 ? 1 : 0);
}

void PBRStandard::InitializeParameters()
{
    const GLuint shader_id = shader_program_.GetId();
    direct_lights_.Initialize(shader_id, "DirectLightBlock");
    shadow_buffer_.Initialize(shader_id, "ShadowBlock");
    shadow_samplers_.Initialize(shader_id);
    ambient_uniforms_.Initialize(shader_id);
    environment_uniforms_.Initialize(shader_id);

    cam_pos_id_ = glGetUniformLocation(shader_id, "cam_pos_world");

    m_id_ = glGetUniformLocation(shader_id, "model");
    v_id_ = glGetUniformLocation(shader_id, "view");
    p_id_ = glGetUniformLocation(shader_id, "projection");

    albedo_id_ = glGetUniformLocation(shader_id, "albedo");
    normal_id_ = glGetUniformLocation(shader_id, "normal");
    metallic_roughness_ao_id_ = glGetUniformLocation(shader_id, "metallic_roughness_ao");
    base_color_factor_id_ = glGetUniformLocation(shader_id, "base_color_factor");
    metallic_roughness_ao_factors_id_ = glGetUniformLocation(shader_id, "metallic_roughness_ao_factors");
    alpha_cutoff_id_ = glGetUniformLocation(shader_id, "alpha_cutoff");
    render_mode_id_ = glGetUniformLocation(shader_id, "render_mode");
    receives_shadows_id_ = glGetUniformLocation(shader_id, "receives_shadows");
    lightmap_id_ = glGetUniformLocation(shader_id, "lightmap");
    lightmap_scale_offset_id_ = glGetUniformLocation(shader_id, "lightmap_scale_offset");
    lightmap_rgbm_range_id_ = glGetUniformLocation(shader_id, "lightmap_rgbm_range");
    has_lightmap_id_ = glGetUniformLocation(shader_id, "has_lightmap");
}

void PBRStandard::SetTextures(GLuint albedo, GLuint normal, GLuint metallic_roughness_ao, GLuint lightmap)
{
    albedo_tex = albedo;
    normal_tex = normal;
    metallic_roughness_ao_tex = metallic_roughness_ao;
    lightmap_tex = lightmap;
}

void PBRStandard::SetParametersStatic()
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, albedo_tex);
    glUniform1i(albedo_id_, 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, normal_tex);
    glUniform1i(normal_id_, 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, metallic_roughness_ao_tex);
    glUniform1i(metallic_roughness_ao_id_, 2);

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, lightmap_tex != 0 ? lightmap_tex : albedo_tex);
    glUniform1i(lightmap_id_, 3);
}

void PBRStandard::SetMaterialParameters(const Material &material) const
{
    const glm::vec4 &base_color = material.base_color_factor;
    glUniform4fv(base_color_factor_id_, 1, glm::value_ptr(base_color));
    glUniform3fv(metallic_roughness_ao_factors_id_, 1, glm::value_ptr(material.metallic_roughness_ao_factors));
    glUniform1f(alpha_cutoff_id_, material.alpha_cutoff);
    glUniform1i(render_mode_id_, static_cast<int>(material.render_mode));
    glUniform1i(receives_shadows_id_, material.receives_shadows ? 1 : 0);
}

} // namespace CEngine::Renderer
