#include "depth_only.h"

#include <glm/gtc/type_ptr.hpp>

namespace {
bool RequiresAlphaTest(MaterialRenderMode mode)
{
	return mode == MaterialRenderMode::AlphaClip || mode == MaterialRenderMode::AlphaHashDither;
}
}

DepthOnly::DepthOnly()
{
	shader_program.Load("shaders/opengl/depth_only.vert", "shaders/opengl/depth_only.frag");
	InitializeParameters();
}

void DepthOnly::Use() const
{
	shader_program.Use();
}

void DepthOnly::Update(const glm::mat4& model, const glm::mat4& view, const glm::mat4& projection,
	const Material& material, GLuint albedo_texture)
{
	const bool alpha_test = RequiresAlphaTest(material.GetRenderMode());

	glUniformMatrix4fv(model_id, 1, GL_FALSE, glm::value_ptr(model));
	glUniformMatrix4fv(view_id, 1, GL_FALSE, glm::value_ptr(view));
	glUniformMatrix4fv(projection_id, 1, GL_FALSE, glm::value_ptr(projection));
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

void DepthOnly::InitializeParameters()
{
	const GLuint shader_id = shader_program.GetId();
	model_id = glGetUniformLocation(shader_id, "model");
	view_id = glGetUniformLocation(shader_id, "view");
	projection_id = glGetUniformLocation(shader_id, "projection");
	albedo_id = glGetUniformLocation(shader_id, "albedo");
	base_color_factor_id = glGetUniformLocation(shader_id, "base_color_factor");
	alpha_cutoff_id = glGetUniformLocation(shader_id, "alpha_cutoff");
	alpha_test_id = glGetUniformLocation(shader_id, "alpha_test");
}
