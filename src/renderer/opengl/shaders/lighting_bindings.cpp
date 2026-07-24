//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/renderer/opengl/shaders/lighting_bindings.cpp
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#include "lighting_bindings.h"

#include "renderer/light.h"
#include "renderer/opengl/shaders/texture_units.h"
#include "renderer/render_system.h"

#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <string>

namespace CEngine::Renderer::OpenGL
{

namespace
{
constexpr GLuint KDirectLightBindingPoint = 0;
constexpr GLuint KShadowBindingPoint = 1;

/**
 * @brief TODO: Describe SameShadowData.
 *
 * @param left TODO: Describe this parameter.
 * @param right TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool SameShadowData(const ShadowGpuData &left, const ShadowGpuData &right)
{
    return left.spot_matrices == right.spot_matrices && left.cascade_matrices == right.cascade_matrices &&
           left.spot_atlas_rects == right.spot_atlas_rects && left.spot_params == right.spot_params &&
           left.cascade_atlas_rects == right.cascade_atlas_rects && left.cascade_params == right.cascade_params &&
           left.point_params == right.point_params && left.point_shadow_params == right.point_shadow_params &&
           left.shadow_counts == right.shadow_counts;
}
} // namespace

/**
 * @brief TODO: Describe DirectLightBuffer::~DirectLightBuffer.
 */
DirectLightBuffer::~DirectLightBuffer()
{
    Destroy();
}

/**
 * @brief TODO: Describe DirectLightBuffer::Initialize.
 *
 * @param shader_id TODO: Describe this parameter.
 * @param block_name TODO: Describe this parameter.
 */
void DirectLightBuffer::Initialize(GLuint shader_id, const char *block_name)
{
    const GLuint block_index = glGetUniformBlockIndex(shader_id, block_name);
    if (block_index != GL_INVALID_INDEX)
    {
        glUniformBlockBinding(shader_id, block_index, KDirectLightBindingPoint);
    }

    glGenBuffers(1, &buffer_);
    glBindBuffer(GL_UNIFORM_BUFFER, buffer_);
    glBufferData(GL_UNIFORM_BUFFER,
                 static_cast<GLsizeiptr>(sizeof(glm::vec4) + (RenderSystem::MaxGpuLights * sizeof(GpuLight))), nullptr,
                 GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, KDirectLightBindingPoint, buffer_);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

/**
 * @brief TODO: Describe DirectLightBuffer::BindAndUploadIfNeeded.
 *
 * @param rendering TODO: Describe this parameter.
 */
void DirectLightBuffer::BindAndUploadIfNeeded(RenderSystem &rendering)
{
    glBindBufferBase(GL_UNIFORM_BUFFER, KDirectLightBindingPoint, buffer_);

    const uint64_t light_revision = rendering.GetLightRevision();
    if (uploaded_revision_ == light_revision)
    {
        return;
    }

    const std::vector<GpuLight> &gpu_lights = rendering.GetGpuLights();
    const glm::vec4 light_info(static_cast<float>(gpu_lights.size()), 0.0f, 0.0f, 0.0f);

    glBindBuffer(GL_UNIFORM_BUFFER, buffer_);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::vec4), glm::value_ptr(light_info));
    if (!gpu_lights.empty())
    {
        glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::vec4),
                        static_cast<GLsizeiptr>(gpu_lights.size() * sizeof(GpuLight)), gpu_lights.data());
    }
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    uploaded_revision_ = light_revision;
}

/**
 * @brief TODO: Describe DirectLightBuffer::Destroy.
 */
void DirectLightBuffer::Destroy()
{
    if (buffer_ != 0)
    {
        glDeleteBuffers(1, &buffer_);
        buffer_ = 0;
    }
    uploaded_revision_ = 0;
}

/**
 * @brief TODO: Describe ShadowBuffer::~ShadowBuffer.
 */
ShadowBuffer::~ShadowBuffer()
{
    Destroy();
}

/**
 * @brief TODO: Describe ShadowBuffer::Initialize.
 *
 * @param shader_id TODO: Describe this parameter.
 * @param block_name TODO: Describe this parameter.
 */
void ShadowBuffer::Initialize(GLuint shader_id, const char *block_name)
{
    const GLuint block_index = glGetUniformBlockIndex(shader_id, block_name);
    if (block_index != GL_INVALID_INDEX)
    {
        glUniformBlockBinding(shader_id, block_index, KShadowBindingPoint);
    }

    glGenBuffers(1, &buffer_);
    glBindBuffer(GL_UNIFORM_BUFFER, buffer_);
    glBufferData(GL_UNIFORM_BUFFER, static_cast<GLsizeiptr>(sizeof(ShadowGpuData)), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, KShadowBindingPoint, buffer_);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

/**
 * @brief TODO: Describe ShadowBuffer::Upload.
 *
 * @param data TODO: Describe this parameter.
 */
void ShadowBuffer::Upload(const ShadowGpuData &data)
{
    glBindBufferBase(GL_UNIFORM_BUFFER, KShadowBindingPoint, buffer_);
    if (uploaded_ && SameShadowData(uploaded_data_, data))
    {
        return;
    }
    glBindBuffer(GL_UNIFORM_BUFFER, buffer_);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, static_cast<GLsizeiptr>(sizeof(ShadowGpuData)), &data);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    uploaded_data_ = data;
    uploaded_ = true;
}

/**
 * @brief TODO: Describe ShadowBuffer::Destroy.
 */
void ShadowBuffer::Destroy()
{
    if (buffer_ != 0)
    {
        glDeleteBuffers(1, &buffer_);
        buffer_ = 0;
    }
    uploaded_data_ = {};
    uploaded_ = false;
}

/**
 * @brief TODO: Describe ShadowSamplers::Initialize.
 *
 * @param shader_id TODO: Describe this parameter.
 */
void ShadowSamplers::Initialize(GLuint shader_id)
{
    atlas = glGetUniformLocation(shader_id, "shadow_atlas");
    for (int index = 0; index < ShadowLimits::KMaxPointShadows; ++index)
    {
        const std::string name = "point_shadow_maps[" + std::to_string(index) + "]";
        point_maps[index] = glGetUniformLocation(shader_id, name.c_str());
    }
}

/**
 * @brief TODO: Describe ShadowSamplers::Bind.
 *
 * @param atlas_texture TODO: Describe this parameter.
 * @param point_textures TODO: Describe this parameter.
 */
void ShadowSamplers::Bind(GLuint atlas_texture,
                          const std::array<GLuint, ShadowLimits::KMaxPointShadows> &point_textures) const
{
    glActiveTexture(GL_TEXTURE0 + TextureUnits::ShadowAtlas);
    glBindTexture(GL_TEXTURE_2D, atlas_texture);
    glUniform1i(atlas, TextureUnits::ShadowAtlas);

    for (int index = 0; index < ShadowLimits::KMaxPointShadows; ++index)
    {
        glActiveTexture(GL_TEXTURE0 + TextureUnits::PointShadowFirst + index);
        glBindTexture(GL_TEXTURE_CUBE_MAP, point_textures[index]);
        glUniform1i(point_maps[index], TextureUnits::PointShadowFirst + index);
    }
}

/**
 * @brief TODO: Describe AmbientUniforms::Initialize.
 *
 * @param shader_id TODO: Describe this parameter.
 */
void AmbientUniforms::Initialize(GLuint shader_id)
{
    sky_color = glGetUniformLocation(shader_id, "ambient_sky_color");
    ground_color = glGetUniformLocation(shader_id, "ambient_ground_color");
    intensity = glGetUniformLocation(shader_id, "ambient_intensity");
    enabled = glGetUniformLocation(shader_id, "ambient_enabled");
}

/**
 * @brief TODO: Describe AmbientUniforms::Upload.
 *
 * @param rendering TODO: Describe this parameter.
 */
void AmbientUniforms::Upload(const RenderSystem &rendering) const
{
    const AmbientLighting &ambient = rendering.GetAmbientLighting();
    glUniform3fv(sky_color, 1, glm::value_ptr(ambient.sky_color));
    glUniform3fv(ground_color, 1, glm::value_ptr(ambient.ground_color));
    glUniform1f(intensity, ambient.intensity);
    glUniform1i(enabled, ambient.enabled ? 1 : 0);
}

/**
 * @brief TODO: Describe EnvironmentUniforms::Initialize.
 *
 * @param shader_id TODO: Describe this parameter.
 */
void EnvironmentUniforms::Initialize(GLuint shader_id)
{
    irradiance = glGetUniformLocation(shader_id, "ibl_irradiance");
    prefiltered = glGetUniformLocation(shader_id, "ibl_prefiltered");
    ibl_enabled = glGetUniformLocation(shader_id, "ibl_enabled");
    ibl_intensity = glGetUniformLocation(shader_id, "ibl_intensity");
    ibl_rotation = glGetUniformLocation(shader_id, "ibl_rotation_radians");
    fog_enabled = glGetUniformLocation(shader_id, "fog_enabled");
    fog_color = glGetUniformLocation(shader_id, "fog_inscattering_color");
    fog_density = glGetUniformLocation(shader_id, "fog_density");
    fog_height_falloff = glGetUniformLocation(shader_id, "fog_height_falloff");
    fog_base_height = glGetUniformLocation(shader_id, "fog_base_height");
    fog_start_distance = glGetUniformLocation(shader_id, "fog_start_distance");
    fog_max_opacity = glGetUniformLocation(shader_id, "fog_max_opacity");
    fog_cutoff_distance = glGetUniformLocation(shader_id, "fog_cutoff_distance");
}

/**
 * @brief TODO: Describe EnvironmentUniforms::BindAndUpload.
 *
 * @param rendering TODO: Describe this parameter.
 * @param irradiance_texture TODO: Describe this parameter.
 * @param prefiltered_texture TODO: Describe this parameter.
 */
void EnvironmentUniforms::BindAndUpload(const RenderSystem &rendering, GLuint irradiance_texture,
                                        GLuint prefiltered_texture) const
{
    glActiveTexture(GL_TEXTURE0 + TextureUnits::GlobalIrradiance);
    glBindTexture(GL_TEXTURE_CUBE_MAP, irradiance_texture);
    glUniform1i(irradiance, TextureUnits::GlobalIrradiance);
    glActiveTexture(GL_TEXTURE0 + TextureUnits::GlobalPrefiltered);
    glBindTexture(GL_TEXTURE_CUBE_MAP, prefiltered_texture);
    glUniform1i(prefiltered, TextureUnits::GlobalPrefiltered);

    const ImageBasedLighting &ibl = rendering.GetImageBasedLighting();
    glUniform1i(ibl_enabled, static_cast<GLint>(ibl.enabled && irradiance_texture != 0 && prefiltered_texture != 0));
    glUniform1f(ibl_intensity, ibl.lighting_intensity);
    glUniform1f(ibl_rotation, ibl.rotation_radians);

    const ExponentialHeightFog &fog = rendering.GetExponentialHeightFog();
    glUniform1i(fog_enabled, static_cast<GLint>(fog.enabled));
    glUniform3fv(fog_color, 1, glm::value_ptr(fog.inscattering_color));
    glUniform1f(fog_density, fog.density);
    glUniform1f(fog_height_falloff, fog.height_falloff);
    glUniform1f(fog_base_height, fog.base_height);
    glUniform1f(fog_start_distance, fog.start_distance);
    glUniform1f(fog_max_opacity, fog.max_opacity);
    glUniform1f(fog_cutoff_distance, fog.cutoff_distance);
}

void EnvironmentProbeUniforms::Initialize(GLuint shader_id)
{
    count = glGetUniformLocation(shader_id, "environment_probe_count");
    world_to_local = glGetUniformLocation(shader_id, "environment_probe_world_to_local[0]");
    local_to_world = glGetUniformLocation(shader_id, "environment_probe_local_to_world[0]");
    position_intensity = glGetUniformLocation(shader_id, "environment_probe_position_intensity[0]");
    extents_blend = glGetUniformLocation(shader_id, "environment_probe_extents_blend[0]");
    for (std::size_t index = 0; index < KMaxBoundEnvironmentProbes; ++index)
    {
        const std::string suffix = std::to_string(index);
        irradiance[index] = glGetUniformLocation(shader_id, ("environment_probe_irradiance_" + suffix).c_str());
        prefiltered[index] = glGetUniformLocation(shader_id, ("environment_probe_prefiltered_" + suffix).c_str());
    }
}

void EnvironmentProbeUniforms::BindAndUpload(std::span<const EnvironmentProbeBinding> probes) const
{
    const std::size_t probe_count = std::min(probes.size(), KMaxBoundEnvironmentProbes);
    std::array<glm::mat4, KMaxBoundEnvironmentProbes> world_to_local_values{};
    std::array<glm::mat4, KMaxBoundEnvironmentProbes> local_to_world_values{};
    std::array<glm::vec4, KMaxBoundEnvironmentProbes> position_intensity_values{};
    std::array<glm::vec4, KMaxBoundEnvironmentProbes> extents_blend_values{};

    for (std::size_t index = 0; index < KMaxBoundEnvironmentProbes; ++index)
    {
        const GLint irradiance_unit = TextureUnits::EnvironmentProbeFirst + static_cast<GLint>(index * 2u);
        const GLint prefiltered_unit = irradiance_unit + 1;
        const bool active = index < probe_count && probes[index].probe != nullptr;
        glActiveTexture(GL_TEXTURE0 + irradiance_unit);
        glBindTexture(GL_TEXTURE_CUBE_MAP, active ? probes[index].irradiance : 0);
        glUniform1i(irradiance[index], irradiance_unit);
        glActiveTexture(GL_TEXTURE0 + prefiltered_unit);
        glBindTexture(GL_TEXTURE_CUBE_MAP, active ? probes[index].prefiltered : 0);
        glUniform1i(prefiltered[index], prefiltered_unit);
        if (!active)
        {
            world_to_local_values[index] = glm::mat4(1.0f);
            local_to_world_values[index] = glm::mat4(1.0f);
            continue;
        }

        const EnvironmentProbe &probe = *probes[index].probe;
        local_to_world_values[index] = probe.transform;
        world_to_local_values[index] = glm::inverse(probe.transform);
        position_intensity_values[index] = glm::vec4(glm::vec3(probe.transform[3]), probe.intensity);
        extents_blend_values[index] =
            glm::vec4(glm::length(glm::vec3(probe.transform[0])), glm::length(glm::vec3(probe.transform[1])),
                      glm::length(glm::vec3(probe.transform[2])), probe.blend_distance);
    }

    glUniform1i(count, static_cast<GLint>(probe_count));
    glUniformMatrix4fv(world_to_local, static_cast<GLsizei>(KMaxBoundEnvironmentProbes), GL_FALSE,
                       glm::value_ptr(world_to_local_values[0]));
    glUniformMatrix4fv(local_to_world, static_cast<GLsizei>(KMaxBoundEnvironmentProbes), GL_FALSE,
                       glm::value_ptr(local_to_world_values[0]));
    glUniform4fv(position_intensity, static_cast<GLsizei>(KMaxBoundEnvironmentProbes),
                 glm::value_ptr(position_intensity_values[0]));
    glUniform4fv(extents_blend, static_cast<GLsizei>(KMaxBoundEnvironmentProbes),
                 glm::value_ptr(extents_blend_values[0]));
}

} // namespace CEngine::Renderer::OpenGL
