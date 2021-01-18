#include "render_system.h"


#define DEFAULT_BUFFER_SIZE 10000000

/*GLuint RenderSystem::vertex_buffer;
GLuint RenderSystem::uv_buffer;
GLuint RenderSystem::normal_buffer;
GLuint RenderSystem::tangent_buffer;

size_t RenderSystem::vertex_buffer_length = 0;
size_t RenderSystem::uv_buffer_length = 0;
size_t RenderSystem::normal_buffer_length = 0;
size_t RenderSystem::tangent_buffer_length = 0;

std::vector<Model*> RenderSystem::model_list;*/

std::unordered_map<Shader*, ShaderBuffers> RenderSystem::shader_buffer_dict;
std::unordered_map<Material*, MaterialRenderData> RenderSystem::material_render_dict;


std::vector<glm::vec3> RenderSystem::light_pos_list;
std::vector<glm::vec3> RenderSystem::light_col_list;
std::vector<float> RenderSystem::light_pow_list;

void RenderSystem::Initialize()
{
	// Initialize our buffers.
	/*glGenBuffers(1, &vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, DEFAULT_BUFFER_SIZE, nullptr, GL_STATIC_DRAW);

	glGenBuffers(1, &uv_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, uv_buffer);
	glBufferData(GL_ARRAY_BUFFER, DEFAULT_BUFFER_SIZE, nullptr, GL_STATIC_DRAW);

	glGenBuffers(1, &normal_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, normal_buffer);
	glBufferData(GL_ARRAY_BUFFER, DEFAULT_BUFFER_SIZE, nullptr, GL_STATIC_DRAW);

	glGenBuffers(1, &tangent_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, tangent_buffer);
	glBufferData(GL_ARRAY_BUFFER, DEFAULT_BUFFER_SIZE, nullptr, GL_STATIC_DRAW);*/

	// Enable depth test
	glEnable(GL_DEPTH_TEST);
	// Accept fragment if it closer to the camera than the former one
	glDepthFunc(GL_LESS);

	glEnable(GL_CULL_FACE);

	/*vertex_buffer_length = 0;
	uv_buffer_length = 0;
	normal_buffer_length = 0;
	tangent_buffer_length = 0;*/
}

void RenderSystem::RegisterMesh(Mesh *mesh)
{
	std::unordered_map<Material*, MeshData>& mesh_data_dict = mesh->getMaterialMeshData();

	for (auto& it : mesh_data_dict)
	{
		if (material_render_dict.find(it.first) == material_render_dict.end())
			material_render_dict[it.first] = MaterialRenderData();

		MeshData *current_mesh_data = &it.second;

		size_t vertex_size = current_mesh_data->vertices.size() * sizeof(glm::vec3);
		size_t uv_size = current_mesh_data->uvs.size() * sizeof(glm::vec2);
		size_t normal_size = current_mesh_data->normals.size() * sizeof(glm::vec3);
		size_t tangent_size = current_mesh_data->tangents.size() * sizeof(glm::vec3);
			
		// This assumes that the materials and shader are both already registered.
		ShaderBuffers *current_buffers = &shader_buffer_dict[it.first->shader];

		// Add necessary data to relevant shader buffers to render.
		glNamedBufferSubData(current_buffers->vertex_buffer, current_buffers->vertex_buffer_length, vertex_size, &current_mesh_data->vertices[0]);
		glNamedBufferSubData(current_buffers->uv_buffer, current_buffers->uv_buffer_length, uv_size, &current_mesh_data->uvs[0]);
		glNamedBufferSubData(current_buffers->normal_buffer, current_buffers->normal_buffer_length, normal_size, &current_mesh_data->normals[0]);
		glNamedBufferSubData(current_buffers->tangent_buffer, current_buffers->tangent_buffer_length, tangent_size, &current_mesh_data->tangents[0]);

		// Add data to render dict.
		material_render_dict[it.first].mesh_data.push_back(current_mesh_data);

		// Multidrawarrays uses an element offset, not a byte offset!
		material_render_dict[it.first].counts.push_back(vertex_size / sizeof(glm::vec3));
		material_render_dict[it.first].start_indexes.push_back(current_buffers->vertex_buffer_length / sizeof(glm::vec3));

		// Keep track of buffer sizes.
		current_buffers->vertex_buffer_length += vertex_size;
		current_buffers->uv_buffer_length += uv_size;
		current_buffers->normal_buffer_length += normal_size;
		current_buffers->tangent_buffer_length += tangent_size;
	}
}

ShaderBuffers RenderSystem::InitShaderBuffers(Shader* shader)
{
	ShaderBuffers buffers = ShaderBuffers();

	glGenVertexArrays(1, &buffers.vertex_array_obj);
	glBindVertexArray(buffers.vertex_array_obj);

	// This order is essential!!!
	glGenBuffers(1, &buffers.vertex_buffer);

	// ENABLE AFTER GENERATING BUFFER
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, buffers.vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, DEFAULT_BUFFER_SIZE, nullptr, GL_STATIC_DRAW);
	glVertexAttribPointer(
		0,                  // attribute 0. No particular reason for 0, but must match the layout in the shader.
		3,                  // size
		GL_FLOAT,           // type
		GL_FALSE,           // normalized?
		0,                  // stride
		(void*)0            // array buffer offset
	);

	
	glGenBuffers(1, &buffers.uv_buffer);

	glEnableVertexAttribArray(1);
	glBindBuffer(GL_ARRAY_BUFFER, buffers.uv_buffer);
	glBufferData(GL_ARRAY_BUFFER, DEFAULT_BUFFER_SIZE, nullptr, GL_STATIC_DRAW);
	glVertexAttribPointer(
		1,                                // attribute. No particular reason for 1, but must match the layout in the shader.
		2,                                // size
		GL_FLOAT,                         // type
		GL_FALSE,                         // normalized?
		0,                                // stride
		(void*)0                          // array buffer offset
	);
	
	glGenBuffers(1, &buffers.normal_buffer);

	glEnableVertexAttribArray(2);
	glBindBuffer(GL_ARRAY_BUFFER, buffers.normal_buffer);
	glBufferData(GL_ARRAY_BUFFER, DEFAULT_BUFFER_SIZE, nullptr, GL_STATIC_DRAW);
	glVertexAttribPointer(
		2,                                // attribute. No particular reason for 1, but must match the layout in the shader.
		3,                                // size
		GL_FLOAT,                         // type
		GL_FALSE,                         // normalized?
		0,                                // stride
		(void*)0                          // array buffer offset	
	);

	
	glGenBuffers(1, &buffers.tangent_buffer);

	glEnableVertexAttribArray(3);
	glBindBuffer(GL_ARRAY_BUFFER, buffers.tangent_buffer);
	glBufferData(GL_ARRAY_BUFFER, DEFAULT_BUFFER_SIZE, nullptr, GL_STATIC_DRAW);
	glVertexAttribPointer(
		3,                                // attribute. No particular reason for 1, but must match the layout in the shader.
		3,                                // size
		GL_FLOAT,                         // type
		GL_FALSE,                         // normalized?
		0,                                // stride
		(void*)0                          // array buffer offset	
	);

	glBindVertexArray(0);
	return buffers;
}

void RenderSystem::RegisterMaterial(Material * material)
{
	if (shader_buffer_dict.find(material->shader) == shader_buffer_dict.end())
	{
		// Initializing buffers.
		shader_buffer_dict[material->shader] = InitShaderBuffers(material->shader);
	}

	shader_buffer_dict[material->shader].materials.push_back(material);
}

/*void RenderSystem::RegisterModel(Model *model)
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

	// Split vertex buffer by material?

}*/

// Render all materials for each shader at a time.

unsigned int RenderSystem::RegisterLight(glm::vec3 light_pos, glm::vec3 light_col, float light_pow)
{
	// Can feed these directly into any shader.
	size_t id = light_pos_list.size();
	light_pos_list.push_back(light_pos);
	light_col_list.push_back(light_col);
	light_pow_list.push_back(light_pow);

	return (unsigned int)id;
}

void RenderSystem::UpdateLight(unsigned int id, glm::vec3 light_pos, glm::vec3 light_col, float light_pow)
{
	if (id < light_pos_list.size())
	{
		light_pos_list[id] = light_pos;
		light_col_list[id] = light_col;
		light_pow_list[id] = light_pow;
	}
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

void RenderSystem::Render()
{
	// Clear the screen.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	for (auto& it : shader_buffer_dict) {

		glBindVertexArray(it.second.vertex_array_obj);
		it.first->Use();
		
		for (Material* material : it.second.materials)
		{
			material->UpdateShader();

			// Draw the triangles batched per material!
			glMultiDrawArrays(GL_TRIANGLES, 
				&material_render_dict[material].start_indexes[0],
				&material_render_dict[material].counts[0], 
				material_render_dict[material].counts.size()); // Starting from vertex 0; 3 vertices total -> 1 triangle

		}
	}
	glBindVertexArray(0);
}
