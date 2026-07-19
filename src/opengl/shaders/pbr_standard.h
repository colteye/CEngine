#ifndef PBR_STANDARD_H
#define PBR_STANDARD_H

#include "shader.h"

#include <cstdint>

class PBRStandard
{
public:

	GLuint albedo_tex;
	GLuint normal_tex;
	GLuint metallic_roughness_ao_tex;

	PBRStandard();
	~PBRStandard();
	void Use() const;
	void Update();
	void SetTextures(GLuint albedo, GLuint normal, GLuint metallic_roughness_ao);

private:
	void InitializeParameters();
	void SetParametersStatic();
	void SetParametersDynamic();

	ShaderProgram shader_program;
	GLuint light_ubo = 0;
	uint64_t uploaded_light_revision = 0;
	GLuint cam_pos_id;
	GLuint m_id, v_id, p_id;
	GLuint albedo_id, normal_id, metallic_roughness_ao_id;

};

#endif
