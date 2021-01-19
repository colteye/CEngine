#include "render_system.h"


#define DEFAULT_BUFFER_SIZE 10000000

GLuint RenderSystem::g_buffer, RenderSystem::g_position, RenderSystem::g_normal, RenderSystem::g_color, RenderSystem::rbo_depth;

GLuint RenderSystem::quad_VAO, RenderSystem::quad_VBO;

int RenderSystem::window_width, RenderSystem::window_height;

// Add SSAO!
SSAO *RenderSystem::ssao;

std::unordered_map<Shader*, ShaderBuffers> RenderSystem::shader_buffer_dict;
std::unordered_map<Material*, MaterialRenderData> RenderSystem::material_render_dict;


std::vector<glm::vec3> RenderSystem::light_pos_list;
std::vector<glm::vec3> RenderSystem::light_col_list;
std::vector<float> RenderSystem::light_pow_list;

void RenderSystem::Initialize(int in_window_width, int in_window_height)
{

	//////// BASE PARAMETERS /////////
	// Enable depth test
	glEnable(GL_DEPTH_TEST);
	// Accept fragment if it closer to the camera than the former one
	glDepthFunc(GL_LESS);

	glEnable(GL_CULL_FACE);

	glGenFramebuffers(1, &g_buffer);
	glBindFramebuffer(GL_FRAMEBUFFER, g_buffer);

	glGenTextures(1, &g_color);
	glBindTexture(GL_TEXTURE_2D, g_color);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, in_window_width, in_window_height, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_color, 0);

	glGenTextures(1, &rbo_depth);
	glBindTexture(GL_TEXTURE_2D, rbo_depth);
	glTexImage2D(GL_TEXTURE_2D, 0,GL_DEPTH_COMPONENT24, 1024, 768, 0,GL_DEPTH_COMPONENT, GL_FLOAT, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); 
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, rbo_depth, 0);


	// - tell OpenGL which color attachments we'll use (of this framebuffer) for rendering 
	GLenum attachments[2] = { GL_COLOR_ATTACHMENT0, GL_DEPTH_ATTACHMENT };
	glDrawBuffers(2, attachments);

	ssao = new SSAO();
	ssao->SetTextures(g_color, rbo_depth);

	window_width = in_window_width;
	window_height = in_window_height;
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
	glBindFramebuffer(GL_FRAMEBUFFER, g_buffer);
	//glViewport(0, 0, window_width, window_height);

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

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	//glViewport(0, 0, window_width, window_height);

	// Clear the screen
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	ssao->Use();
	ssao->Update();

	// Render screenspace quad for everything!
	RenderScreenSpaceQuad();

}

void RenderSystem::RenderScreenSpaceQuad()
{
	if (quad_VAO == 0)
	{
		float quadVertices[] = {
		-1.0f, -1.0f, 0.0f,
		 1.0f, -1.0f, 0.0f,
		-1.0f,  1.0f, 0.0f,
		-1.0f,  1.0f, 0.0f,
		 1.0f, -1.0f, 0.0f,
		 1.0f,  1.0f, 0.0f,
		};
		// setup plane VAO
		glGenVertexArrays(1, &quad_VAO);
		glGenBuffers(1, &quad_VBO);
		glBindVertexArray(quad_VAO);
		glBindBuffer(GL_ARRAY_BUFFER, quad_VBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	}
	glBindVertexArray(quad_VAO);

	glDrawArrays(GL_TRIANGLES, 0, 6); // 2*3 indices starting at 0 -> 2 triangles
	glBindVertexArray(0);
}