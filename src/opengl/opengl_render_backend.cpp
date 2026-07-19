#include "opengl_render_backend.h"

#include "mesh.h"
#include "render_system.h"
#include "opengl/opengl_texture.h"

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

void OpenGLMaterialResources::Destroy()
{
	if (albedo_tex != 0)
	{
		glDeleteTextures(1, &albedo_tex);
		albedo_tex = 0;
	}
	if (normal_tex != 0)
	{
		glDeleteTextures(1, &normal_tex);
		normal_tex = 0;
	}
	if (metallic_roughness_ao_tex != 0)
	{
		glDeleteTextures(1, &metallic_roughness_ao_tex);
		metallic_roughness_ao_tex = 0;
	}
}

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

bool OpenGLRenderBackend::Initialize(GLFWwindow* /*window*/, int in_window_width, int in_window_height)
{
	glEnable(GL_DEPTH_TEST);
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
		std::cout << "OpenGL render backend framebuffer is incomplete.\n";
		return false;
	}

	ssao = std::make_unique<SSAO>();
	ssao->SetTextures(g_color, rbo_depth, in_window_width, in_window_height);

	window_width = in_window_width;
	window_height = in_window_height;
	return true;
}

void OpenGLRenderBackend::Shutdown()
{
	ssao.reset();

	for (auto& it : material_resources)
	{
		it.second.Destroy();
	}
	material_resources.clear();

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
	window_width = 0;
	window_height = 0;
}

void OpenGLRenderBackend::RegisterRenderable(const Renderable& renderable)
{
	assert(renderable.mesh != nullptr);
	assert(renderable.material != nullptr);

	const std::unordered_map<Material*, MeshData>& mesh_data_dict = renderable.mesh->GetMaterialMeshData();
	const auto mesh_data_it = mesh_data_dict.find(renderable.material);
	if (mesh_data_it == mesh_data_dict.end())
	{
		return;
	}

	{
		Material* material = renderable.material;
		const MeshData& current_mesh_data = mesh_data_it->second;
		if (current_mesh_data.vertices.empty())
		{
			return;
		}

		assert(current_mesh_data.uvs.size() == current_mesh_data.vertices.size());
		assert(current_mesh_data.normals.size() == current_mesh_data.vertices.size());
		assert(current_mesh_data.tangents.size() == current_mesh_data.vertices.size());

		size_t vertex_size = current_mesh_data.vertices.size() * sizeof(glm::vec3);
		size_t uv_size = current_mesh_data.uvs.size() * sizeof(glm::vec2);
		size_t normal_size = current_mesh_data.normals.size() * sizeof(glm::vec3);
		size_t tangent_size = current_mesh_data.tangents.size() * sizeof(glm::vec3);
			
		// This assumes that the materials and shader are both already registered.
		ShaderBuffers* current_buffers = &shader_buffer_dict.at(material->shader_type);
		assert(CanAppendBufferData(*current_buffers, vertex_size, uv_size, normal_size, tangent_size));

		glNamedBufferSubData(current_buffers->vertex_buffer, current_buffers->vertex_buffer_length,
			vertex_size, current_mesh_data.vertices.data());
		glNamedBufferSubData(current_buffers->uv_buffer, current_buffers->uv_buffer_length,
			uv_size, current_mesh_data.uvs.data());
		glNamedBufferSubData(current_buffers->normal_buffer, current_buffers->normal_buffer_length,
			normal_size, current_mesh_data.normals.data());
		glNamedBufferSubData(current_buffers->tangent_buffer, current_buffers->tangent_buffer_length,
			tangent_size, current_mesh_data.tangents.data());

		material_render_dict[material].counts.push_back(static_cast<GLsizei>(current_mesh_data.vertices.size()));
		material_render_dict[material].start_indexes.push_back(
			static_cast<GLint>(current_buffers->vertex_buffer_length / sizeof(glm::vec3)));

		current_buffers->vertex_buffer_length += vertex_size;
		current_buffers->uv_buffer_length += uv_size;
		current_buffers->normal_buffer_length += normal_size;
		current_buffers->tangent_buffer_length += tangent_size;
	}
}

ShaderBuffers OpenGLRenderBackend::InitShaderBuffers()
{
	ShaderBuffers buffers = ShaderBuffers();

	glGenVertexArrays(1, &buffers.vertex_array_obj);
	glBindVertexArray(buffers.vertex_array_obj);

	glGenBuffers(1, &buffers.vertex_buffer);

	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, buffers.vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, kDefaultBufferSize, nullptr, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

	glGenBuffers(1, &buffers.uv_buffer);

	glEnableVertexAttribArray(1);
	glBindBuffer(GL_ARRAY_BUFFER, buffers.uv_buffer);
	glBufferData(GL_ARRAY_BUFFER, kDefaultBufferSize, nullptr, GL_STATIC_DRAW);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
	
	glGenBuffers(1, &buffers.normal_buffer);

	glEnableVertexAttribArray(2);
	glBindBuffer(GL_ARRAY_BUFFER, buffers.normal_buffer);
	glBufferData(GL_ARRAY_BUFFER, kDefaultBufferSize, nullptr, GL_STATIC_DRAW);
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
	
	glGenBuffers(1, &buffers.tangent_buffer);

	glEnableVertexAttribArray(3);
	glBindBuffer(GL_ARRAY_BUFFER, buffers.tangent_buffer);
	glBufferData(GL_ARRAY_BUFFER, kDefaultBufferSize, nullptr, GL_STATIC_DRAW);
	glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

	glBindVertexArray(0);
	return buffers;
}

void OpenGLRenderBackend::RegisterMaterial(Material* material)
{
	assert(material != nullptr);

	if (shader_buffer_dict.find(material->shader_type) == shader_buffer_dict.end())
	{
		shader_buffer_dict[material->shader_type] = InitShaderBuffers();
	}

	std::vector<Material*>& materials = shader_buffer_dict[material->shader_type].materials;
	materials.push_back(material);

	OpenGLMaterialResources& resources = material_resources[material];
	resources.albedo_tex = OpenGLTexture::LoadDDS(material->GetAlbedoPath());
	resources.normal_tex = OpenGLTexture::LoadDDS(material->GetNormalPath());
	resources.metallic_roughness_ao_tex = OpenGLTexture::LoadDDS(material->GetMetallicRoughnessAoPath());
}

PBRStandard* OpenGLRenderBackend::GetShader(MaterialShaderType shader_type)
{
	switch (shader_type)
	{
	case MaterialShaderType::PBRStandard:
		if (pbr_shader == nullptr)
		{
			pbr_shader = std::make_unique<PBRStandard>();
		}
		return pbr_shader.get();
	}

	return nullptr;
}

void OpenGLRenderBackend::Render()
{
	glBindFramebuffer(GL_FRAMEBUFFER, g_buffer);
	glViewport(0, 0, window_width, window_height);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	for (auto& it : shader_buffer_dict) {
		PBRStandard* shader = GetShader(it.first);
		if (shader == nullptr)
		{
			continue;
		}

		glBindVertexArray(it.second.vertex_array_obj);
		shader->Use();
		
		for (Material* material : it.second.materials)
		{
			MaterialRenderData& render_data = material_render_dict[material];
			if (render_data.counts.empty())
			{
				continue;
			}

			const OpenGLMaterialResources& resources = material_resources.at(material);
			shader->SetTextures(resources.albedo_tex, resources.normal_tex, resources.metallic_roughness_ao_tex);
			shader->Update();

			glMultiDrawArrays(GL_TRIANGLES, 
				&render_data.start_indexes[0],
				&render_data.counts[0], 
				static_cast<GLsizei>(render_data.counts.size()));
		}
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, window_width, window_height);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	assert(ssao != nullptr);
	ssao->Use();
	ssao->Update();

	RenderScreenSpaceQuad();
}

void OpenGLRenderBackend::RenderScreenSpaceQuad()
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
		glGenVertexArrays(1, &quad_VAO);
		glGenBuffers(1, &quad_VBO);
		glBindVertexArray(quad_VAO);
		glBindBuffer(GL_ARRAY_BUFFER, quad_VBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	}
	glBindVertexArray(quad_VAO);

	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);
}
