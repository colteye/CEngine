#include "depth_only.h"

#include <glm/gtc/type_ptr.hpp>

namespace CEngine::Renderer
{

DepthOnly::DepthOnly()
{
    shader_program_.Load("shaders/opengl/depth_only.vert", "shaders/opengl/depth_only.frag");
    InitializeParameters();
}

void DepthOnly::Use() const
{
    shader_program_.Use();
}

void DepthOnly::UpdateFrame(const glm::mat4 &view, const glm::mat4 &projection) const
{
    glUniformMatrix4fv(view_id_, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(projection_id_, 1, GL_FALSE, glm::value_ptr(projection));
}

void DepthOnly::UpdateObject(const glm::mat4 &model, const Material &material, GLuint albedo_texture) const
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

void DepthOnly::InitializeParameters()
{
    const GLuint shader_id = shader_program_.GetId();
    model_id_ = glGetUniformLocation(shader_id, "model");
    view_id_ = glGetUniformLocation(shader_id, "view");
    projection_id_ = glGetUniformLocation(shader_id, "projection");
    albedo_id_ = glGetUniformLocation(shader_id, "albedo");
    base_color_factor_id_ = glGetUniformLocation(shader_id, "base_color_factor");
    alpha_cutoff_id_ = glGetUniformLocation(shader_id, "alpha_cutoff");
    alpha_test_id_ = glGetUniformLocation(shader_id, "alpha_test");
}

} // namespace CEngine::Renderer
