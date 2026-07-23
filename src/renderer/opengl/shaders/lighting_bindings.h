#ifndef LIGHTING_BINDINGS_H
#define LIGHTING_BINDINGS_H

#include "renderer/opengl/opengl_shadow_types.h"

#include <array>
#include <cstdint>

#include <glad/glad.h>

namespace CEngine::Renderer
{

class RenderSystem;

class OpenGLDirectLightBuffer
{
  public:
    OpenGLDirectLightBuffer() = default;
    OpenGLDirectLightBuffer(const OpenGLDirectLightBuffer &) = delete;
    OpenGLDirectLightBuffer &operator=(const OpenGLDirectLightBuffer &) = delete;
    ~OpenGLDirectLightBuffer();

    void Initialize(GLuint shader_id, const char *block_name);
    void BindAndUploadIfNeeded(RenderSystem &rendering);
    void Destroy();

  private:
    GLuint buffer_ = 0;
    uint64_t uploaded_revision_ = 0;
};

class OpenGLShadowBuffer
{
  public:
    OpenGLShadowBuffer() = default;
    OpenGLShadowBuffer(const OpenGLShadowBuffer &) = delete;
    OpenGLShadowBuffer &operator=(const OpenGLShadowBuffer &) = delete;
    ~OpenGLShadowBuffer();

    void Initialize(GLuint shader_id, const char *block_name);
    void Upload(const OpenGLShadowGpuData &data);
    void Destroy();

  private:
    GLuint buffer_ = 0;
    OpenGLShadowGpuData uploaded_data_{};
    bool uploaded_ = false;
};

struct OpenGLShadowSamplers
{
    GLint atlas = -1;
    std::array<GLint, OpenGLShadows::KMaxPointShadows> point_maps{};

    void Initialize(GLuint shader_id);
    void Bind(GLuint atlas_texture, const std::array<GLuint, OpenGLShadows::KMaxPointShadows> &point_textures) const;
};

struct OpenGLAmbientUniforms
{
    GLint sky_color = -1;
    GLint ground_color = -1;
    GLint intensity = -1;
    GLint enabled = -1;

    void Initialize(GLuint shader_id);
    void Upload(const RenderSystem &rendering) const;
};

struct OpenGLEnvironmentUniforms
{
    GLint irradiance = -1;
    GLint prefiltered = -1;
    GLint ibl_enabled = -1;
    GLint ibl_intensity = -1;
    GLint ibl_rotation = -1;
    GLint fog_enabled = -1;
    GLint fog_color = -1;
    GLint fog_density = -1;
    GLint fog_height_falloff = -1;
    GLint fog_base_height = -1;
    GLint fog_start_distance = -1;
    GLint fog_max_opacity = -1;
    GLint fog_cutoff_distance = -1;

    void Initialize(GLuint shader_id);
    void BindAndUpload(const RenderSystem &rendering, GLuint irradiance_texture, GLuint prefiltered_texture) const;
};

} // namespace CEngine::Renderer
#endif
