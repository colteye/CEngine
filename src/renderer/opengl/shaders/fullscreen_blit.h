//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/renderer/opengl/shaders/fullscreen_blit.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#ifndef CENGINE_RENDERER_OPENGL_SHADERS_FULLSCREEN_BLIT_H
#define CENGINE_RENDERER_OPENGL_SHADERS_FULLSCREEN_BLIT_H

#include "shader.h"

namespace CEngine::Renderer
{
class RenderSystem;
}

namespace CEngine::Renderer::OpenGL
{
/**
 * @brief TODO: Describe FullscreenBlit.
 */
class FullscreenBlit
{
  public:
    /**
     * @brief TODO: Describe FullscreenBlit.
     */
    FullscreenBlit();

    /**
     * @brief TODO: Describe Use.
     */
    void Use() const;
    /**
     * @brief TODO: Describe Update.
     *
     * @param rendering TODO: Describe this parameter.
     * @param source_texture TODO: Describe this parameter.
     * @param depth_texture TODO: Describe this parameter.
     */
    void Update(const RenderSystem &rendering, GLuint source_texture, GLuint depth_texture) const;

  private:
    /**
     * @brief TODO: Describe InitializeParameters.
     */
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

} // namespace CEngine::Renderer::OpenGL
#endif
