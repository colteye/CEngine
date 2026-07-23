#include "lighting_bindings.h"

#include "renderer/light.h"
#include "renderer/render_system.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cstring>
#include <string>

namespace CEngine::Renderer
{

namespace
{
constexpr GLuint KDirectLightBindingPoint = 0;
constexpr GLuint KShadowBindingPoint = 1;
constexpr GLint KShadowAtlasTextureUnit = 5;
constexpr GLint KPointShadowFirstTextureUnit = 6;
} // namespace

OpenGLDirectLightBuffer::~OpenGLDirectLightBuffer()
{
    Destroy();
}

void OpenGLDirectLightBuffer::Initialize(GLuint shader_id, const char *block_name)
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

void OpenGLDirectLightBuffer::BindAndUploadIfNeeded(RenderSystem &rendering)
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

void OpenGLDirectLightBuffer::Destroy()
{
    if (buffer_ != 0)
    {
        glDeleteBuffers(1, &buffer_);
        buffer_ = 0;
    }
    uploaded_revision_ = 0;
}

OpenGLShadowBuffer::~OpenGLShadowBuffer()
{
    Destroy();
}

void OpenGLShadowBuffer::Initialize(GLuint shader_id, const char *block_name)
{
    const GLuint block_index = glGetUniformBlockIndex(shader_id, block_name);
    if (block_index != GL_INVALID_INDEX)
    {
        glUniformBlockBinding(shader_id, block_index, KShadowBindingPoint);
    }

    glGenBuffers(1, &buffer_);
    glBindBuffer(GL_UNIFORM_BUFFER, buffer_);
    glBufferData(GL_UNIFORM_BUFFER, static_cast<GLsizeiptr>(sizeof(OpenGLShadowGpuData)), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, KShadowBindingPoint, buffer_);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void OpenGLShadowBuffer::Upload(const OpenGLShadowGpuData &data)
{
    glBindBufferBase(GL_UNIFORM_BUFFER, KShadowBindingPoint, buffer_);
    if (uploaded_ && std::memcmp(&uploaded_data_, &data, sizeof(data)) == 0)
    {
        return;
    }
    glBindBuffer(GL_UNIFORM_BUFFER, buffer_);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, static_cast<GLsizeiptr>(sizeof(OpenGLShadowGpuData)), &data);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    uploaded_data_ = data;
    uploaded_ = true;
}

void OpenGLShadowBuffer::Destroy()
{
    if (buffer_ != 0)
    {
        glDeleteBuffers(1, &buffer_);
        buffer_ = 0;
    }
    uploaded_data_ = {};
    uploaded_ = false;
}

void OpenGLShadowSamplers::Initialize(GLuint shader_id)
{
    atlas = glGetUniformLocation(shader_id, "shadow_atlas");
    for (int index = 0; index < OpenGLShadows::KMaxPointShadows; ++index)
    {
        const std::string name = "point_shadow_maps[" + std::to_string(index) + "]";
        point_maps[index] = glGetUniformLocation(shader_id, name.c_str());
    }
}

void OpenGLShadowSamplers::Bind(GLuint atlas_texture,
                                const std::array<GLuint, OpenGLShadows::KMaxPointShadows> &point_textures) const
{
    glActiveTexture(GL_TEXTURE0 + KShadowAtlasTextureUnit);
    glBindTexture(GL_TEXTURE_2D, atlas_texture);
    glUniform1i(atlas, KShadowAtlasTextureUnit);

    for (int index = 0; index < OpenGLShadows::KMaxPointShadows; ++index)
    {
        glActiveTexture(GL_TEXTURE0 + KPointShadowFirstTextureUnit + index);
        glBindTexture(GL_TEXTURE_CUBE_MAP, point_textures[index]);
        glUniform1i(point_maps[index], KPointShadowFirstTextureUnit + index);
    }
}

void OpenGLAmbientUniforms::Initialize(GLuint shader_id)
{
    sky_color = glGetUniformLocation(shader_id, "ambient_sky_color");
    ground_color = glGetUniformLocation(shader_id, "ambient_ground_color");
    intensity = glGetUniformLocation(shader_id, "ambient_intensity");
    enabled = glGetUniformLocation(shader_id, "ambient_enabled");
}

void OpenGLAmbientUniforms::Upload(const RenderSystem &rendering) const
{
    const AmbientLighting &ambient = rendering.GetAmbientLighting();
    glUniform3fv(sky_color, 1, glm::value_ptr(ambient.sky_color));
    glUniform3fv(ground_color, 1, glm::value_ptr(ambient.ground_color));
    glUniform1f(intensity, ambient.intensity);
    glUniform1i(enabled, ambient.enabled ? 1 : 0);
}

void OpenGLEnvironmentUniforms::Initialize(GLuint shader_id)
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

void OpenGLEnvironmentUniforms::BindAndUpload(const RenderSystem &rendering, GLuint irradiance_texture,
                                              GLuint prefiltered_texture) const
{
    constexpr GLint KIrradianceTextureUnit = 14;
    constexpr GLint KPrefilteredTextureUnit = 15;
    glActiveTexture(GL_TEXTURE0 + KIrradianceTextureUnit);
    glBindTexture(GL_TEXTURE_CUBE_MAP, irradiance_texture);
    glUniform1i(irradiance, KIrradianceTextureUnit);
    glActiveTexture(GL_TEXTURE0 + KPrefilteredTextureUnit);
    glBindTexture(GL_TEXTURE_CUBE_MAP, prefiltered_texture);
    glUniform1i(prefiltered, KPrefilteredTextureUnit);

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

} // namespace CEngine::Renderer
