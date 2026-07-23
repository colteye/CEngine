//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/renderer/opengl/shaders/ssao.cpp
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#include "ssao.h"

#include "renderer/render_system.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace CEngine::Renderer::OpenGL
{

/**
 * @brief TODO: Describe SSAO::SSAO.
 */
SSAO::SSAO()
{
    compute_program_.Load("shaders/opengl/ssao.vert", "shaders/opengl/ssao.frag");
    composite_program_.Load("shaders/opengl/ssao.vert", "shaders/opengl/ssao_composite.frag");
    InitializeParameters();
}

/**
 * @brief TODO: Describe SSAO::UseCompute.
 */
void SSAO::UseCompute() const
{
    compute_program_.Use();
}

/**
 * @brief TODO: Describe SSAO::UpdateCompute.
 *
 * @param rendering TODO: Describe this parameter.
 */
void SSAO::UpdateCompute(const RenderSystem &rendering) const
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, depth_tex_);
    glUniform1i(depth_id_, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, normal_roughness_tex_);
    glUniform1i(normal_roughness_id_, 1);

    const CameraFrameData &camera_frame_data = rendering.GetCameraFrameData();
    const glm::mat4 inverse_projection = glm::inverse(camera_frame_data.proj);
    glUniformMatrix4fv(projection_id_, 1, GL_FALSE, glm::value_ptr(camera_frame_data.proj));
    glUniformMatrix4fv(inverse_projection_id_, 1, GL_FALSE, glm::value_ptr(inverse_projection));
    glUniformMatrix4fv(view_id_, 1, GL_FALSE, glm::value_ptr(camera_frame_data.view));
    const SSAOSettings &settings = rendering.GetSSAOSettings();
    glUniform1f(radius_id_, settings.radius);
    glUniform1f(bias_id_, settings.bias);
    glUniform1f(intensity_id_, settings.enabled ? settings.intensity : 0.0f);
    glUniform1f(contrast_id_, settings.contrast);
}

/**
 * @brief TODO: Describe SSAO::UseComposite.
 */
void SSAO::UseComposite() const
{
    composite_program_.Use();
}

/**
 * @brief TODO: Describe SSAO::UpdateComposite.
 *
 * @param rendering TODO: Describe this parameter.
 */
void SSAO::UpdateComposite(const RenderSystem &rendering) const
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, render_tex_);
    glUniform1i(composite_render_id_, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, depth_tex_);
    glUniform1i(composite_depth_id_, 1);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, ao_tex_);
    glUniform1i(composite_ao_id_, 2);
    glUniform2f(texel_size_id_, 1.0f / static_cast<float>(texture_width_), 1.0f / static_cast<float>(texture_height_));
    const CameraFrameData &camera_frame_data = rendering.GetCameraFrameData();
    const glm::mat4 inverse_projection = glm::inverse(camera_frame_data.proj);
    glUniformMatrix4fv(composite_inverse_projection_id_, 1, GL_FALSE, glm::value_ptr(inverse_projection));
    const ExponentialHeightFog &fog = rendering.GetExponentialHeightFog();
    glUniform1i(fog_enabled_id_, static_cast<GLint>(fog.enabled));
    glUniform1f(fog_density_id_, fog.density);
    glUniform1f(fog_start_distance_id_, fog.start_distance);
}

/**
 * @brief TODO: Describe SSAO::InitializeParameters.
 */
void SSAO::InitializeParameters()
{
    const GLuint compute_id = compute_program_.GetId();
    depth_id_ = glGetUniformLocation(compute_id, "depth_tex");
    normal_roughness_id_ = glGetUniformLocation(compute_id, "normal_roughness_tex");
    projection_id_ = glGetUniformLocation(compute_id, "projection");
    inverse_projection_id_ = glGetUniformLocation(compute_id, "inverse_projection");
    view_id_ = glGetUniformLocation(compute_id, "view");
    radius_id_ = glGetUniformLocation(compute_id, "radius");
    bias_id_ = glGetUniformLocation(compute_id, "bias");
    intensity_id_ = glGetUniformLocation(compute_id, "intensity");
    contrast_id_ = glGetUniformLocation(compute_id, "contrast");

    const GLuint composite_id = composite_program_.GetId();
    composite_render_id_ = glGetUniformLocation(composite_id, "render_tex");
    composite_depth_id_ = glGetUniformLocation(composite_id, "depth_tex");
    composite_ao_id_ = glGetUniformLocation(composite_id, "ao_tex");
    texel_size_id_ = glGetUniformLocation(composite_id, "texel_size");
    composite_inverse_projection_id_ = glGetUniformLocation(composite_id, "inverse_projection");
    fog_enabled_id_ = glGetUniformLocation(composite_id, "fog_enabled");
    fog_density_id_ = glGetUniformLocation(composite_id, "fog_density");
    fog_start_distance_id_ = glGetUniformLocation(composite_id, "fog_start_distance");
}

/**
 * @brief TODO: Describe SSAO::SetTextures.
 *
 * @param render TODO: Describe this parameter.
 * @param depth TODO: Describe this parameter.
 * @param normal_roughness TODO: Describe this parameter.
 * @param ao TODO: Describe this parameter.
 * @param width TODO: Describe this parameter.
 * @param height TODO: Describe this parameter.
 */
void SSAO::SetTextures(GLuint render, GLuint depth, GLuint normal_roughness, GLuint ao, int width, int height)
{
    render_tex_ = render;
    depth_tex_ = depth;
    normal_roughness_tex_ = normal_roughness;
    ao_tex_ = ao;
    texture_width_ = width;
    texture_height_ = height;
}

} // namespace CEngine::Renderer::OpenGL
