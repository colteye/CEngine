#include "pbr_geometry_pass.h"

#include "render_system.h"

#include <glm/gtc/type_ptr.hpp>

PBRGeometryPass::PBRGeometryPass()
{
	shader_program.Load("shaders/opengl/pbr_geometry_pass.vert", "shaders/opengl/pbr_geometry_pass.frag");
	InitializeParameters();
}

void PBRGeometryPass::Use() const
{
	shader_program.Use();
}

void PBRGeometryPass::Update(const glm::mat4& model, const Material& material)
{
	const RenderFrameConstants& constants = RenderSystem::GetFrameConstants();
	glUniformMatrix4fv(model_id, 1, GL_FALSE, glm::value_ptr(model));
	glUniformMatrix4fv(view_id, 1, GL_FALSE, glm::value_ptr(constants.view));
	glUniformMatrix4fv(projection_id, 1, GL_FALSE, glm::value_ptr(constants.proj));
	glUniform4fv(base_color_factor_id, 1, glm::value_ptr(material.GetBaseColorFactor()));
	glUniform1f(alpha_cutoff_id, material.GetAlphaCutoff());
	glUniform1i(render_mode_id, static_cast<int>(material.GetRenderMode()));
	glUniform1i(receives_shadows_id, material.ReceivesShadows() ? 1 : 0);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, albedo_tex);
	glUniform1i(albedo_id, 0);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, normal_tex);
	glUniform1i(normal_id, 1);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, metallic_roughness_ao_tex);
	glUniform1i(metallic_roughness_ao_id, 2);
}

void PBRGeometryPass::SetTextures(GLuint albedo, GLuint normal, GLuint metallic_roughness_ao)
{
	albedo_tex = albedo;
	normal_tex = normal;
	metallic_roughness_ao_tex = metallic_roughness_ao;
}

void PBRGeometryPass::InitializeParameters()
{
	const GLuint shader_id = shader_program.GetId();
	model_id = glGetUniformLocation(shader_id, "model");
	view_id = glGetUniformLocation(shader_id, "view");
	projection_id = glGetUniformLocation(shader_id, "projection");
	albedo_id = glGetUniformLocation(shader_id, "albedo");
	normal_id = glGetUniformLocation(shader_id, "normal");
	metallic_roughness_ao_id = glGetUniformLocation(shader_id, "metallic_roughness_ao");
	base_color_factor_id = glGetUniformLocation(shader_id, "base_color_factor");
	alpha_cutoff_id = glGetUniformLocation(shader_id, "alpha_cutoff");
	render_mode_id = glGetUniformLocation(shader_id, "render_mode");
	receives_shadows_id = glGetUniformLocation(shader_id, "receives_shadows");
}
