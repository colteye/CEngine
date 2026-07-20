#include "opengl_render_backend.h"

#include "mesh.h"
#include "render_system.h"
#include "opengl/opengl_texture.h"

#include <cassert>
#include <algorithm>
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

void ConfigureFramebufferTexture(GLuint texture, GLenum internal_format, GLenum format, GLenum type, int width, int height)
{
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(internal_format), width, height, 0, format, type, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

glm::vec3 BoundsCenter(const Bounds& bounds, const glm::mat4& transform)
{
	if (!bounds.valid)
	{
		return glm::vec3(transform[3]);
	}
	return (bounds.min + bounds.max) * 0.5f;
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

	if (!CreateFrameResources(in_window_width, in_window_height))
	{
		DestroyFrameResources();
		return false;
	}

	ssao = std::make_unique<SSAO>();
	ssao->SetTextures(lit_color, scene_depth, in_window_width, in_window_height);

	window_width = in_window_width;
	window_height = in_window_height;
	return true;
}

void OpenGLRenderBackend::Shutdown()
{
	ssao.reset();
	pbr_shader.reset();
	pbr_geometry_pass.reset();
	deferred_lighting.reset();
	depth_only_shader.reset();

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
	DestroyFrameResources();

	shader_buffer_dict.clear();
	draw_items.clear();
	opaque_deferred_queue.clear();
	forward_queue.clear();
	transparent_queue.clear();
	shadow_caster_queue.clear();
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

		OpenGLDrawItem draw_item;
		draw_item.material = material;
		draw_item.transform = renderable.transform;
		draw_item.world_bounds = renderable.world_bounds;
		draw_item.start_index = static_cast<GLint>(current_buffers->vertex_buffer_length / sizeof(glm::vec3));
		draw_item.count = static_cast<GLsizei>(current_mesh_data.vertices.size());
		draw_item.flags = renderable.flags;
		draw_items.push_back(draw_item);

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

bool OpenGLRenderBackend::CreateFrameResources(int width, int height)
{
	glGenFramebuffers(1, &g_buffer_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, g_buffer_fbo);

	glGenTextures(1, &g_albedo);
	ConfigureFramebufferTexture(g_albedo, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, width, height);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_albedo, 0);

	glGenTextures(1, &g_normal_roughness);
	ConfigureFramebufferTexture(g_normal_roughness, GL_RGBA16F, GL_RGBA, GL_FLOAT, width, height);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, g_normal_roughness, 0);

	glGenTextures(1, &g_material);
	ConfigureFramebufferTexture(g_material, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, width, height);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, g_material, 0);

	glGenTextures(1, &scene_depth);
	ConfigureFramebufferTexture(scene_depth, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_FLOAT, width, height);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, scene_depth, 0);

	const GLenum g_buffer_attachments[3] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
	glDrawBuffers(3, g_buffer_attachments);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		std::cout << "OpenGL G-buffer framebuffer is incomplete.\n";
		return false;
	}

	glGenFramebuffers(1, &lit_color_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, lit_color_fbo);

	glGenTextures(1, &lit_color);
	ConfigureFramebufferTexture(lit_color, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, width, height);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, lit_color, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, scene_depth, 0);

	const GLenum lit_attachment = GL_COLOR_ATTACHMENT0;
	glDrawBuffers(1, &lit_attachment);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		std::cout << "OpenGL lit color framebuffer is incomplete.\n";
		return false;
	}

	std::cout << "OpenGL G-buffer ready: albedo, normal/roughness, material/flags, depth.\n";
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	return true;
}

void OpenGLRenderBackend::DestroyFrameResources()
{
	if (g_albedo != 0)
	{
		glDeleteTextures(1, &g_albedo);
		g_albedo = 0;
	}
	if (g_normal_roughness != 0)
	{
		glDeleteTextures(1, &g_normal_roughness);
		g_normal_roughness = 0;
	}
	if (g_material != 0)
	{
		glDeleteTextures(1, &g_material);
		g_material = 0;
	}
	if (scene_depth != 0)
	{
		glDeleteTextures(1, &scene_depth);
		scene_depth = 0;
	}
	if (lit_color != 0)
	{
		glDeleteTextures(1, &lit_color);
		lit_color = 0;
	}
	if (g_buffer_fbo != 0)
	{
		glDeleteFramebuffers(1, &g_buffer_fbo);
		g_buffer_fbo = 0;
	}
	if (lit_color_fbo != 0)
	{
		glDeleteFramebuffers(1, &lit_color_fbo);
		lit_color_fbo = 0;
	}
	if (depth_only_fbo != 0)
	{
		glDeleteFramebuffers(1, &depth_only_fbo);
		depth_only_fbo = 0;
	}
}

void OpenGLRenderBackend::BuildRenderQueues()
{
	opaque_deferred_queue.clear();
	forward_queue.clear();
	transparent_queue.clear();
	shadow_caster_queue.clear();

	if (opaque_deferred_queue.capacity() < draw_items.size())
	{
		opaque_deferred_queue.reserve(draw_items.size());
		forward_queue.reserve(draw_items.size());
		transparent_queue.reserve(draw_items.size());
		shadow_caster_queue.reserve(draw_items.size());
	}

	for (uint32_t index = 0; index < draw_items.size(); ++index)
	{
		const OpenGLDrawItem& item = draw_items[index];
		const MaterialRenderMode mode = item.material->GetRenderMode();

		if (DrawsInDeferredGeometry(mode))
		{
			opaque_deferred_queue.push_back(index);
		}
		else if (DrawsTransparent(mode))
		{
			transparent_queue.push_back(index);
		}
		else if (DrawsInForward(mode))
		{
			forward_queue.push_back(index);
		}

		if (DrawsShadowCaster(item))
		{
			shadow_caster_queue.push_back(index);
		}
	}

	const glm::vec3 camera_position = RenderSystem::GetFrameConstants().camera_position;
	std::sort(transparent_queue.begin(), transparent_queue.end(),
		[&](uint32_t left_index, uint32_t right_index) {
			const glm::vec3 left_center = BoundsCenter(draw_items[left_index].world_bounds,
				draw_items[left_index].transform);
			const glm::vec3 right_center = BoundsCenter(draw_items[right_index].world_bounds,
				draw_items[right_index].transform);
			const float left_distance = glm::dot(left_center - camera_position, left_center - camera_position);
			const float right_distance = glm::dot(right_center - camera_position, right_center - camera_position);
			return left_distance > right_distance;
		});
}

void OpenGLRenderBackend::RenderGeometryPass()
{
	if (pbr_geometry_pass == nullptr)
	{
		pbr_geometry_pass = std::make_unique<PBRGeometryPass>();
	}

	glBindFramebuffer(GL_FRAMEBUFFER, g_buffer_fbo);
	glViewport(0, 0, window_width, window_height);
	const GLenum attachments[3] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
	glDrawBuffers(3, attachments);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
	glEnable(GL_CULL_FACE);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	pbr_geometry_pass->Use();
	for (uint32_t draw_index : opaque_deferred_queue)
	{
		const OpenGLDrawItem& item = draw_items[draw_index];
		const ShaderBuffers& buffers = shader_buffer_dict.at(item.material->shader_type);
		const OpenGLMaterialResources& resources = material_resources.at(item.material);

		glBindVertexArray(buffers.vertex_array_obj);
		pbr_geometry_pass->SetTextures(resources.albedo_tex, resources.normal_tex, resources.metallic_roughness_ao_tex);
		pbr_geometry_pass->Update(item.transform, *item.material);
		DrawItem(item);
	}
	glBindVertexArray(0);
}

void OpenGLRenderBackend::RenderDeferredLightingPass()
{
	if (deferred_lighting == nullptr)
	{
		deferred_lighting = std::make_unique<DeferredLighting>();
	}

	glBindFramebuffer(GL_FRAMEBUFFER, lit_color_fbo);
	glViewport(0, 0, window_width, window_height);
	const GLenum attachment = GL_COLOR_ATTACHMENT0;
	glDrawBuffers(1, &attachment);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	glClear(GL_COLOR_BUFFER_BIT);

	deferred_lighting->Use();
	deferred_lighting->Update(g_albedo, g_normal_roughness, g_material, scene_depth, window_width, window_height);
	RenderScreenSpaceQuad();
}

void OpenGLRenderBackend::RenderForwardQueue(const std::vector<uint32_t>& queue)
{
	if (queue.empty())
	{
		return;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, lit_color_fbo);
	glViewport(0, 0, window_width, window_height);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	const bool transparent = DrawsTransparent(draw_items[queue.front()].material->GetRenderMode());
	if (transparent)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDepthMask(GL_FALSE);
	}
	else
	{
		glDisable(GL_BLEND);
		glDepthMask(GL_TRUE);
	}

	for (uint32_t draw_index : queue)
	{
		const OpenGLDrawItem& item = draw_items[draw_index];
		PBRStandard* shader = GetShader(item.material->shader_type);
		if (shader == nullptr)
		{
			continue;
		}

		const ShaderBuffers& buffers = shader_buffer_dict.at(item.material->shader_type);
		const OpenGLMaterialResources& resources = material_resources.at(item.material);
		glBindVertexArray(buffers.vertex_array_obj);
		shader->Use();
		shader->SetTextures(resources.albedo_tex, resources.normal_tex, resources.metallic_roughness_ao_tex);
		shader->Update(item.transform, *item.material);
		DrawItem(item);
	}

	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
	glBindVertexArray(0);
}

void OpenGLRenderBackend::DrawItem(const OpenGLDrawItem& item) const
{
	glDrawArrays(GL_TRIANGLES, item.start_index, item.count);
}

bool OpenGLRenderBackend::DrawsInDeferredGeometry(MaterialRenderMode mode) const
{
	return mode == MaterialRenderMode::OpaqueDeferred ||
		mode == MaterialRenderMode::AlphaClip ||
		mode == MaterialRenderMode::AlphaHashDither;
}

bool OpenGLRenderBackend::DrawsInForward(MaterialRenderMode mode) const
{
	return mode == MaterialRenderMode::OpaqueForward ||
		mode == MaterialRenderMode::Unlit;
}

bool OpenGLRenderBackend::DrawsTransparent(MaterialRenderMode mode) const
{
	return mode == MaterialRenderMode::TransparentBlend;
}

bool OpenGLRenderBackend::DrawsShadowCaster(const OpenGLDrawItem& item) const
{
	return item.material->CastsShadows() && (item.flags & RenderableFlagCastsShadow) != 0;
}

void OpenGLRenderBackend::Render()
{
	BuildRenderQueues();
	RenderGeometryPass();
	RenderDeferredLightingPass();
	RenderForwardQueue(forward_queue);
	RenderForwardQueue(transparent_queue);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, window_width, window_height);

	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	assert(ssao != nullptr);
	ssao->Use();
	ssao->Update();

	RenderScreenSpaceQuad();

	glEnable(GL_DEPTH_TEST);
}

void OpenGLRenderBackend::RenderDepthOnly(const glm::mat4& view, const glm::mat4& projection,
	uint32_t native_depth_texture, int texture_width, int texture_height)
{
	if (native_depth_texture == 0 || texture_width <= 0 || texture_height <= 0)
	{
		return;
	}

	if (depth_only_fbo == 0)
	{
		glGenFramebuffers(1, &depth_only_fbo);
	}
	if (depth_only_shader == nullptr)
	{
		depth_only_shader = std::make_unique<DepthOnly>();
	}

	BuildRenderQueues();

	glBindFramebuffer(GL_FRAMEBUFFER, depth_only_fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, native_depth_texture, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		std::cout << "OpenGL depth-only framebuffer is incomplete.\n";
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		return;
	}
	glViewport(0, 0, texture_width, texture_height);
	glClear(GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);

	depth_only_shader->Use();
	for (uint32_t draw_index : shadow_caster_queue)
	{
		const OpenGLDrawItem& item = draw_items[draw_index];
		const ShaderBuffers& buffers = shader_buffer_dict.at(item.material->shader_type);
		const OpenGLMaterialResources& resources = material_resources.at(item.material);

		glBindVertexArray(buffers.vertex_array_obj);
		depth_only_shader->Update(item.transform, view, projection, *item.material, resources.albedo_tex);
		DrawItem(item);
	}

	glBindVertexArray(0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, window_width, window_height);
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
