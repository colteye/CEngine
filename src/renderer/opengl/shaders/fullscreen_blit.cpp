#include "fullscreen_blit.h"

#include "renderer/render_system.h"

#include <glm/geometric.hpp>

namespace CEngine::Renderer {

FullscreenBlit::FullscreenBlit()
{
	shader_program.Load("shaders/opengl/ssao.vert", "shaders/opengl/fullscreen_blit.frag");
	InitializeParameters();
}

void FullscreenBlit::Use() const
{
	shader_program.Use();
}

void FullscreenBlit::Update(const RenderSystem& rendering,
	GLuint source_texture, GLuint depth_texture)
{
	const PostProcessSettings& settings = rendering.GetPostProcessSettings();
	const RenderFrameConstants& frame = rendering.GetFrameConstants();
	glm::vec3 sun_direction(0.0f);
	for (const Light& light : rendering.GetDirectLights())
	{
		if (light.enabled && light.type == LightType::Directional)
		{
			sun_direction = -glm::normalize(light.direction);
			break;
		}
	}
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, source_texture);
	glUniform1i(source_texture_id, 0);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, depth_texture);
	glUniform1i(depth_texture_id, 1);
	glUniform1i(bloom_enabled_id, settings.bloom_enabled);
	glUniform1f(bloom_threshold_id, settings.bloom_threshold);
	glUniform1f(bloom_intensity_id, settings.bloom_intensity);
	glUniform1i(
		tone_mapping_enabled_id, settings.tone_mapping_enabled);
	glUniform1f(exposure_id, settings.exposure);
	glUniform1f(contrast_id, settings.contrast);
	glUniform1f(saturation_id, settings.saturation);
	glUniform1i(
		depth_of_field_enabled_id, settings.depth_of_field_enabled);
	glUniform1f(focus_distance_id, settings.focus_distance);
	glUniform1f(focus_range_id, settings.focus_range);
	glUniform1f(
		depth_of_field_strength_id, settings.depth_of_field_strength);
	glUniform1i(sun_lens_flare_enabled_id,
		settings.sun_lens_flare_enabled &&
			glm::length(sun_direction) > 0.0f);
	glUniform1f(
		sun_lens_flare_intensity_id, settings.sun_lens_flare_intensity);
	glUniform1f(sun_disc_size_id, settings.sun_disc_size);
	glUniform1f(sun_disc_softness_id, settings.sun_disc_softness);
	const glm::vec4 sun_clip = frame.proj * frame.view * glm::vec4(sun_direction, 0.0f);
	glUniform3f(sun_clip_id, sun_clip.x, sun_clip.y, sun_clip.w);
	glUniform1f(near_clip_id, frame.near_clip);
	glUniform1f(far_clip_id, frame.far_clip);
}

void FullscreenBlit::InitializeParameters()
{
	const GLuint program = shader_program.GetId();
	source_texture_id = glGetUniformLocation(program, "source_texture");
	depth_texture_id = glGetUniformLocation(program, "depth_texture");
	bloom_enabled_id = glGetUniformLocation(program, "bloom_enabled");
	bloom_threshold_id = glGetUniformLocation(program, "bloom_threshold");
	bloom_intensity_id = glGetUniformLocation(program, "bloom_intensity");
	tone_mapping_enabled_id =
		glGetUniformLocation(program, "tone_mapping_enabled");
	exposure_id = glGetUniformLocation(program, "exposure");
	contrast_id = glGetUniformLocation(program, "contrast");
	saturation_id = glGetUniformLocation(program, "saturation");
	depth_of_field_enabled_id =
		glGetUniformLocation(program, "depth_of_field_enabled");
	focus_distance_id =
		glGetUniformLocation(program, "focus_distance");
	focus_range_id = glGetUniformLocation(program, "focus_range");
	depth_of_field_strength_id =
		glGetUniformLocation(program, "depth_of_field_strength");
	sun_lens_flare_enabled_id =
		glGetUniformLocation(program, "sun_lens_flare_enabled");
	sun_lens_flare_intensity_id =
		glGetUniformLocation(program, "sun_lens_flare_intensity");
	sun_disc_size_id = glGetUniformLocation(program, "sun_disc_size");
	sun_disc_softness_id =
		glGetUniformLocation(program, "sun_disc_softness");
	sun_clip_id = glGetUniformLocation(program, "sun_clip");
	near_clip_id = glGetUniformLocation(program, "near_clip");
	far_clip_id = glGetUniformLocation(program, "far_clip");
}

} // namespace CEngine::Renderer
