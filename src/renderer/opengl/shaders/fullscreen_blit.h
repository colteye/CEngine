#ifndef FULLSCREEN_BLIT_H
#define FULLSCREEN_BLIT_H

#include "shader.h"

namespace CEngine::Renderer {

class RenderSystem;

class FullscreenBlit
{
public:
	FullscreenBlit();

	void Use() const;
	void Update(const RenderSystem& rendering,
		GLuint source_texture, GLuint depth_texture);

private:
	void InitializeParameters();

	ShaderProgram shader_program;
	GLint source_texture_id = -1;
	GLint depth_texture_id = -1;
	GLint bloom_enabled_id = -1;
	GLint bloom_threshold_id = -1;
	GLint bloom_intensity_id = -1;
	GLint tone_mapping_enabled_id = -1;
	GLint exposure_id = -1;
	GLint contrast_id = -1;
	GLint saturation_id = -1;
	GLint depth_of_field_enabled_id = -1;
	GLint focus_distance_id = -1;
	GLint focus_range_id = -1;
	GLint depth_of_field_strength_id = -1;
	GLint sun_lens_flare_enabled_id = -1;
	GLint sun_lens_flare_intensity_id = -1;
	GLint sun_disc_size_id = -1;
	GLint sun_disc_softness_id = -1;
	GLint sun_clip_id = -1;
	GLint near_clip_id = -1;
	GLint far_clip_id = -1;
};


} // namespace CEngine::Renderer
#endif
