#include "pbr_standard.h"

#include "renderer/render_system.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace CEngine::Renderer {

PBRStandard::PBRStandard()
	: albedo_tex(0),
	  normal_tex(0),
	  metallic_roughness_ao_tex(0),
	  lightmap_tex(0)
{
    shader_program.Load("shaders/opengl/pbr_standard.vert",
                        "shaders/opengl/pbr_standard.frag");
    InitializeParameters();
}

void PBRStandard::Use() const
{
    shader_program.Use();
}

void PBRStandard::UpdateFrame(const OpenGLShadowGpuData& shadow_data,
    GLuint shadow_atlas, const std::array<GLuint, OpenGLShadows::kMaxPointShadows>& point_shadow_maps)
{
	const RenderFrameConstants& constants = RenderSystem::GetFrameConstants();
	glUniform3fv(cam_pos_id, 1, glm::value_ptr(constants.camera_position));
	glUniformMatrix4fv(v_id, 1, GL_FALSE, glm::value_ptr(constants.view));
	glUniformMatrix4fv(p_id, 1, GL_FALSE, glm::value_ptr(constants.proj));
	ambient_uniforms.Upload();
	direct_lights.BindAndUploadIfNeeded();
    shadow_buffer.Upload(shadow_data);
    shadow_samplers.Bind(shadow_atlas, point_shadow_maps);
}

void PBRStandard::UpdateObject(const glm::mat4& model, const Material& material,
	const glm::vec2& lightmap_scale, const glm::vec2& lightmap_offset, float lightmap_rgbm_range)
{
	SetParametersStatic();
	SetMaterialParameters(material);
	glUniformMatrix4fv(m_id, 1, GL_FALSE, glm::value_ptr(model));
	glUniform4f(lightmap_scale_offset_id, lightmap_scale.x, lightmap_scale.y,
		lightmap_offset.x, lightmap_offset.y);
	glUniform1f(lightmap_rgbm_range_id, lightmap_rgbm_range);
	glUniform1i(has_lightmap_id, lightmap_tex != 0 ? 1 : 0);
}

void PBRStandard::InitializeParameters()
{
    const GLuint shader_id = shader_program.GetId();
    direct_lights.Initialize(shader_id, "DirectLightBlock");
    shadow_buffer.Initialize(shader_id, "ShadowBlock");
    shadow_samplers.Initialize(shader_id);
    ambient_uniforms.Initialize(shader_id);

    cam_pos_id = glGetUniformLocation(shader_id, "cam_pos_world");

    m_id = glGetUniformLocation(shader_id, "model");
    v_id = glGetUniformLocation(shader_id, "view");
    p_id = glGetUniformLocation(shader_id, "projection");

    albedo_id = glGetUniformLocation(shader_id, "albedo");
    normal_id = glGetUniformLocation(shader_id, "normal");
    metallic_roughness_ao_id = glGetUniformLocation(shader_id, "metallic_roughness_ao");
    base_color_factor_id = glGetUniformLocation(shader_id, "base_color_factor");
    alpha_cutoff_id = glGetUniformLocation(shader_id, "alpha_cutoff");
    render_mode_id = glGetUniformLocation(shader_id, "render_mode");
	receives_shadows_id = glGetUniformLocation(shader_id, "receives_shadows");
	lightmap_id = glGetUniformLocation(shader_id, "lightmap");
	lightmap_scale_offset_id = glGetUniformLocation(shader_id, "lightmap_scale_offset");
	lightmap_rgbm_range_id = glGetUniformLocation(shader_id, "lightmap_rgbm_range");
	has_lightmap_id = glGetUniformLocation(shader_id, "has_lightmap");
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
    glUniform1i(albedo_id, 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, normal_tex);
    glUniform1i(normal_id, 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, metallic_roughness_ao_tex);
	glUniform1i(metallic_roughness_ao_id, 2);

	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, lightmap_tex);
	glUniform1i(lightmap_id, 3);
}

void PBRStandard::SetMaterialParameters(const Material& material)
{
    const glm::vec4& base_color = material.GetBaseColorFactor();
    glUniform4fv(base_color_factor_id, 1, glm::value_ptr(base_color));
    glUniform1f(alpha_cutoff_id, material.GetAlphaCutoff());
    glUniform1i(render_mode_id, static_cast<int>(material.GetRenderMode()));
    glUniform1i(receives_shadows_id, material.ReceivesShadows() ? 1 : 0);
}

} // namespace CEngine::Renderer
