#include "point_shadow_depth.h"

#include <glm/gtc/type_ptr.hpp>

namespace CEngine::Renderer {

namespace {
bool RequiresAlphaTest(MaterialRenderMode mode)
{
	return mode == MaterialRenderMode::AlphaClip || mode == MaterialRenderMode::AlphaHashDither;
}
} // namespace

PointShadowDepth::PointShadowDepth()
{
	shader_program.Load("shaders/opengl/point_shadow_depth.vert", "shaders/opengl/point_shadow_depth.frag");
	InitializeParameters();
}

void PointShadowDepth::Use() const
{
	shader_program.Use();
}

void PointShadowDepth::Update(const glm::mat4& model, const glm::mat4& view, const glm::mat4& projection,
	const glm::vec3& light_position, float far_plane, const Material& material, GLuint albedo_texture)
{
	const bool alpha_test = RequiresAlphaTest(material.GetRenderMode());

	glUniformMatrix4fv(model_id, 1, GL_FALSE, glm::value_ptr(model));
	glUniformMatrix4fv(view_id, 1, GL_FALSE, glm::value_ptr(view));
	glUniformMatrix4fv(projection_id, 1, GL_FALSE, glm::value_ptr(projection));
	glUniform3fv(light_position_id, 1, glm::value_ptr(light_position));
	glUniform1f(far_plane_id, far_plane);
	glUniform4fv(base_color_factor_id, 1, glm::value_ptr(material.GetBaseColorFactor()));
	glUniform1f(alpha_cutoff_id, material.GetAlphaCutoff());
	glUniform1i(alpha_test_id, alpha_test ? 1 : 0);

	if (alpha_test)
	{
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, albedo_texture);
		glUniform1i(albedo_id, 0);
	}
}

void PointShadowDepth::InitializeParameters()
{
	const GLuint shader_id = shader_program.GetId();
	model_id = glGetUniformLocation(shader_id, "model");
	view_id = glGetUniformLocation(shader_id, "view");
	projection_id = glGetUniformLocation(shader_id, "projection");
	light_position_id = glGetUniformLocation(shader_id, "light_position");
	far_plane_id = glGetUniformLocation(shader_id, "far_plane");
	albedo_id = glGetUniformLocation(shader_id, "albedo");
	base_color_factor_id = glGetUniformLocation(shader_id, "base_color_factor");
	alpha_cutoff_id = glGetUniformLocation(shader_id, "alpha_cutoff");
	alpha_test_id = glGetUniformLocation(shader_id, "alpha_test");
}

} // namespace CEngine::Renderer
