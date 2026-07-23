#include "point_shadow_depth.h"

#include <glm/gtc/type_ptr.hpp>

namespace CEngine::Renderer::OpenGL
{

PointShadowDepth::PointShadowDepth()
{
    shader_program_.Load("shaders/opengl/point_shadow_depth.vert", "shaders/opengl/point_shadow_depth.frag");
    InitializeParameters();
}

void PointShadowDepth::Use() const
{
    shader_program_.Use();
}

void PointShadowDepth::UpdateFrame(const glm::mat4 &view, const glm::mat4 &projection, const glm::vec3 &light_position,
                                   float far_plane) const
{
    glUniformMatrix4fv(view_id_, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(projection_id_, 1, GL_FALSE, glm::value_ptr(projection));
    glUniform3fv(light_position_id_, 1, glm::value_ptr(light_position));
    glUniform1f(far_plane_id_, far_plane);
}

void PointShadowDepth::UpdateObject(const glm::mat4 &model, const Material &material, GLuint albedo_texture) const
{
    const bool alpha_test = RequiresAlphaTest(material.render_mode);

    glUniformMatrix4fv(model_id_, 1, GL_FALSE, glm::value_ptr(model));
    glUniform4fv(base_color_factor_id_, 1, glm::value_ptr(material.base_color_factor));
    glUniform1f(alpha_cutoff_id_, material.alpha_cutoff);
    glUniform1i(alpha_test_id_, alpha_test ? 1 : 0);

    if (alpha_test)
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, albedo_texture);
        glUniform1i(albedo_id_, 0);
    }
}

void PointShadowDepth::InitializeParameters()
{
    const GLuint shader_id = shader_program_.GetId();
    model_id_ = glGetUniformLocation(shader_id, "model");
    view_id_ = glGetUniformLocation(shader_id, "view");
    projection_id_ = glGetUniformLocation(shader_id, "projection");
    light_position_id_ = glGetUniformLocation(shader_id, "light_position");
    far_plane_id_ = glGetUniformLocation(shader_id, "far_plane");
    albedo_id_ = glGetUniformLocation(shader_id, "albedo");
    base_color_factor_id_ = glGetUniformLocation(shader_id, "base_color_factor");
    alpha_cutoff_id_ = glGetUniformLocation(shader_id, "alpha_cutoff");
    alpha_test_id_ = glGetUniformLocation(shader_id, "alpha_test");
}

} // namespace CEngine::Renderer::OpenGL
