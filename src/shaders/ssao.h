#ifndef SSAO_H
#define SSAO_H

#include <vector>
#include "shader.h"
#include "../light.h"

class SSAO : public Shader
{
public:

	GLuint random_tex;
	GLuint render_tex;
	GLuint depth_tex;

	SSAO();
	void SetTextures(GLuint render, GLuint depth);

protected:
	virtual void SetShaderFiles();
	virtual void InitializeParameters();
	virtual void SetParametersStatic();
	virtual void SetParametersDynamic();

	GLuint cam_pos_id;
	GLuint m_id, v_id, p_id;
	GLuint render_id, depth_id, random_id;

};

#endif