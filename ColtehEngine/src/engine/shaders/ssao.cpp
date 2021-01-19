#include "ssao.h"
#include "shader_constants.h"
#include "../render_system.h"
#include "../texture.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

SSAO::SSAO()
{
	Load();
	random_tex = Texture::LoadDDS("random.DDS");

}

void SSAO::SetShaderFiles()
{
	vertex_file_path = "SSAO_Vertex.vert";
	fragment_file_path = "SSAO_Fragment.frag";
}	

void SSAO::InitializeParameters()
{
	cam_pos_id = glGetUniformLocation(shader_id, "cam_pos_world");

	m_id = glGetUniformLocation(shader_id, "model");
	v_id = glGetUniformLocation(shader_id, "view");
	p_id = glGetUniformLocation(shader_id, "projection");

	render_id = glGetUniformLocation(shader_id, "render_tex");
	random_id = glGetUniformLocation(shader_id, "random_tex");
	depth_id = glGetUniformLocation(shader_id, "depth_tex");
}

void SSAO::SetTextures(GLuint render, GLuint depth)
{
	render_tex = render;
	depth_tex = depth;
}

void SSAO::SetParametersStatic()
{
	// Load textures into shader.
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, render_tex);
	glUniform1i(render_id, 0);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, depth_tex);
	glUniform1i(depth_id, 1);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, random_tex);
	glUniform1i(random_id, 2);
}

void SSAO::SetParametersDynamic()
{
	glUniform3f(cam_pos_id, ShaderConstants::camera_position[0], ShaderConstants::camera_position[1], ShaderConstants::camera_position[2]);
	glUniformMatrix4fv(m_id, 1, GL_FALSE, &ShaderConstants::model[0][0]);
	glUniformMatrix4fv(v_id, 1, GL_FALSE, &ShaderConstants::view[0][0]);
	glUniformMatrix4fv(p_id, 1, GL_FALSE, &ShaderConstants::proj[0][0]);

	// Load light data into shader.
	std::vector<glm::vec3> light_pos_list = RenderSystem::GetLightPositions();
	std::vector<glm::vec3> light_col_list = RenderSystem::GetLightColors();
	std::vector<float> light_pow_list = RenderSystem::GetLightPowers();
}
