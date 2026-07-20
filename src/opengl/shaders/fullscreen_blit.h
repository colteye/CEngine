#ifndef FULLSCREEN_BLIT_H
#define FULLSCREEN_BLIT_H

#include "shader.h"

class FullscreenBlit
{
public:
	FullscreenBlit();

	void Use() const;
	void Update(GLuint source_texture);

private:
	void InitializeParameters();

	ShaderProgram shader_program;
	GLint source_texture_id = -1;
};

#endif
