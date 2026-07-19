#include "render_system.h"

#include <cassert>
#include <iostream>
#include <utility>

namespace {
constexpr size_t kDefaultBufferSize = 10000000;

bool CanAppendBufferData(const ShaderBuffers& buffers, size_t vertex_size, size_t uv_size,
	size_t normal_size, size_t tangent_size)
{
	return buffers.vertex_buffer_length + vertex_size <= kDefaultBufferSize &&
		buffers.uv_buffer_length + uv_size <= kDefaultBufferSize &&
		buffers.normal_buffer_length + normal_size <= kDefaultBufferSize &&
		buffers.tangent_buffer_length + tangent_size <= kDefaultBufferSize;
}
} // namespace

ShaderBuffers::ShaderBuffers(ShaderBuffers&& other) noexcept
{
	*this = std::move(other);
}

ShaderBuffers& ShaderBuffers::operator=(ShaderBuffers&& other) noexcept
{
	if (this == &other)
	{
		return *this;
	}

	Destroy();

	materials = std::move(other.materials);
	vertex_array_obj = other.vertex_array_obj;
	vertex_buffer = other.vertex_buffer;
	uv_buffer = other.uv_buffer;
	normal_buffer = other.normal_buffer;
	tangent_buffer = other.tangent_buffer;
	indice_buffer = other.indice_buffer;
	vertex_buffer_length = other.vertex_buffer_length;
	uv_buffer_length = other.uv_buffer_length;
	normal_buffer_length = other.normal_buffer_length;
	tangent_buffer_length = other.tangent_buffer_length;
	indice_buffer_length = other.indice_buffer_length;

	other.vertex_array_obj = 0;
	other.vertex_buffer = 0;
	other.uv_buffer = 0;
	other.normal_buffer = 0;
	other.tangent_buffer = 0;
	other.indice_buffer = 0;
	other.vertex_buffer_length = 0;
	other.uv_buffer_length = 0;
	other.normal_buffer_length = 0;
	other.tangent_buffer_length = 0;
	other.indice_buffer_length = 0;

	return *this;
}

ShaderBuffers::~ShaderBuffers()
{
	Destroy();
}

void ShaderBuffers::Destroy()
{
	if (vertex_buffer != 0)
	{
		glDeleteBuffers(1, &vertex_buffer);
		vertex_buffer = 0;
	}
	if (uv_buffer != 0)
	{
		glDeleteBuffers(1, &uv_buffer);
		uv_buffer = 0;
	}
	if (normal_buffer != 0)
	{
		glDeleteBuffers(1, &normal_buffer);
		normal_buffer = 0;
	}
	if (tangent_buffer != 0)
	{
		glDeleteBuffers(1, &tangent_buffer);
		tangent_buffer = 0;
	}
	if (indice_buffer != 0)
	{
		glDeleteBuffers(1, &indice_buffer);
		indice_buffer = 0;
	}
	if (vertex_array_obj != 0)
	{
		glDeleteVertexArrays(1, &vertex_array_obj);
		vertex_array_obj = 0;
	}

	materials.clear();
	vertex_buffer_length = 0;
	uv_buffer_length = 0;
	normal_buffer_length = 0;
	tangent_buffer_length = 0;
	indice_buffer_length = 0;
}

GLuint RenderSystem::g_buffer = 0;
GLuint RenderSystem::g_color = 0;
GLuint RenderSystem::rbo_depth = 0;

GLuint RenderSystem::quad_VAO = 0;
GLuint RenderSystem::quad_VBO = 0;

int RenderSystem::window_width = 0;
int RenderSystem::window_height = 0;

std::unique_ptr<SSAO> RenderSystem::ssao;

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

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, in_window_width, in_window_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_color, 0);

	glGenTextures(1, &rbo_depth);
	glBindTexture(GL_TEXTURE_2D, rbo_depth);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, in_window_width, in_window_height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); 
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, rbo_depth, 0);


	GLenum attachments[1] = { GL_COLOR_ATTACHMENT0 };
	glDrawBuffers(1, attachments);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		std::cout << "RenderSystem framebuffer is incomplete.\n";
	}

	ssao = std::make_unique<SSAO>();
	ssao->SetTextures(g_color, rbo_depth, in_window_width, in_window_height);

	window_width = in_window_width;
	window_height = in_window_height;
}

void RenderSystem::Shutdown()
{
	ssao.reset();

	if (quad_VBO != 0)
	{
		glDeleteBuffers(1, &quad_VBO);
		quad_VBO = 0;
	}
	if (quad_VAO != 0)
	{
		glDeleteVertexArrays(1, &quad_VAO);
		quad_VAO = 0;
	}
	if (g_color != 0)
	{
		glDeleteTextures(1, &g_color);
		g_color = 0;
	}
	if (rbo_depth != 0)
	{
		glDeleteTextures(1, &rbo_depth);
		rbo_depth = 0;
	}
	if (g_buffer != 0)
	{
		glDeleteFramebuffers(1, &g_buffer);
		g_buffer = 0;
	}

	shader_buffer_dict.clear();
	material_render_dict.clear();
	light_pos_list.clear();
	light_col_list.clear();
	light_pow_list.clear();
	window_width = 0;
	window_height = 0;
}

void RenderSystem::RegisterMesh(const Mesh* mesh)
{
	assert(mesh != nullptr);

	const std::unordered_map<Material*, MeshData>& mesh_data_dict = mesh->GetMaterialMeshData();

	for (const auto& it : mesh_data_dict)
	{
		assert(it.first != nullptr);
		assert(it.first->shader != nullptr);

		const MeshData& current_mesh_data = it.second;
		if (current_mesh_data.vertices.empty())
		{
			continue;
		}

		assert(current_mesh_data.uvs.size() == current_mesh_data.vertices.size());
		assert(current_mesh_data.normals.size() == current_mesh_data.vertices.size());
		assert(current_mesh_data.tangents.size() == current_mesh_data.vertices.size());

		size_t vertex_size = current_mesh_data.vertices.size() * sizeof(glm::vec3);
		size_t uv_size = current_mesh_data.uvs.size() * sizeof(glm::vec2);
		size_t normal_size = current_mesh_data.normals.size() * sizeof(glm::vec3);
		size_t tangent_size = current_mesh_data.tangents.size() * sizeof(glm::vec3);
			
		// This assumes that the materials and shader are both already registered.
		ShaderBuffers* current_buffers = &shader_buffer_dict.at(it.first->shader);
		assert(CanAppendBufferData(*current_buffers, vertex_size, uv_size, normal_size, tangent_size));

		// Add necessary data to relevant shader buffers to render.
		glNamedBufferSubData(current_buffers->vertex_buffer, current_buffers->vertex_buffer_length,
			vertex_size, current_mesh_data.vertices.data());
		glNamedBufferSubData(current_buffers->uv_buffer, current_buffers->uv_buffer_length,
			uv_size, current_mesh_data.uvs.data());
		glNamedBufferSubData(current_buffers->normal_buffer, current_buffers->normal_buffer_length,
			normal_size, current_mesh_data.normals.data());
		glNamedBufferSubData(current_buffers->tangent_buffer, current_buffers->tangent_buffer_length,
			tangent_size, current_mesh_data.tangents.data());

		// Multidrawarrays uses an element offset, not a byte offset!
		material_render_dict[it.first].counts.push_back(static_cast<GLsizei>(current_mesh_data.vertices.size()));
		material_render_dict[it.first].start_indexes.push_back(
			static_cast<GLint>(current_buffers->vertex_buffer_length / sizeof(glm::vec3)));

		// Keep track of buffer sizes.
		current_buffers->vertex_buffer_length += vertex_size;
		current_buffers->uv_buffer_length += uv_size;
		current_buffers->normal_buffer_length += normal_size;
		current_buffers->tangent_buffer_length += tangent_size;
	}
}

ShaderBuffers RenderSystem::InitShaderBuffers(Shader* /*shader*/)
{
	ShaderBuffers buffers = ShaderBuffers();

	glGenVertexArrays(1, &buffers.vertex_array_obj);
	glBindVertexArray(buffers.vertex_array_obj);

	// This order is essential!!!
	glGenBuffers(1, &buffers.vertex_buffer);

	// ENABLE AFTER GENERATING BUFFER
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, buffers.vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, kDefaultBufferSize, nullptr, GL_STATIC_DRAW);
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
	glBufferData(GL_ARRAY_BUFFER, kDefaultBufferSize, nullptr, GL_STATIC_DRAW);
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
	glBufferData(GL_ARRAY_BUFFER, kDefaultBufferSize, nullptr, GL_STATIC_DRAW);
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
	glBufferData(GL_ARRAY_BUFFER, kDefaultBufferSize, nullptr, GL_STATIC_DRAW);
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

void RenderSystem::RegisterMaterial(Material* material)
{
	assert(material != nullptr);
	assert(material->shader != nullptr);

	if (shader_buffer_dict.find(material->shader) == shader_buffer_dict.end())
	{
		// Initializing buffers.
		shader_buffer_dict[material->shader] = InitShaderBuffers(material->shader);
	}

	std::vector<Material*>& materials = shader_buffer_dict[material->shader].materials;
	materials.push_back(material);
}

// Render all materials for each shader at a time.
unsigned int RenderSystem::RegisterLight(const glm::vec3& light_pos, const glm::vec3& light_col, float light_pow)
{
	// Can feed these directly into any shader.
	size_t id = light_pos_list.size();
	light_pos_list.push_back(light_pos);
	light_col_list.push_back(light_col);
	light_pow_list.push_back(light_pow);

	return (unsigned int)id;
}

void RenderSystem::UpdateLight(unsigned int id, const glm::vec3& light_pos, const glm::vec3& light_col, float light_pow)
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
	glViewport(0, 0, window_width, window_height);

	// Clear the screen.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	for (auto& it : shader_buffer_dict) {

		glBindVertexArray(it.second.vertex_array_obj);
		it.first->Use();
		
		for (Material* material : it.second.materials)
		{
			MaterialRenderData& render_data = material_render_dict[material];
			if (render_data.counts.empty())
			{
				continue;
			}

			material->UpdateShader();

			// Draw the triangles batched per material!
			glMultiDrawArrays(GL_TRIANGLES, 
				&render_data.start_indexes[0],
				&render_data.counts[0], 
				static_cast<GLsizei>(render_data.counts.size())); // Starting from vertex 0; 3 vertices total -> 1 triangle
		}
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, window_width, window_height);

	// Clear the screen
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	assert(ssao != nullptr);
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
