#include "ssao.h"

#include "renderer/render_system.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace CEngine::Renderer {

SSAO::SSAO()
	: render_tex(0),
	  depth_tex(0),
	  normal_roughness_tex(0),
	  texture_width(1),
	  texture_height(1)
{
    shader_program.Load("shaders/opengl/ssao.vert", "shaders/opengl/ssao.frag");
    InitializeParameters();
}

void SSAO::Use() const
{
    shader_program.Use();
}

void SSAO::Update()
{
    SetParametersStatic();
    SetParametersDynamic();
}

void SSAO::InitializeParameters()
{
    const GLuint shader_id = shader_program.GetId();
    render_id = glGetUniformLocation(shader_id, "render_tex");
    depth_id = glGetUniformLocation(shader_id, "depth_tex");
    normal_roughness_id = glGetUniformLocation(shader_id, "normal_roughness_tex");

    projection_id = glGetUniformLocation(shader_id, "projection");
    inverse_projection_id = glGetUniformLocation(shader_id, "inverse_projection");
    view_id = glGetUniformLocation(shader_id, "view");
    texel_size_id = glGetUniformLocation(shader_id, "texel_size");
}

void SSAO::SetTextures(GLuint render, GLuint depth, GLuint normal_roughness, int width, int height)
{
    render_tex = render;
    depth_tex = depth;
    normal_roughness_tex = normal_roughness;
    texture_width = width;
    texture_height = height;
}

void SSAO::SetParametersStatic()
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, render_tex);
    glUniform1i(render_id, 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, depth_tex);
    glUniform1i(depth_id, 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, normal_roughness_tex);
    glUniform1i(normal_roughness_id, 2);
}

void SSAO::SetParametersDynamic()
{
    const RenderFrameConstants& constants = RenderSystem::GetFrameConstants();
    const glm::mat4 inverse_projection = glm::inverse(constants.proj);

    glUniformMatrix4fv(projection_id, 1, GL_FALSE, glm::value_ptr(constants.proj));
    glUniformMatrix4fv(inverse_projection_id, 1, GL_FALSE, glm::value_ptr(inverse_projection));
    glUniformMatrix4fv(view_id, 1, GL_FALSE, glm::value_ptr(constants.view));
    glUniform2f(texel_size_id, 1.0f / texture_width, 1.0f / texture_height);
}

} // namespace CEngine::Renderer
