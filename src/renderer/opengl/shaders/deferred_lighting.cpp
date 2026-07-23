//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/renderer/opengl/shaders/deferred_lighting.cpp
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#include "deferred_lighting.h"

#include "renderer/render_system.h"

#include <glm/gtc/type_ptr.hpp>

namespace CEngine::Renderer::OpenGL
{

/**
 * @brief TODO: Describe DeferredLighting::DeferredLighting.
 */
DeferredLighting::DeferredLighting()
{
    shader_program_.Load("shaders/opengl/ssao.vert", "shaders/opengl/deferred_lighting.frag");
    InitializeParameters();
}

/**
 * @brief TODO: Describe DeferredLighting::Use.
 */
void DeferredLighting::Use() const
{
    shader_program_.Use();
}

/**
 * @brief TODO: Describe DeferredLighting::Update.
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
void DeferredLighting::Update(RenderSystem &rendering, GLuint albedo, GLuint normal_roughness, GLuint material,
                              GLuint baked_light, GLuint depth, int width, int height, const ShadowGpuData &shadow_data,
                              GLuint shadow_atlas,
                              const std::array<GLuint, ShadowLimits::KMaxPointShadows> &point_shadow_maps,
                              GLuint irradiance_map, GLuint prefiltered_map)
{
    const CameraFrameData &camera_frame_data = rendering.GetCameraFrameData();
    const glm::mat4 inverse_view = glm::inverse(camera_frame_data.view);
    const glm::mat4 inverse_projection = glm::inverse(camera_frame_data.proj);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, albedo);
    glUniform1i(albedo_id_, 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, normal_roughness);
    glUniform1i(normal_roughness_id_, 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, material);
    glUniform1i(material_id_, 2);

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, baked_light);
    glUniform1i(baked_light_id_, 3);

    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, depth);
    glUniform1i(depth_id_, 4);

    glUniformMatrix4fv(view_id_, 1, GL_FALSE, glm::value_ptr(camera_frame_data.view));
    glUniformMatrix4fv(projection_id_, 1, GL_FALSE, glm::value_ptr(camera_frame_data.proj));
    glUniformMatrix4fv(inverse_view_id_, 1, GL_FALSE, glm::value_ptr(inverse_view));
    glUniformMatrix4fv(inverse_projection_id_, 1, GL_FALSE, glm::value_ptr(inverse_projection));
    glUniform3fv(camera_position_id_, 1, glm::value_ptr(camera_frame_data.camera_position));
    glUniform2f(texel_size_id_, 1.0f / static_cast<float>(width), 1.0f / static_cast<float>(height));

    ambient_uniforms_.Upload(rendering);
    direct_lights_.BindAndUploadIfNeeded(rendering);
    shadow_buffer_.Upload(shadow_data);
    shadow_samplers_.Bind(shadow_atlas, point_shadow_maps);
    environment_uniforms_.BindAndUpload(rendering, irradiance_map, prefiltered_map);
}

/**
 * @brief TODO: Describe DeferredLighting::InitializeParameters.
 */
void DeferredLighting::InitializeParameters()
{
    const GLuint shader_id = shader_program_.GetId();
    direct_lights_.Initialize(shader_id, "DirectLightBlock");
    shadow_buffer_.Initialize(shader_id, "ShadowBlock");
    shadow_samplers_.Initialize(shader_id);
    ambient_uniforms_.Initialize(shader_id);
    environment_uniforms_.Initialize(shader_id);

    albedo_id_ = glGetUniformLocation(shader_id, "g_albedo");
    normal_roughness_id_ = glGetUniformLocation(shader_id, "g_normal_roughness");
    material_id_ = glGetUniformLocation(shader_id, "g_material");
    baked_light_id_ = glGetUniformLocation(shader_id, "g_baked_light");
    depth_id_ = glGetUniformLocation(shader_id, "g_depth");
    view_id_ = glGetUniformLocation(shader_id, "view");
    projection_id_ = glGetUniformLocation(shader_id, "projection");
    inverse_view_id_ = glGetUniformLocation(shader_id, "inverse_view");
    inverse_projection_id_ = glGetUniformLocation(shader_id, "inverse_projection");
    camera_position_id_ = glGetUniformLocation(shader_id, "cam_pos_world");
    texel_size_id_ = glGetUniformLocation(shader_id, "texel_size");
}

} // namespace CEngine::Renderer::OpenGL
