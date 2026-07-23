#include "pbr_geometry_pass.h"

#include "renderer/render_system.h"

#include <glm/gtc/type_ptr.hpp>

namespace CEngine::Renderer
{

PBRGeometryPass::PBRGeometryPass()
{
    shader_program_.Load("shaders/opengl/pbr_geometry_pass.vert", "shaders/opengl/pbr_geometry_pass.frag");
    InitializeParameters();
}

void PBRGeometryPass::Use() const
{
    shader_program_.Use();
}

void PBRGeometryPass::UpdateFrame(const RenderSystem &rendering) const
{
    const RenderFrameConstants &constants = rendering.GetFrameConstants();
    glUniformMatrix4fv(view_id_, 1, GL_FALSE, glm::value_ptr(constants.view));
    glUniformMatrix4fv(projection_id_, 1, GL_FALSE, glm::value_ptr(constants.proj));
}

void PBRGeometryPass::UpdateObject(const glm::mat4 &model, const Material &material, const glm::vec2 &lightmap_scale,
                                   const glm::vec2 &lightmap_offset, float lightmap_rgbm_range)
{
    glUniformMatrix4fv(model_id_, 1, GL_FALSE, glm::value_ptr(model));
    glUniform4fv(base_color_factor_id_, 1, glm::value_ptr(material.base_color_factor));
    glUniform3fv(metallic_roughness_ao_factors_id_, 1, glm::value_ptr(material.metallic_roughness_ao_factors));
    glUniform1f(alpha_cutoff_id_, material.alpha_cutoff);
    glUniform1i(render_mode_id_, static_cast<int>(material.render_mode));
    glUniform1i(receives_shadows_id_, material.receives_shadows ? 1 : 0);
    glUniform4f(lightmap_scale_offset_id_, lightmap_scale.x, lightmap_scale.y, lightmap_offset.x, lightmap_offset.y);
    glUniform1f(lightmap_rgbm_range_id_, lightmap_rgbm_range);
    glUniform1i(has_lightmap_id_, lightmap_tex_ != 0 ? 1 : 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, albedo_tex_);
    glUniform1i(albedo_id_, 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, normal_tex_);
    glUniform1i(normal_id_, 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, metallic_roughness_ao_tex_);
    glUniform1i(metallic_roughness_ao_id_, 2);

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, lightmap_tex_ != 0 ? lightmap_tex_ : albedo_tex_);
    glUniform1i(lightmap_id_, 3);
}

void PBRGeometryPass::SetTextures(GLuint albedo, GLuint normal, GLuint metallic_roughness_ao, GLuint lightmap)
{
    albedo_tex_ = albedo;
    normal_tex_ = normal;
    metallic_roughness_ao_tex_ = metallic_roughness_ao;
    lightmap_tex_ = lightmap;
}

void PBRGeometryPass::InitializeParameters()
{
    const GLuint shader_id = shader_program_.GetId();
    model_id_ = glGetUniformLocation(shader_id, "model");
    view_id_ = glGetUniformLocation(shader_id, "view");
    projection_id_ = glGetUniformLocation(shader_id, "projection");
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

} // namespace CEngine::Renderer
