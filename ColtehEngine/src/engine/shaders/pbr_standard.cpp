#include "pbr_standard.h"
#include "shader_constants.h"
#include "../render_system.h"
#include "../texture.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

PBRStandard::PBRStandard()
{
	Load();
}

void PBRStandard::SetShaderFiles()
{
	vertex_file_path = "PBR_Standard_Vertex.vert";
	fragment_file_path = "PBR_Standard_Fragment.frag";
}

void PBRStandard::InitializeParameters()
{
	light_pos_id = glGetUniformLocation(shader_id, "light_positions");
	light_col_id = glGetUniformLocation(shader_id, "light_colors");
	light_pow_id = glGetUniformLocation(shader_id, "light_powers");
	
	cam_pos_id = glGetUniformLocation(shader_id, "cam_pos_world");

	m_id = glGetUniformLocation(shader_id, "model");
	v_id = glGetUniformLocation(shader_id, "view");
	p_id = glGetUniformLocation(shader_id, "projection");

	albedo_id = glGetUniformLocation(shader_id, "albedo");
	normal_id = glGetUniformLocation(shader_id, "normal");
	metallic_roughness_ao_id = glGetUniformLocation(shader_id, "metallic_roughness_ao");
}

void PBRStandard::SetTextures(GLuint albedo, GLuint normal, GLuint metallic_roughness_ao)
{
	albedo_tex = albedo;
	normal_tex = normal;
	metallic_roughness_ao_tex = metallic_roughness_ao;
}

void PBRStandard::SetParametersStatic()
{
	// Load textures into shader.
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, albedo_tex);
	glUniform1i(albedo_id, 0);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, normal_tex);
	glUniform1i(normal_id, 1);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, metallic_roughness_ao_tex);
	glUniform1i(metallic_roughness_ao_id, 2);
}

void PBRStandard::SetParametersDynamic()
{
	glUniform3f(cam_pos_id, ShaderConstants::camera_position[0], ShaderConstants::camera_position[1], ShaderConstants::camera_position[2]);
	glUniformMatrix4fv(m_id, 1, GL_FALSE, &ShaderConstants::model[0][0]);
	glUniformMatrix4fv(v_id, 1, GL_FALSE, &ShaderConstants::view[0][0]);
	glUniformMatrix4fv(p_id, 1, GL_FALSE, &ShaderConstants::proj[0][0]);

	// Load light data into shader.
	std::vector<glm::vec3> light_pos_list = RenderSystem::GetLightPositions();
	std::vector<glm::vec3> light_col_list = RenderSystem::GetLightColors();
	std::vector<float> light_pow_list = RenderSystem::GetLightPowers();

	glUniform3fv(light_pos_id, light_pos_list.size(), glm::value_ptr(light_pos_list[0]));
	glUniform3fv(light_col_id, light_col_list.size(), glm::value_ptr(light_col_list[0]));
	glUniform1fv(light_pow_id, light_pow_list.size(), &light_pow_list[0]);
}
