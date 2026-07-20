#include "fullscreen_blit.h"

FullscreenBlit::FullscreenBlit()
{
	shader_program.Load("shaders/opengl/ssao.vert", "shaders/opengl/fullscreen_blit.frag");
	InitializeParameters();
}

void FullscreenBlit::Use() const
{
	shader_program.Use();
}

void FullscreenBlit::Update(GLuint source_texture)
{
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, source_texture);
	glUniform1i(source_texture_id, 0);
}

void FullscreenBlit::InitializeParameters()
{
	source_texture_id = glGetUniformLocation(shader_program.GetId(), "source_texture");
}
