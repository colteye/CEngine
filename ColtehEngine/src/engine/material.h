#ifndef MATERIAL_H
#define MATERIAL_H

#include <string>
#include <glad/glad.h>
#include "shaders/Shader.h"
#include "shaders/pbr_standard.h"

class Material
{
public:
	
	GLuint albedo_tex;
	GLuint normal_tex;
	GLuint metallic_roughness_ao_tex;

	Material(PBRStandard * in_shader, std::string albedo_p, std::string normal_p, std::string metallic_roughness_ao_p);

	void UpdateShader();

	std::string material_name;
	PBRStandard *shader;
};
#endif