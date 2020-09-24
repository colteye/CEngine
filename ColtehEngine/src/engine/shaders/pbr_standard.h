#ifndef PBR_STANDARD_H
#define PBR_STANDARD_H

#include "shader.h"

class PBRStandard : public Shader
{
public:
	PBRStandard();
protected:
	virtual void SetShaderFiles();
	virtual void InitializeParameters();
	virtual void SetParametersStatic();
	virtual void SetParametersDynamic();

	GLuint light_pos_id, light_col_id;// , light_pow_id;
	GLuint cam_pos_id;
	GLuint m_id, v_id, p_id;
};

#endif