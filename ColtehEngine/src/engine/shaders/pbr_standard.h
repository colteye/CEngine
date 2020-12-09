#ifndef PBR_STANDARD_H
#define PBR_STANDARD_H

#include <vector>
#include "shader.h"
#include "../light.h"

class PBRStandard : public Shader
{
public:
	PBRStandard(std::string albedo_p, std::string metallic_roughness_ao_p, std::string normal_p);
protected:
	virtual void SetShaderFiles();
	virtual void InitializeParameters();
	virtual void SetParametersStatic();
	virtual void SetParametersDynamic();

	std::vector<Light*> *light_list;

	GLuint light_pos_id, light_col_id, light_pow_id;
	GLuint cam_pos_id;
	GLuint m_id, v_id, p_id;

	std::string albedo_path;
	std::string metallic_roughness_ao_path;
	std::string normal_path;
};

#endif