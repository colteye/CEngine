#ifndef PBR_STANDARD_H
#define PBR_STANDARD_H

#include <vector>
#include "shader.h"
#include "../light.h"

class PBRStandard : public Shader
{
public:

	GLuint albedo_tex;
	GLuint normal_tex;
	GLuint metallic_roughness_ao_tex;

	PBRStandard();
	void SetTextures(GLuint albedo, GLuint normal, GLuint metallic_roughness_ao);

protected:
	virtual void SetShaderFiles();
	virtual void InitializeParameters();
	virtual void SetParametersStatic();
	virtual void SetParametersDynamic();

	std::vector<Light*> *light_list;

	GLuint light_pos_id, light_col_id, light_pow_id;
	GLuint cam_pos_id;
	GLuint m_id, v_id, p_id;
	GLuint albedo_id, normal_id, metallic_roughness_ao_id;

};

#endif