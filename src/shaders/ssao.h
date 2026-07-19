#ifndef SSAO_H
#define SSAO_H

#include "shader.h"

class SSAO : public Shader
{
public:

	SSAO();
	void SetTextures(GLuint render, GLuint depth, int width, int height);

protected:
	void SetShaderFiles() override;
	void InitializeParameters() override;
	void SetParametersStatic() override;
	void SetParametersDynamic() override;

	GLuint render_tex;
	GLuint depth_tex;
	GLuint render_id, depth_id;
	GLuint projection_id, inverse_projection_id, texel_size_id;

	int texture_width;
	int texture_height;

};

#endif
