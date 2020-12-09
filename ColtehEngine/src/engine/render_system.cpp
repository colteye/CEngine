#include "render_system.h"

#define MAX_BUFFER_SIZE 100000000

GLuint RenderSystem::vertex_buffer;
GLuint RenderSystem::uv_buffer;
GLuint RenderSystem::normal_buffer;
GLuint RenderSystem::tangent_buffer;

size_t RenderSystem::vertex_buffer_length = 0;
size_t RenderSystem::uv_buffer_length = 0;
size_t RenderSystem::normal_buffer_length = 0;
size_t RenderSystem::tangent_buffer_length = 0;

std::vector<Model*> RenderSystem::model_list;
std::vector<Shader*> RenderSystem::shader_list;

std::vector<glm::vec3> RenderSystem::light_pos_list;
std::vector<glm::vec3> RenderSystem::light_col_list;
std::vector<float> RenderSystem::light_pow_list;

void RenderSystem::Initialize()
{
	GLuint VertexArrayID;
	glGenVertexArrays(1, &VertexArrayID);
	glBindVertexArray(VertexArrayID);
	
	// Initialize our buffers.
	glGenBuffers(1, &vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, MAX_BUFFER_SIZE, nullptr, GL_STATIC_DRAW);

	glGenBuffers(1, &uv_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, uv_buffer);
	glBufferData(GL_ARRAY_BUFFER, MAX_BUFFER_SIZE, nullptr, GL_STATIC_DRAW);

	glGenBuffers(1, &normal_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, normal_buffer);
	glBufferData(GL_ARRAY_BUFFER, MAX_BUFFER_SIZE, nullptr, GL_STATIC_DRAW);

	glGenBuffers(1, &tangent_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, tangent_buffer);
	glBufferData(GL_ARRAY_BUFFER, MAX_BUFFER_SIZE, nullptr, GL_STATIC_DRAW);

	// Enable depth test
	glEnable(GL_DEPTH_TEST);
	// Accept fragment if it closer to the camera than the former one
	glDepthFunc(GL_LESS);

	glEnable(GL_CULL_FACE);

	vertex_buffer_length = 0;
	uv_buffer_length = 0;
	normal_buffer_length = 0;
	tangent_buffer_length = 0;
}

void RenderSystem::RegisterModel(Model *model)
{
	// Keep track of different models.
	model_list.push_back(model);

	size_t vertex_size = model->vertices.size() * sizeof(glm::vec3);
	size_t uv_size = model->uvs.size() * sizeof(glm::vec2);
	size_t normal_size = model->normals.size() * sizeof(glm::vec3);
	size_t tangent_size = model->tangents.size() * sizeof(glm::vec3);

	// Add necessary data to buffers to render.
	glNamedBufferSubData(vertex_buffer, vertex_buffer_length, vertex_size, &model->vertices[0]);
	glNamedBufferSubData(uv_buffer, uv_buffer_length, uv_size, &model->uvs[0]);
	glNamedBufferSubData(normal_buffer, normal_buffer_length, normal_size, &model->normals[0]);
	glNamedBufferSubData(tangent_buffer, tangent_buffer_length, tangent_size, &model->tangents[0]);

	// Update counters.
	vertex_buffer_length += vertex_size;
	uv_buffer_length += uv_size;
	normal_buffer_length += normal_size;
	tangent_buffer_length += tangent_size;
}

unsigned int RenderSystem::RegisterLight(glm::vec3 light_pos, glm::vec3 light_col, float light_pow)
{
	// Can feed these directly into any shader.
	size_t id = light_pos_list.size();
	light_pos_list.push_back(light_pos);
	light_col_list.push_back(light_col);
	light_pow_list.push_back(light_pow);

	return (unsigned int)id;
}


const std::vector<glm::vec3>& RenderSystem::GetLightPositions()
{
	return light_pos_list;
}

const std::vector<glm::vec3>& RenderSystem::GetLightColors()
{
	return light_col_list;
}

const std::vector<float>& RenderSystem::GetLightPowers()
{
	return light_pow_list;
}

void RenderSystem::RegisterShader(Shader *shader)
{
	shader_list.push_back(shader);
}

void RenderSystem::Render()
{
	// Clear the screen.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	//Update all shaders.
	for (Shader* shader : shader_list)
	{
		shader->Update();
	}

	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
	glVertexAttribPointer(
		0,                  // attribute 0. No particular reason for 0, but must match the layout in the shader.
		3,                  // size
		GL_FLOAT,           // type
		GL_FALSE,           // normalized?
		0,                  // stride
		(void*)0            // array buffer offset
	);

	// 2nd attribute buffer : uvs
	glEnableVertexAttribArray(1);
	glBindBuffer(GL_ARRAY_BUFFER, uv_buffer);
	glVertexAttribPointer(
		1,                                // attribute. No particular reason for 1, but must match the layout in the shader.
		2,                                // size
		GL_FLOAT,                         // type
		GL_FALSE,                         // normalized?
		0,                                // stride
		(void*)0                          // array buffer offset
	);

	glEnableVertexAttribArray(2);
	glBindBuffer(GL_ARRAY_BUFFER, normal_buffer);
	glVertexAttribPointer(
		2,                                // attribute. No particular reason for 1, but must match the layout in the shader.
		3,                                // size
		GL_FLOAT,                         // type
		GL_FALSE,                         // normalized?
		0,                                // stride
		(void*)0                          // array buffer offset	
	);


	glEnableVertexAttribArray(3);
	glBindBuffer(GL_ARRAY_BUFFER, tangent_buffer);
	glVertexAttribPointer(
		3,                                // attribute. No particular reason for 1, but must match the layout in the shader.
		3,                                // size
		GL_FLOAT,                         // type
		GL_FALSE,                         // normalized?
		0,                                // stride
		(void*)0                          // array buffer offset	
	);

	// Draw the triangle !
	glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertex_buffer_length); // Starting from vertex 0; 3 vertices total -> 1 triangle
	glDisableVertexAttribArray(0);
	////////////////////////////////////////////////
}
