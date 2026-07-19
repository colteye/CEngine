#ifndef SSAO_H
#define SSAO_H

#include "shader.h"

class SSAO
{
public:

	SSAO();
	void Use() const;
	void Update();
	void SetTextures(GLuint render, GLuint depth, int width, int height);

private:
	void InitializeParameters();
	void SetParametersStatic();
	void SetParametersDynamic();

	ShaderProgram shader_program;
	GLuint render_tex;
	GLuint depth_tex;
	GLuint render_id, depth_id;
	GLuint projection_id, inverse_projection_id, texel_size_id;

	int texture_width;
	int texture_height;

};

#endif
