#include "shader_constants.h"
#include "pbr_standard.h"

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
	//light_pow_id = glGetUniformLocation(shader_id, "light_power");
	
	cam_pos_id = glGetUniformLocation(shader_id, "cam_pos_world");

	m_id = glGetUniformLocation(shader_id, "model");
	v_id = glGetUniformLocation(shader_id, "view");
	p_id = glGetUniformLocation(shader_id, "projection");
}

void PBRStandard::SetParametersStatic()
{
	glm::vec3 light_positions[4];
	light_positions[0] = glm::vec3(0.0f, 10.0f, 7.0f);
	light_positions[1] = glm::vec3(0.0f, 10.0f, -7.0f);
	light_positions[2] = glm::vec3(0.0f, 20.0f, 4.0f);
	light_positions[3] = glm::vec3(0.0f, 7.0f, 0.0f);

	glm::vec3 light_colors[4];
	light_colors[0] = glm::vec3(1.0f, 1.0f, 1.0f);
	light_colors[2] = glm::vec3(1.0f, 0.0f, 0.0f);
	light_colors[1] = glm::vec3(0.0f, 1.0f, 0.0f);
	light_colors[3] = glm::vec3(0.0f, 0.0f, 1.0f);

	glUniform3fv(light_pos_id, 4, glm::value_ptr(light_positions[0]));
	glUniform3fv(light_col_id, 4, glm::value_ptr(light_colors[0]));

	//glUniform3f(light_pos_id, 0.0f, 10.0f, 7.0f);
	//glUniform3f(light_col_id, 1.0f, 1.0f, 1.0f);
	//glUniform1f(light_pow_id, 100.0f);
}

void PBRStandard::SetParametersDynamic()
{
	glUniform3f(cam_pos_id, ShaderConstants::camera_position[0], ShaderConstants::camera_position[1], ShaderConstants::camera_position[2]);
	glUniformMatrix4fv(m_id, 1, GL_FALSE, &ShaderConstants::model[0][0]);
	glUniformMatrix4fv(v_id, 1, GL_FALSE, &ShaderConstants::view[0][0]);
	glUniformMatrix4fv(p_id, 1, GL_FALSE, &ShaderConstants::proj[0][0]);

}
