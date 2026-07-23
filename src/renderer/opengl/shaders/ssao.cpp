#include "ssao.h"

#include "renderer/render_system.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace CEngine::Renderer {

SSAO::SSAO() : texture_width(1),
	  texture_height(1)
{
	compute_program.Load("shaders/opengl/ssao.vert", "shaders/opengl/ssao.frag");
	composite_program.Load("shaders/opengl/ssao.vert", "shaders/opengl/ssao_composite.frag");
    InitializeParameters();
}

void SSAO::UseCompute() const
{
	compute_program.Use();
}

void SSAO::UpdateCompute(const RenderSystem& rendering)
{
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, depth_tex);
	glUniform1i(depth_id, 0);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, normal_roughness_tex);
	glUniform1i(normal_roughness_id, 1);

	const RenderFrameConstants& constants = rendering.GetFrameConstants();
	const glm::mat4 inverse_projection = glm::inverse(constants.proj);
	glUniformMatrix4fv(projection_id, 1, GL_FALSE, glm::value_ptr(constants.proj));
	glUniformMatrix4fv(inverse_projection_id, 1, GL_FALSE, glm::value_ptr(inverse_projection));
	glUniformMatrix4fv(view_id, 1, GL_FALSE, glm::value_ptr(constants.view));
	const SSAOSettings& settings = rendering.GetSSAOSettings();
	glUniform1f(radius_id, settings.radius);
	glUniform1f(bias_id, settings.bias);
	glUniform1f(intensity_id, settings.enabled ? settings.intensity : 0.0f);
	glUniform1f(contrast_id, settings.contrast);
}

void SSAO::UseComposite() const
{
	composite_program.Use();
}

void SSAO::UpdateComposite(const RenderSystem& rendering)
{
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, render_tex);
	glUniform1i(composite_render_id, 0);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, depth_tex);
	glUniform1i(composite_depth_id, 1);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, ao_tex);
	glUniform1i(composite_ao_id, 2);
	glUniform2f(texel_size_id, 1.0f / texture_width, 1.0f / texture_height);
	const RenderFrameConstants& constants = rendering.GetFrameConstants();
	const glm::mat4 inverse_projection = glm::inverse(constants.proj);
	glUniformMatrix4fv(composite_inverse_projection_id, 1, GL_FALSE, glm::value_ptr(inverse_projection));
	const ExponentialHeightFog& fog = rendering.GetExponentialHeightFog();
	glUniform1i(fog_enabled_id, fog.enabled);
	glUniform1f(fog_density_id, fog.density);
	glUniform1f(fog_start_distance_id, fog.start_distance);
}

void SSAO::InitializeParameters()
{
	const GLuint compute_id = compute_program.GetId();
	depth_id = glGetUniformLocation(compute_id, "depth_tex");
	normal_roughness_id = glGetUniformLocation(compute_id, "normal_roughness_tex");
	projection_id = glGetUniformLocation(compute_id, "projection");
	inverse_projection_id = glGetUniformLocation(compute_id, "inverse_projection");
	view_id = glGetUniformLocation(compute_id, "view");
	radius_id = glGetUniformLocation(compute_id, "radius");
	bias_id = glGetUniformLocation(compute_id, "bias");
	intensity_id = glGetUniformLocation(compute_id, "intensity");
	contrast_id = glGetUniformLocation(compute_id, "contrast");

	const GLuint composite_id = composite_program.GetId();
	composite_render_id = glGetUniformLocation(composite_id, "render_tex");
	composite_depth_id = glGetUniformLocation(composite_id, "depth_tex");
	composite_ao_id = glGetUniformLocation(composite_id, "ao_tex");
	texel_size_id = glGetUniformLocation(composite_id, "texel_size");
	composite_inverse_projection_id = glGetUniformLocation(composite_id, "inverse_projection");
	fog_enabled_id = glGetUniformLocation(composite_id, "fog_enabled");
	fog_density_id = glGetUniformLocation(composite_id, "fog_density");
	fog_start_distance_id = glGetUniformLocation(composite_id, "fog_start_distance");
}

void SSAO::SetTextures(GLuint render, GLuint depth, GLuint normal_roughness, GLuint ao, int width, int height)
{
    render_tex = render;
    depth_tex = depth;
    normal_roughness_tex = normal_roughness;
	ao_tex = ao;
    texture_width = width;
    texture_height = height;
}

} // namespace CEngine::Renderer
