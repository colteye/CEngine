//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/renderer/opengl/shaders/deferred_lighting.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#ifndef CENGINE_RENDERER_OPENGL_SHADERS_DEFERRED_LIGHTING_H
#define CENGINE_RENDERER_OPENGL_SHADERS_DEFERRED_LIGHTING_H

#include "lighting_bindings.h"
#include "shader.h"

#include <glm/glm.hpp>

namespace CEngine::Renderer
{
class RenderSystem;
}

namespace CEngine::Renderer::OpenGL
{
/**
 * @brief TODO: Describe DeferredLighting.
 */
class DeferredLighting
{
  public:
    /**
     * @brief TODO: Describe DeferredLighting.
     */
    DeferredLighting();

    /**
     * @brief TODO: Describe Use.
     */
    void Use() const;
    /**
     * @brief TODO: Describe Update.
     *
     * @param rendering TODO: Describe this parameter.
     * @param albedo TODO: Describe this parameter.
     * @param normal_roughness TODO: Describe this parameter.
     * @param material TODO: Describe this parameter.
     * @param baked_light TODO: Describe this parameter.
     * @param depth TODO: Describe this parameter.
     * @param width TODO: Describe this parameter.
     * @param height TODO: Describe this parameter.
     * @param shadow_data TODO: Describe this parameter.
     * @param shadow_atlas TODO: Describe this parameter.
     * @param point_shadow_maps TODO: Describe this parameter.
     * @param irradiance_map TODO: Describe this parameter.
     * @param prefiltered_map TODO: Describe this parameter.
     */
    void Update(RenderSystem &rendering, GLuint albedo, GLuint normal_roughness, GLuint material, GLuint baked_light,
                GLuint depth, int width, int height, const ShadowGpuData &shadow_data, GLuint shadow_atlas,
                const std::array<GLuint, ShadowLimits::KMaxPointShadows> &point_shadow_maps, GLuint irradiance_map,
                GLuint prefiltered_map);

  private:
    /**
     * @brief TODO: Describe InitializeParameters.
     */
    void InitializeParameters();

    ShaderProgram shader_program_;
    DirectLightBuffer direct_lights_;
    ShadowBuffer shadow_buffer_;
    ShadowSamplers shadow_samplers_;
    AmbientUniforms ambient_uniforms_;
    EnvironmentUniforms environment_uniforms_;

    GLint albedo_id_ = -1;
    GLint normal_roughness_id_ = -1;
    GLint material_id_ = -1;
    GLint baked_light_id_ = -1;
    GLint depth_id_ = -1;
    GLint view_id_ = -1;
    GLint projection_id_ = -1;
    GLint inverse_view_id_ = -1;
    GLint inverse_projection_id_ = -1;
    GLint camera_position_id_ = -1;
    GLint texel_size_id_ = -1;
};

} // namespace CEngine::Renderer::OpenGL
#endif
