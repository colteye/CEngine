//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/renderer/opengl/shaders/lighting_bindings.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#ifndef CENGINE_RENDERER_OPENGL_SHADERS_LIGHTING_BINDINGS_H
#define CENGINE_RENDERER_OPENGL_SHADERS_LIGHTING_BINDINGS_H

#include "renderer/environment_probe.h"
#include "renderer/opengl/shadow_types.h"

#include <array>
#include <cstdint>
#include <span>

#include <glad/glad.h>

namespace CEngine::Renderer
{
class RenderSystem;
}

namespace CEngine::Renderer::OpenGL
{
/**
 * @brief TODO: Describe DirectLightBuffer.
 */
class DirectLightBuffer
{
  public:
    /**
     * @brief TODO: Describe DirectLightBuffer.
     */
    DirectLightBuffer() = default;
    /**
     * @brief TODO: Describe DirectLightBuffer.
     */
    DirectLightBuffer(const DirectLightBuffer &) = delete;
    /**
     * @brief TODO: Describe operator=.
     *
     * @return TODO: Describe the return value.
     */
    DirectLightBuffer &operator=(const DirectLightBuffer &) = delete;
    /**
     * @brief TODO: Describe DirectLightBuffer.
     */
    DirectLightBuffer(DirectLightBuffer &&) = delete;
    /**
     * @brief TODO: Describe operator=.
     *
     * @return TODO: Describe the return value.
     */
    DirectLightBuffer &operator=(DirectLightBuffer &&) = delete;
    /**
     * @brief TODO: Describe ~DirectLightBuffer.
     */
    ~DirectLightBuffer();

    /**
     * @brief TODO: Describe Initialize.
     *
     * @param shader_id TODO: Describe this parameter.
     * @param block_name TODO: Describe this parameter.
     */
    void Initialize(GLuint shader_id, const char *block_name);
    /**
     * @brief TODO: Describe BindAndUploadIfNeeded.
     *
     * @param rendering TODO: Describe this parameter.
     */
    void BindAndUploadIfNeeded(RenderSystem &rendering);
    /**
     * @brief TODO: Describe Destroy.
     */
    void Destroy();

  private:
    GLuint buffer_ = 0;
    uint64_t uploaded_revision_ = 0;
};

/**
 * @brief TODO: Describe ShadowBuffer.
 */
class ShadowBuffer
{
  public:
    /**
     * @brief TODO: Describe ShadowBuffer.
     */
    ShadowBuffer() = default;
    /**
     * @brief TODO: Describe ShadowBuffer.
     */
    ShadowBuffer(const ShadowBuffer &) = delete;
    /**
     * @brief TODO: Describe operator=.
     *
     * @return TODO: Describe the return value.
     */
    ShadowBuffer &operator=(const ShadowBuffer &) = delete;
    /**
     * @brief TODO: Describe ShadowBuffer.
     */
    ShadowBuffer(ShadowBuffer &&) = delete;
    /**
     * @brief TODO: Describe operator=.
     *
     * @return TODO: Describe the return value.
     */
    ShadowBuffer &operator=(ShadowBuffer &&) = delete;
    /**
     * @brief TODO: Describe ~ShadowBuffer.
     */
    ~ShadowBuffer();

    /**
     * @brief TODO: Describe Initialize.
     *
     * @param shader_id TODO: Describe this parameter.
     * @param block_name TODO: Describe this parameter.
     */
    void Initialize(GLuint shader_id, const char *block_name);
    /**
     * @brief TODO: Describe Upload.
     *
     * @param data TODO: Describe this parameter.
     */
    void Upload(const ShadowGpuData &data);
    /**
     * @brief TODO: Describe Destroy.
     */
    void Destroy();

  private:
    GLuint buffer_ = 0;
    ShadowGpuData uploaded_data_{};
    bool uploaded_ = false;
};

/**
 * @brief TODO: Describe ShadowSamplers.
 */
struct ShadowSamplers
{
    GLint atlas = -1;
    std::array<GLint, ShadowLimits::KMaxPointShadows> point_maps{};

    /**
     * @brief TODO: Describe Initialize.
     *
     * @param shader_id TODO: Describe this parameter.
     */
    void Initialize(GLuint shader_id);
    /**
     * @brief TODO: Describe Bind.
     *
     * @param atlas_texture TODO: Describe this parameter.
     * @param point_textures TODO: Describe this parameter.
     */
    void Bind(GLuint atlas_texture, const std::array<GLuint, ShadowLimits::KMaxPointShadows> &point_textures) const;
};

/**
 * @brief TODO: Describe AmbientUniforms.
 */
struct AmbientUniforms
{
    GLint sky_color = -1;
    GLint ground_color = -1;
    GLint intensity = -1;
    GLint enabled = -1;

    /**
     * @brief TODO: Describe Initialize.
     *
     * @param shader_id TODO: Describe this parameter.
     */
    void Initialize(GLuint shader_id);
    /**
     * @brief TODO: Describe Upload.
     *
     * @param rendering TODO: Describe this parameter.
     */
    void Upload(const RenderSystem &rendering) const;
};

/**
 * @brief TODO: Describe EnvironmentUniforms.
 */
struct EnvironmentUniforms
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

    /**
     * @brief TODO: Describe Initialize.
     *
     * @param shader_id TODO: Describe this parameter.
     */
    void Initialize(GLuint shader_id);
    /**
     * @brief TODO: Describe BindAndUpload.
     *
     * @param rendering TODO: Describe this parameter.
     * @param irradiance_texture TODO: Describe this parameter.
     * @param prefiltered_texture TODO: Describe this parameter.
     */
    void BindAndUpload(const RenderSystem &rendering, GLuint irradiance_texture, GLuint prefiltered_texture) const;
};

inline constexpr std::size_t KMaxBoundEnvironmentProbes = 2;

struct EnvironmentProbeBinding
{
    const EnvironmentProbe *probe = nullptr;
    GLuint irradiance = 0;
    GLuint prefiltered = 0;
};

struct EnvironmentProbeUniforms
{
    GLint count = -1;
    GLint world_to_local = -1;
    GLint local_to_world = -1;
    GLint position_intensity = -1;
    GLint extents_blend = -1;
    std::array<GLint, KMaxBoundEnvironmentProbes> irradiance{};
    std::array<GLint, KMaxBoundEnvironmentProbes> prefiltered{};

    void Initialize(GLuint shader_id);
    void BindAndUpload(std::span<const EnvironmentProbeBinding> probes) const;
};

} // namespace CEngine::Renderer::OpenGL
#endif
