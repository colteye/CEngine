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
class DeferredLighting
{
  public:
    DeferredLighting();

    void Use() const;
    void Update(RenderSystem &rendering, GLuint albedo, GLuint normal_roughness, GLuint material, GLuint baked_light,
                GLuint depth, int width, int height, const ShadowGpuData &shadow_data, GLuint shadow_atlas,
                const std::array<GLuint, ShadowLimits::KMaxPointShadows> &point_shadow_maps, GLuint irradiance_map,
                GLuint prefiltered_map);

  private:
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
