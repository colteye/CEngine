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

void FullscreenBlit::Update(GLuint source_texture, GLuint depth_texture)
{
	const PostProcessSettings& settings = RenderSystem::GetPostProcessSettings();
	const RenderFrameConstants& frame = RenderSystem::GetFrameConstants();
	glm::vec3 sun_direction(0.0f);
	for (const LightRecord& light : RenderSystem::GetDirectLights())
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
	const GLuint program = shader_program.GetId();
	glUniform1i(glGetUniformLocation(program, "bloom_enabled"), settings.bloom_enabled);
	glUniform1f(glGetUniformLocation(program, "bloom_threshold"), settings.bloom_threshold);
	glUniform1f(glGetUniformLocation(program, "bloom_intensity"), settings.bloom_intensity);
	glUniform1i(glGetUniformLocation(program, "tone_mapping_enabled"), settings.tone_mapping_enabled);
	glUniform1f(glGetUniformLocation(program, "exposure"), settings.exposure);
	glUniform1f(glGetUniformLocation(program, "contrast"), settings.contrast);
	glUniform1f(glGetUniformLocation(program, "saturation"), settings.saturation);
	glUniform1i(glGetUniformLocation(program, "depth_of_field_enabled"), settings.depth_of_field_enabled);
	glUniform1f(glGetUniformLocation(program, "focus_distance"), settings.focus_distance);
	glUniform1f(glGetUniformLocation(program, "focus_range"), settings.focus_range);
	glUniform1f(glGetUniformLocation(program, "depth_of_field_strength"), settings.depth_of_field_strength);
	glUniform1i(glGetUniformLocation(program, "sun_lens_flare_enabled"), settings.sun_lens_flare_enabled && glm::length(sun_direction) > 0.0f);
	glUniform1f(glGetUniformLocation(program, "sun_lens_flare_intensity"), settings.sun_lens_flare_intensity);
	glUniform1f(glGetUniformLocation(program, "sun_disc_size"), settings.sun_disc_size);
	glUniform1f(glGetUniformLocation(program, "sun_disc_softness"), settings.sun_disc_softness);
	const glm::vec4 sun_clip = frame.proj * frame.view * glm::vec4(sun_direction, 0.0f);
	glUniform3f(glGetUniformLocation(program, "sun_clip"), sun_clip.x, sun_clip.y, sun_clip.w);
	glUniform1f(glGetUniformLocation(program, "near_clip"), frame.near_clip);
	glUniform1f(glGetUniformLocation(program, "far_clip"), frame.far_clip);
}

void FullscreenBlit::InitializeParameters()
{
	source_texture_id = glGetUniformLocation(shader_program.GetId(), "source_texture");
	depth_texture_id = glGetUniformLocation(shader_program.GetId(), "depth_texture");
}

} // namespace CEngine::Renderer
