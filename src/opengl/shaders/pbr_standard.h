#ifndef PBR_STANDARD_H
#define PBR_STANDARD_H

#include "shader.h"

class PBRStandard
{
public:

	GLuint albedo_tex;
	GLuint normal_tex;
	GLuint metallic_roughness_ao_tex;

	PBRStandard();
	void Use() const;
	void Update();
	void SetTextures(GLuint albedo, GLuint normal, GLuint metallic_roughness_ao);

private:
	void InitializeParameters();
	void SetParametersStatic();
	void SetParametersDynamic();

	ShaderProgram shader_program;
	GLuint light_pos_id, light_col_id, light_pow_id;
	GLuint cam_pos_id;
	GLuint m_id, v_id, p_id;
	GLuint albedo_id, normal_id, metallic_roughness_ao_id;

};

#endif
