#ifndef FULLSCREEN_BLIT_H
#define FULLSCREEN_BLIT_H

#include "shader.h"

namespace CEngine::Renderer
{

class RenderSystem;

class FullscreenBlit
{
  public:
    FullscreenBlit();

    void Use() const;
    void Update(const RenderSystem &rendering, GLuint source_texture, GLuint depth_texture) const;

  private:
    void InitializeParameters();

    ShaderProgram shader_program_;
    GLint source_texture_id_ = -1;
    GLint depth_texture_id_ = -1;
    GLint bloom_enabled_id_ = -1;
    GLint bloom_threshold_id_ = -1;
    GLint bloom_intensity_id_ = -1;
    GLint tone_mapping_enabled_id_ = -1;
    GLint exposure_id_ = -1;
    GLint contrast_id_ = -1;
    GLint saturation_id_ = -1;
    GLint depth_of_field_enabled_id_ = -1;
    GLint focus_distance_id_ = -1;
    GLint focus_range_id_ = -1;
    GLint depth_of_field_strength_id_ = -1;
    GLint sun_lens_flare_enabled_id_ = -1;
    GLint sun_lens_flare_intensity_id_ = -1;
    GLint sun_disc_size_id_ = -1;
    GLint sun_disc_softness_id_ = -1;
    GLint sun_clip_id_ = -1;
    GLint near_clip_id_ = -1;
    GLint far_clip_id_ = -1;
};

} // namespace CEngine::Renderer
#endif
