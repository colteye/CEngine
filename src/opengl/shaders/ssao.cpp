#include "ssao.h"

#include "shader_constants.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

SSAO::SSAO()
	: render_tex(0),
	  depth_tex(0),
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

    projection_id = glGetUniformLocation(shader_id, "projection");
    inverse_projection_id = glGetUniformLocation(shader_id, "inverse_projection");
    texel_size_id = glGetUniformLocation(shader_id, "texel_size");
}

void SSAO::SetTextures(GLuint render, GLuint depth, int width, int height)
{
    render_tex = render;
    depth_tex = depth;
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
}

void SSAO::SetParametersDynamic()
{
    const glm::mat4 inverse_projection = glm::inverse(ShaderConstants::proj);

    glUniformMatrix4fv(projection_id, 1, GL_FALSE, glm::value_ptr(ShaderConstants::proj));
    glUniformMatrix4fv(inverse_projection_id, 1, GL_FALSE, glm::value_ptr(inverse_projection));
    glUniform2f(texel_size_id, 1.0f / texture_width, 1.0f / texture_height);
}
