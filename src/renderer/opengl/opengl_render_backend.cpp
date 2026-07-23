#include "opengl_render_backend.h"

#include "renderer/mesh.h"
#include "renderer/render_system.h"
#include "renderer/opengl/opengl_texture.h"

#include <cassert>
#include <algorithm>
#include <iostream>
#include <utility>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace CEngine::Renderer {

namespace {
constexpr size_t kDefaultBufferSize = 10000000;

bool CanAppendBufferData(const ShaderBuffers& buffers, size_t vertex_size, size_t uv_size,
	size_t normal_size, size_t tangent_size)
{
	return buffers.vertex_buffer_length + vertex_size <= kDefaultBufferSize &&
		buffers.uv_buffer_length + uv_size <= kDefaultBufferSize &&
		buffers.lightmap_uv_buffer_length + uv_size <= kDefaultBufferSize &&
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

void OpenGLEnvironmentResources::Destroy()
{
    panorama_to_cube.reset();
    irradiance.reset();
    prefilter.reset();
    skybox.reset();
    const GLuint textures[] = {panorama, environment_map, irradiance_map, prefiltered_map};
    glDeleteTextures(4, textures);
    panorama = environment_map = irradiance_map = prefiltered_map = 0;
    if (capture_fbo != 0) glDeleteFramebuffers(1, &capture_fbo);
    if (capture_rbo != 0) glDeleteRenderbuffers(1, &capture_rbo);
    if (cube_vbo != 0) glDeleteBuffers(1, &cube_vbo);
    if (cube_vao != 0) glDeleteVertexArrays(1, &cube_vao);
    capture_fbo = capture_rbo = cube_vbo = cube_vao = 0;
}

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
	lightmap_uv_buffer = other.lightmap_uv_buffer;
	normal_buffer = other.normal_buffer;
	tangent_buffer = other.tangent_buffer;
	indice_buffer = other.indice_buffer;
	vertex_buffer_length = other.vertex_buffer_length;
	uv_buffer_length = other.uv_buffer_length;
	lightmap_uv_buffer_length = other.lightmap_uv_buffer_length;
	normal_buffer_length = other.normal_buffer_length;
	tangent_buffer_length = other.tangent_buffer_length;
	indice_buffer_length = other.indice_buffer_length;

	other.vertex_array_obj = 0;
	other.vertex_buffer = 0;
	other.uv_buffer = 0;
	other.lightmap_uv_buffer = 0;
	other.normal_buffer = 0;
	other.tangent_buffer = 0;
	other.indice_buffer = 0;
	other.vertex_buffer_length = 0;
	other.uv_buffer_length = 0;
	other.lightmap_uv_buffer_length = 0;
	other.normal_buffer_length = 0;
	other.tangent_buffer_length = 0;
	other.indice_buffer_length = 0;

	return *this;
}

ShaderBuffers::~ShaderBuffers()
{
	Destroy();
}

void OpenGLFrameResources::Destroy()
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
	if (g_baked_light != 0)
	{
		glDeleteTextures(1, &g_baked_light);
		g_baked_light = 0;
	}
	if (scene_depth != 0)
	{
		glDeleteTextures(1, &scene_depth);
		scene_depth = 0;
	}
	if (opaque_lit_color != 0)
	{
		glDeleteTextures(1, &opaque_lit_color);
		opaque_lit_color = 0;
	}
	if (ssao != 0)
	{
		glDeleteTextures(1, &ssao);
		ssao = 0;
	}
	if (scene_color != 0)
	{
		glDeleteTextures(1, &scene_color);
		scene_color = 0;
	}
	if (g_buffer_fbo != 0)
	{
		glDeleteFramebuffers(1, &g_buffer_fbo);
		g_buffer_fbo = 0;
	}
	if (opaque_lit_fbo != 0)
	{
		glDeleteFramebuffers(1, &opaque_lit_fbo);
		opaque_lit_fbo = 0;
	}
	if (ssao_fbo != 0)
	{
		glDeleteFramebuffers(1, &ssao_fbo);
		ssao_fbo = 0;
	}
	if (scene_color_fbo != 0)
	{
		glDeleteFramebuffers(1, &scene_color_fbo);
		scene_color_fbo = 0;
	}
	if (depth_only_fbo != 0)
	{
		glDeleteFramebuffers(1, &depth_only_fbo);
		depth_only_fbo = 0;
	}
}

void OpenGLRenderQueues::ClearAndReserve(size_t draw_item_count)
{
	opaque_deferred.clear();
	forward.clear();
	transparent.clear();
	shadow_casters.clear();

	if (opaque_deferred.capacity() < draw_item_count)
	{
		opaque_deferred.reserve(draw_item_count);
		forward.reserve(draw_item_count);
		transparent.reserve(draw_item_count);
		shadow_casters.reserve(draw_item_count);
	}
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
	if (lightmap_uv_buffer != 0)
	{
		glDeleteBuffers(1, &lightmap_uv_buffer);
		lightmap_uv_buffer = 0;
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
	lightmap_uv_buffer_length = 0;
	normal_buffer_length = 0;
	tangent_buffer_length = 0;
	indice_buffer_length = 0;
}

bool OpenGLRenderBackend::Initialize(GLFWwindow* /*window*/, int in_window_width, int in_window_height)
{
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	glEnable(GL_CULL_FACE);
	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

	if (!CreateFrameResources(in_window_width, in_window_height))
	{
		DestroyFrameResources();
		return false;
	}
	if (!shadow_system.Initialize())
	{
		DestroyFrameResources();
		return false;
	}

	shader_passes.ssao = std::make_unique<SSAO>();
	shader_passes.ssao->SetTextures(frame_resources.opaque_lit_color, frame_resources.scene_depth,
		frame_resources.g_normal_roughness, frame_resources.ssao, in_window_width, in_window_height);

	window_width = in_window_width;
	window_height = in_window_height;
	return true;
}

void OpenGLRenderBackend::Shutdown()
{
	environment_resources.Destroy();
	shader_passes = OpenGLShaderPasses();

	for (auto& it : material_resources)
	{
		it.second.Destroy();
	}
	material_resources.clear();
	for (const auto& lightmap : lightmap_resources)
	{
		if (lightmap.second != 0) glDeleteTextures(1, &lightmap.second);
	}
	lightmap_resources.clear();

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
	shadow_system.Destroy();

	shader_buffer_dict.clear();
	draw_items.clear();
	render_queues.ClearAndReserve(0);
	window_width = 0;
	window_height = 0;
}

bool OpenGLRenderBackend::Resize(int in_window_width, int in_window_height)
{
	if (in_window_width <= 0 || in_window_height <= 0)
	{
		return false;
	}
	if (in_window_width == window_width && in_window_height == window_height)
	{
		return true;
	}

	DestroyFrameResources();
	if (!CreateFrameResources(in_window_width, in_window_height))
	{
		DestroyFrameResources();
		return false;
	}
	if (shader_passes.ssao != nullptr)
	{
		shader_passes.ssao->SetTextures(frame_resources.opaque_lit_color, frame_resources.scene_depth,
			frame_resources.g_normal_roughness, frame_resources.ssao, in_window_width, in_window_height);
	}
	window_width = in_window_width;
	window_height = in_window_height;
	return true;
}

bool OpenGLRenderBackend::RegisterRenderable(std::uint32_t slot, const Renderable& renderable)
{
	assert(renderable.mesh != nullptr);
	assert(renderable.material != nullptr);

	const std::unordered_map<Material*, MeshData>& mesh_data_dict = renderable.mesh->GetMaterialMeshData();
	const auto mesh_data_it = mesh_data_dict.find(renderable.material);
	if (mesh_data_it == mesh_data_dict.end())
	{
		return false;
	}

	{
		Material* material = renderable.material;
		const MeshData& current_mesh_data = mesh_data_it->second;
		if (current_mesh_data.vertices.empty())
		{
			return false;
		}

		assert(current_mesh_data.uvs.size() == current_mesh_data.vertices.size());
		assert(current_mesh_data.lightmap_uvs.size() == current_mesh_data.vertices.size());
		assert(current_mesh_data.normals.size() == current_mesh_data.vertices.size());
		assert(current_mesh_data.tangents.size() == current_mesh_data.vertices.size());

		size_t vertex_size = current_mesh_data.vertices.size() * sizeof(glm::vec3);
		size_t uv_size = current_mesh_data.uvs.size() * sizeof(glm::vec2);
		size_t normal_size = current_mesh_data.normals.size() * sizeof(glm::vec3);
		size_t tangent_size = current_mesh_data.tangents.size() * sizeof(glm::vec3);
			
		// This assumes that the materials and shader are both already registered.
		ShaderBuffers* current_buffers = &shader_buffer_dict.at(material->shader_type);
		if (!CanAppendBufferData(*current_buffers, vertex_size, uv_size, normal_size, tangent_size))
		{
			std::cerr << "OpenGL mesh buffer capacity exceeded; renderable was not registered.\n";
			return false;
		}

			glBindBuffer(GL_ARRAY_BUFFER, current_buffers->vertex_buffer);
			glBufferSubData(GL_ARRAY_BUFFER, current_buffers->vertex_buffer_length,
				vertex_size, current_mesh_data.vertices.data());
			glBindBuffer(GL_ARRAY_BUFFER, current_buffers->uv_buffer);
			glBufferSubData(GL_ARRAY_BUFFER, current_buffers->uv_buffer_length,
				uv_size, current_mesh_data.uvs.data());
			glBindBuffer(GL_ARRAY_BUFFER, current_buffers->lightmap_uv_buffer);
			glBufferSubData(GL_ARRAY_BUFFER, current_buffers->lightmap_uv_buffer_length,
				uv_size, current_mesh_data.lightmap_uvs.data());
			glBindBuffer(GL_ARRAY_BUFFER, current_buffers->normal_buffer);
			glBufferSubData(GL_ARRAY_BUFFER, current_buffers->normal_buffer_length,
				normal_size, current_mesh_data.normals.data());
			glBindBuffer(GL_ARRAY_BUFFER, current_buffers->tangent_buffer);
			glBufferSubData(GL_ARRAY_BUFFER, current_buffers->tangent_buffer_length,
				tangent_size, current_mesh_data.tangents.data());
			glBindBuffer(GL_ARRAY_BUFFER, 0);

		OpenGLDrawItem draw_item;
		draw_item.material = material;
		draw_item.transform = renderable.transform;
		draw_item.world_bounds = renderable.world_bounds;
		draw_item.start_index = static_cast<GLint>(current_buffers->vertex_buffer_length / sizeof(glm::vec3));
		draw_item.count = static_cast<GLsizei>(current_mesh_data.vertices.size());
		draw_item.flags = renderable.flags;
		draw_item.vertex_array_obj = current_buffers->vertex_array_obj;
		const OpenGLMaterialResources& resources = material_resources.at(material);
		draw_item.albedo_tex = resources.albedo_tex;
		draw_item.normal_tex = resources.normal_tex;
		draw_item.metallic_roughness_ao_tex = resources.metallic_roughness_ao_tex;
		if (renderable.lightmap != nullptr)
		{
			const auto lightmap = lightmap_resources.find(renderable.lightmap);
			if (lightmap != lightmap_resources.end()) draw_item.lightmap_tex = lightmap->second;
		}
		draw_item.lightmap_scale = renderable.lightmap_scale;
		draw_item.lightmap_offset = renderable.lightmap_offset;
		draw_item.lightmap_rgbm_range = renderable.lightmap_rgbm_range;
		if (slot >= draw_items.size()) draw_items.resize(static_cast<std::size_t>(slot) + 1);
		draw_items[slot] = draw_item;

		current_buffers->vertex_buffer_length += vertex_size;
		current_buffers->uv_buffer_length += uv_size;
		current_buffers->lightmap_uv_buffer_length += uv_size;
		current_buffers->normal_buffer_length += normal_size;
		current_buffers->tangent_buffer_length += tangent_size;
	}
	return true;
}

void OpenGLRenderBackend::UpdateRenderableTransform(std::uint32_t slot, const glm::mat4& transform,
	const Bounds& world_bounds)
{
	if (slot >= draw_items.size())
	{
		return;
	}

	draw_items[slot].transform = transform;
	draw_items[slot].world_bounds = world_bounds;
}

void OpenGLRenderBackend::RemoveRenderable(std::uint32_t slot)
{
	if (slot >= draw_items.size()) return;
	draw_items[slot].material = nullptr;
	draw_items[slot].count = 0;
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

	glGenBuffers(1, &buffers.lightmap_uv_buffer);
	glEnableVertexAttribArray(4);
	glBindBuffer(GL_ARRAY_BUFFER, buffers.lightmap_uv_buffer);
	glBufferData(GL_ARRAY_BUFFER, kDefaultBufferSize, nullptr, GL_STATIC_DRAW);
	glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
	
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

bool OpenGLRenderBackend::RegisterMaterial(Material* material)
{
	assert(material != nullptr);

	if (shader_buffer_dict.find(material->shader_type) == shader_buffer_dict.end())
	{
		shader_buffer_dict[material->shader_type] = InitShaderBuffers();
	}

	OpenGLMaterialResources resources;
	resources.albedo_tex = material->GetAlbedoPath().empty() ?
		OpenGLTexture::CreateSolid(255, 255, 255) : OpenGLTexture::LoadDDS(material->GetAlbedoPath());
	resources.normal_tex = material->GetNormalPath().empty() ?
		OpenGLTexture::CreateSolid(128, 128, 255) : OpenGLTexture::LoadDDS(material->GetNormalPath());
	resources.metallic_roughness_ao_tex = material->GetMetallicRoughnessAoPath().empty() ?
		OpenGLTexture::CreateSolid(255, 255, 255) :
		OpenGLTexture::LoadDDS(material->GetMetallicRoughnessAoPath());
	if (resources.albedo_tex == 0 || resources.normal_tex == 0 || resources.metallic_roughness_ao_tex == 0)
	{
		resources.Destroy();
		return false;
	}
	material_resources.emplace(material, std::move(resources));
	return true;
}

void OpenGLRenderBackend::RemoveMaterial(Material* material)
{
	const auto found = material_resources.find(material);
	if (found == material_resources.end()) return;
	found->second.Destroy();
	material_resources.erase(found);
}

bool OpenGLRenderBackend::RegisterLightmap(const Lightmap* lightmap)
{
	assert(lightmap != nullptr);
	// Blender bake pixels already use OpenGL's bottom-left origin. Ordinary
	// source-image DDS files need nv_dds' vertical flip; generated lightmaps do not.
	const GLuint texture = OpenGLTexture::LoadDDS(lightmap->path, false);
	if (texture == 0) return false;
	lightmap_resources.emplace(lightmap, texture);
	return true;
}

void OpenGLRenderBackend::RemoveLightmap(const Lightmap* lightmap)
{
	const auto found = lightmap_resources.find(lightmap);
	if (found == lightmap_resources.end()) return;
	if (found->second != 0) glDeleteTextures(1, &found->second);
	lightmap_resources.erase(found);
}

PBRStandard* OpenGLRenderBackend::GetShader(MaterialShaderType shader_type)
{
	switch (shader_type)
	{
	case MaterialShaderType::PBRStandard:
		if (shader_passes.pbr_forward == nullptr)
		{
			shader_passes.pbr_forward = std::make_unique<PBRStandard>();
		}
		return shader_passes.pbr_forward.get();
	case MaterialShaderType::Unknown:
	case MaterialShaderType::Unlit:
		return nullptr;
	}

	return nullptr;
}

bool OpenGLRenderBackend::CreateFrameResources(int width, int height)
{
	OpenGLFrameResources& resources = frame_resources;

	glGenFramebuffers(1, &resources.g_buffer_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, resources.g_buffer_fbo);

	glGenTextures(1, &resources.g_albedo);
	ConfigureFramebufferTexture(resources.g_albedo, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, width, height);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, resources.g_albedo, 0);

	glGenTextures(1, &resources.g_normal_roughness);
	ConfigureFramebufferTexture(resources.g_normal_roughness, GL_RGBA16F, GL_RGBA, GL_FLOAT, width, height);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, resources.g_normal_roughness, 0);

	glGenTextures(1, &resources.g_material);
	ConfigureFramebufferTexture(resources.g_material, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, width, height);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, resources.g_material, 0);

	glGenTextures(1, &resources.g_baked_light);
	ConfigureFramebufferTexture(resources.g_baked_light, GL_RGB16F, GL_RGB, GL_FLOAT, width, height);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, resources.g_baked_light, 0);

	glGenTextures(1, &resources.scene_depth);
	ConfigureFramebufferTexture(resources.scene_depth, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_FLOAT, width, height);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, resources.scene_depth, 0);

	const GLenum g_buffer_attachments[4] = {
		GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3 };
	glDrawBuffers(4, g_buffer_attachments);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		std::cout << "OpenGL G-buffer framebuffer is incomplete.\n";
		return false;
	}

	glGenFramebuffers(1, &resources.opaque_lit_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, resources.opaque_lit_fbo);

	glGenTextures(1, &resources.opaque_lit_color);
	ConfigureFramebufferTexture(resources.opaque_lit_color, GL_RGBA16F, GL_RGBA, GL_FLOAT, width, height);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, resources.opaque_lit_color, 0);

	const GLenum color_attachment = GL_COLOR_ATTACHMENT0;
	glDrawBuffers(1, &color_attachment);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		std::cout << "OpenGL opaque lighting framebuffer is incomplete.\n";
		return false;
	}

	glGenFramebuffers(1, &resources.ssao_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, resources.ssao_fbo);
	glGenTextures(1, &resources.ssao);
	ConfigureFramebufferTexture(resources.ssao, GL_R8, GL_RED, GL_UNSIGNED_BYTE, width, height);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, resources.ssao, 0);
	glDrawBuffers(1, &color_attachment);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		std::cout << "OpenGL SSAO framebuffer is incomplete.\n";
		return false;
	}

	glGenFramebuffers(1, &resources.scene_color_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, resources.scene_color_fbo);

	glGenTextures(1, &resources.scene_color);
	ConfigureFramebufferTexture(resources.scene_color, GL_RGBA16F, GL_RGBA, GL_FLOAT, width, height);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, resources.scene_color, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, resources.scene_depth, 0);
	glDrawBuffers(1, &color_attachment);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		std::cout << "OpenGL scene color framebuffer is incomplete.\n";
		return false;
	}

	std::cout << "OpenGL G-buffer ready: albedo, normal/roughness, material/flags, baked light, depth.\n";
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	return true;
}

void OpenGLRenderBackend::DestroyFrameResources()
{
	frame_resources.Destroy();
}

void OpenGLRenderBackend::BuildRenderQueues()
{
	render_queues.ClearAndReserve(draw_items.size());
	const RenderFrameConstants& frame = RenderSystem::GetFrameConstants();
	const Frustum camera_frustum = ExtractFrustum(frame.proj * frame.view);

	for (uint32_t index = 0; index < draw_items.size(); ++index)
	{
		const OpenGLDrawItem& item = draw_items[index];
		if (item.material == nullptr || item.count == 0) continue;
		if ((item.flags & RenderableFlagShadowOnly) == 0 &&
			IntersectsFrustum(item.world_bounds, camera_frustum))
		{
			switch (ClassifyRenderMode(item.material->GetRenderMode()))
			{
			case OpenGLRenderQueue::DeferredOpaque:
				render_queues.opaque_deferred.push_back(index);
				break;
			case OpenGLRenderQueue::ForwardOpaque:
				render_queues.forward.push_back(index);
				break;
			case OpenGLRenderQueue::Transparent:
				render_queues.transparent.push_back(index);
				break;
			case OpenGLRenderQueue::None:
				break;
			}
		}

		if (DrawsShadowCaster(item))
		{
			render_queues.shadow_casters.push_back(index);
		}
	}

	const glm::vec3 camera_position = frame.camera_position;
	const auto distance_to_camera = [&](uint32_t index) {
		const glm::vec3 offset = BoundsCenter(draw_items[index].world_bounds,
			draw_items[index].transform) - camera_position;
		return glm::dot(offset, offset);
	};
	const auto front_to_back = [&](uint32_t left, uint32_t right) {
		return distance_to_camera(left) < distance_to_camera(right);
	};
	std::sort(render_queues.opaque_deferred.begin(), render_queues.opaque_deferred.end(), front_to_back);
	std::sort(render_queues.forward.begin(), render_queues.forward.end(), front_to_back);
	std::sort(render_queues.transparent.begin(), render_queues.transparent.end(),
		[&](uint32_t left_index, uint32_t right_index) {
			return distance_to_camera(left_index) > distance_to_camera(right_index);
		});
}

void OpenGLRenderBackend::RenderGeometryPass()
{
	if (shader_passes.pbr_geometry == nullptr)
	{
		shader_passes.pbr_geometry = std::make_unique<PBRGeometryPass>();
	}

	glBindFramebuffer(GL_FRAMEBUFFER, frame_resources.g_buffer_fbo);
	glViewport(0, 0, window_width, window_height);
	const GLenum attachments[4] = {
		GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3 };
	glDrawBuffers(4, attachments);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
	glEnable(GL_CULL_FACE);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	shader_passes.pbr_geometry->Use();
	GLuint bound_vertex_array = 0;
	for (uint32_t draw_index : render_queues.opaque_deferred)
	{
		const OpenGLDrawItem& item = draw_items[draw_index];

		if (bound_vertex_array != item.vertex_array_obj)
		{
			glBindVertexArray(item.vertex_array_obj);
			bound_vertex_array = item.vertex_array_obj;
		}
		shader_passes.pbr_geometry->SetTextures(item.albedo_tex, item.normal_tex,
			item.metallic_roughness_ao_tex, item.lightmap_tex);
		shader_passes.pbr_geometry->Update(item.transform, *item.material, item.lightmap_scale,
			item.lightmap_offset, item.lightmap_rgbm_range);
		DrawItem(item);
	}
	glBindVertexArray(0);
}

void OpenGLRenderBackend::RenderDeferredLightingPass()
{
	if (shader_passes.deferred_lighting == nullptr)
	{
		shader_passes.deferred_lighting = std::make_unique<DeferredLighting>();
	}

	glBindFramebuffer(GL_FRAMEBUFFER, frame_resources.opaque_lit_fbo);
	glViewport(0, 0, window_width, window_height);
	const GLenum attachment = GL_COLOR_ATTACHMENT0;
	glDrawBuffers(1, &attachment);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	glClear(GL_COLOR_BUFFER_BIT);

	shader_passes.deferred_lighting->Use();
	shader_passes.deferred_lighting->Update(frame_resources.g_albedo, frame_resources.g_normal_roughness,
		frame_resources.g_material, frame_resources.g_baked_light, frame_resources.scene_depth,
		window_width, window_height,
		shadow_system.GetGpuData(), shadow_system.GetAtlasTexture(), shadow_system.GetPointTextures(),
		environment_resources.irradiance_map, environment_resources.prefiltered_map);
	RenderScreenSpaceQuad();
}

void OpenGLRenderBackend::RenderAmbientOcclusionPass()
{
	assert(shader_passes.ssao != nullptr);
	glBindFramebuffer(GL_FRAMEBUFFER, frame_resources.ssao_fbo);
	glViewport(0, 0, window_width, window_height);
	const GLenum attachment = GL_COLOR_ATTACHMENT0;
	glDrawBuffers(1, &attachment);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);

	shader_passes.ssao->UseCompute();
	shader_passes.ssao->UpdateCompute();
	RenderScreenSpaceQuad();

	glBindFramebuffer(GL_FRAMEBUFFER, frame_resources.scene_color_fbo);
	shader_passes.ssao->UseComposite();
	shader_passes.ssao->UpdateComposite();
	RenderScreenSpaceQuad();
}

bool OpenGLRenderBackend::BuildEnvironmentResources()
{
    const ImageBasedLighting& lighting = RenderSystem::GetImageBasedLighting();
    auto& resources = environment_resources;
    resources.panorama = OpenGLTexture::LoadDDS(lighting.panorama_path.string(), false);
    if (resources.panorama == 0) return false;
    resources.panorama_to_cube = std::make_unique<ShaderProgram>();
    resources.irradiance = std::make_unique<ShaderProgram>();
    resources.prefilter = std::make_unique<ShaderProgram>();
    resources.skybox = std::make_unique<ShaderProgram>();
    if (!resources.panorama_to_cube->Load("shaders/opengl/environment_cube.vert", "shaders/opengl/equirectangular_to_cube.frag") ||
        !resources.irradiance->Load("shaders/opengl/environment_cube.vert", "shaders/opengl/irradiance_convolution.frag") ||
        !resources.prefilter->Load("shaders/opengl/environment_cube.vert", "shaders/opengl/prefilter_environment.frag") ||
        !resources.skybox->Load("shaders/opengl/skybox.vert", "shaders/opengl/skybox.frag")) return false;

    constexpr float vertices[] = {
        -1,-1,-1, -1,-1, 1, -1, 1, 1, -1, 1, 1, -1,-1,-1, -1, 1,-1,
         1,-1, 1,  1,-1,-1,  1, 1,-1,  1, 1,-1,  1, 1, 1,  1,-1, 1,
        -1,-1, 1, -1,-1,-1,  1,-1,-1,  1,-1,-1,  1,-1, 1, -1,-1, 1,
        -1, 1,-1, -1, 1, 1,  1, 1, 1,  1, 1, 1,  1, 1,-1, -1, 1,-1,
        -1,-1, 1,  1,-1, 1,  1, 1, 1,  1, 1, 1, -1, 1, 1, -1,-1, 1,
         1,-1,-1, -1,-1,-1, -1, 1,-1, -1, 1,-1,  1, 1,-1,  1,-1,-1};
    glGenVertexArrays(1, &resources.cube_vao);
    glGenBuffers(1, &resources.cube_vbo);
    glBindVertexArray(resources.cube_vao);
    glBindBuffer(GL_ARRAY_BUFFER, resources.cube_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glBindVertexArray(0);
    glGenFramebuffers(1, &resources.capture_fbo);
    glGenRenderbuffers(1, &resources.capture_rbo);
    glBindFramebuffer(GL_FRAMEBUFFER, resources.capture_fbo);
    glBindRenderbuffer(GL_RENDERBUFFER, resources.capture_rbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, resources.capture_rbo);

    const glm::mat4 projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    const glm::vec3 origin(0.0f);
    const std::array<glm::mat4, 6> views = {
        glm::lookAt(origin, glm::vec3(1,0,0), glm::vec3(0,-1,0)),
        glm::lookAt(origin, glm::vec3(-1,0,0), glm::vec3(0,-1,0)),
        glm::lookAt(origin, glm::vec3(0,1,0), glm::vec3(0,0,1)),
        glm::lookAt(origin, glm::vec3(0,-1,0), glm::vec3(0,0,-1)),
        glm::lookAt(origin, glm::vec3(0,0,1), glm::vec3(0,-1,0)),
        glm::lookAt(origin, glm::vec3(0,0,-1), glm::vec3(0,-1,0))};
    auto configure_cube = [](GLuint texture, bool mipmapped) {
        glBindTexture(GL_TEXTURE_CUBE_MAP, texture);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER,
            mipmapped ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    };
    auto allocate_cube = [&](GLuint& texture, int size, bool mipmapped) {
        glGenTextures(1, &texture);
        configure_cube(texture, mipmapped);
        for (int face = 0; face < 6; ++face)
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_RGB16F,
                size, size, 0, GL_RGB, GL_FLOAT, nullptr);
    };
    auto set_capture_matrices = [&](GLuint program) {
        glUniformMatrix4fv(glGetUniformLocation(program, "projection"), 1, GL_FALSE,
            glm::value_ptr(projection));
    };
    auto draw_faces = [&](GLuint program, GLuint target, int size, int mip) {
        glViewport(0, 0, size, size);
        for (int face = 0; face < 6; ++face) {
            glUniformMatrix4fv(glGetUniformLocation(program, "view"), 1, GL_FALSE,
                glm::value_ptr(views[face]));
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, target, mip);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            DrawEnvironmentCube();
        }
    };

    glDisable(GL_CULL_FACE);
    allocate_cube(resources.environment_map, 512, true);
    glBindRenderbuffer(GL_RENDERBUFFER, resources.capture_rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
    resources.panorama_to_cube->Use();
    GLuint program = resources.panorama_to_cube->GetId();
    set_capture_matrices(program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, resources.panorama);
    glUniform1i(glGetUniformLocation(program, "panorama"), 0);
    draw_faces(program, resources.environment_map, 512, 0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, resources.environment_map);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

    allocate_cube(resources.irradiance_map, 32, false);
    glBindRenderbuffer(GL_RENDERBUFFER, resources.capture_rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 32, 32);
    resources.irradiance->Use();
    program = resources.irradiance->GetId();
    set_capture_matrices(program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, resources.environment_map);
    glUniform1i(glGetUniformLocation(program, "environment_map"), 0);
    draw_faces(program, resources.irradiance_map, 32, 0);

    glGenTextures(1, &resources.prefiltered_map);
    configure_cube(resources.prefiltered_map, true);
    constexpr int prefilter_size = 128;
    constexpr int mip_count = 5;
    for (int mip = 0; mip < mip_count; ++mip) {
        const int size = prefilter_size >> mip;
        for (int face = 0; face < 6; ++face)
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, mip, GL_RGB16F,
                size, size, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, mip_count - 1);
    resources.prefilter->Use();
    program = resources.prefilter->GetId();
    set_capture_matrices(program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, resources.environment_map);
    glUniform1i(glGetUniformLocation(program, "environment_map"), 0);
    for (int mip = 0; mip < mip_count; ++mip) {
        const int size = prefilter_size >> mip;
        glBindRenderbuffer(GL_RENDERBUFFER, resources.capture_rbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, size, size);
        glUniform1f(glGetUniformLocation(program, "roughness"), float(mip) / float(mip_count - 1));
        draw_faces(program, resources.prefiltered_map, size, mip);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, window_width, window_height);
    glEnable(GL_CULL_FACE);
    return glGetError() == GL_NO_ERROR;
}

void OpenGLRenderBackend::SyncEnvironmentResources()
{
    if (!RenderSystem::ConsumeImageBasedLightingResourcesDirty()) return;
    environment_resources.Destroy();
    if (RenderSystem::GetImageBasedLighting().enabled && !BuildEnvironmentResources())
        std::cerr << "Failed to build image-based lighting resources.\n";
}

void OpenGLRenderBackend::DrawEnvironmentCube() const
{
    glBindVertexArray(environment_resources.cube_vao);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

void OpenGLRenderBackend::RenderSkybox()
{
    const ImageBasedLighting& lighting = RenderSystem::GetImageBasedLighting();
    if (!lighting.enabled || environment_resources.environment_map == 0 || !environment_resources.skybox) return;
    glBindFramebuffer(GL_FRAMEBUFFER, frame_resources.scene_color_fbo);
    glViewport(0, 0, window_width, window_height);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    environment_resources.skybox->Use();
    const GLuint program = environment_resources.skybox->GetId();
    const RenderFrameConstants& frame = RenderSystem::GetFrameConstants();
    glUniformMatrix4fv(glGetUniformLocation(program, "view"), 1, GL_FALSE, glm::value_ptr(frame.view));
    glUniformMatrix4fv(glGetUniformLocation(program, "projection"), 1, GL_FALSE, glm::value_ptr(frame.proj));
    glUniform1f(glGetUniformLocation(program, "intensity"), lighting.sky_intensity);
    glUniform1f(glGetUniformLocation(program, "rotation_radians"), lighting.rotation_radians);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, environment_resources.environment_map);
    glUniform1i(glGetUniformLocation(program, "environment_map"), 0);
    DrawEnvironmentCube();
    glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
}

void OpenGLRenderBackend::RenderForwardQueue(const std::vector<uint32_t>& queue, bool transparent)
{
	if (queue.empty())
	{
		return;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, frame_resources.scene_color_fbo);
	glViewport(0, 0, window_width, window_height);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

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

	GLuint bound_vertex_array = 0;
	PBRStandard* bound_shader = nullptr;
	for (uint32_t draw_index : queue)
	{
		const OpenGLDrawItem& item = draw_items[draw_index];
		PBRStandard* shader = GetShader(item.material->shader_type);
		if (shader == nullptr)
		{
			continue;
		}

		if (bound_vertex_array != item.vertex_array_obj)
		{
		glBindVertexArray(item.vertex_array_obj);
			bound_vertex_array = item.vertex_array_obj;
		}
		if (bound_shader != shader)
		{
			shader->Use();
			shader->UpdateFrame(shadow_system.GetGpuData(), shadow_system.GetAtlasTexture(),
				shadow_system.GetPointTextures(), environment_resources.irradiance_map,
				environment_resources.prefiltered_map);
			bound_shader = shader;
		}
		shader->SetTextures(item.albedo_tex, item.normal_tex, item.metallic_roughness_ao_tex,
			item.lightmap_tex);
		shader->UpdateObject(item.transform, *item.material, item.lightmap_scale,
			item.lightmap_offset, item.lightmap_rgbm_range);
		DrawItem(item);
	}

	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
	glBindVertexArray(0);
}

void OpenGLRenderBackend::PresentSceneColor()
{
	if (shader_passes.fullscreen_blit == nullptr)
	{
		shader_passes.fullscreen_blit = std::make_unique<FullscreenBlit>();
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, window_width, window_height);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// The HDR scene mip chain is the inexpensive bloom/defocus source. It is
	// generated only after every scene contribution has been rendered.
	glBindTexture(GL_TEXTURE_2D, frame_resources.scene_color);
	glGenerateMipmap(GL_TEXTURE_2D);
	shader_passes.fullscreen_blit->Use();
	shader_passes.fullscreen_blit->Update(frame_resources.scene_color, frame_resources.scene_depth);
	RenderScreenSpaceQuad();
}

void OpenGLRenderBackend::DrawItem(const OpenGLDrawItem& item) const
{
	glDrawArrays(GL_TRIANGLES, item.start_index, item.count);
}

OpenGLRenderQueue OpenGLRenderBackend::ClassifyRenderMode(MaterialRenderMode mode) const
{
	switch (mode)
	{
	case MaterialRenderMode::OpaqueDeferred:
	case MaterialRenderMode::AlphaClip:
	case MaterialRenderMode::AlphaHashDither:
		return OpenGLRenderQueue::DeferredOpaque;
	case MaterialRenderMode::Unlit:
		return OpenGLRenderQueue::ForwardOpaque;
	case MaterialRenderMode::TransparentBlend:
		return OpenGLRenderQueue::Transparent;
	}

	return OpenGLRenderQueue::None;
}

bool OpenGLRenderBackend::DrawsShadowCaster(const OpenGLDrawItem& item) const
{
	return item.material->CastsShadows() && (item.flags & RenderableFlagCastsShadow) != 0;
}

void OpenGLRenderBackend::Render()
{
	SyncEnvironmentResources();
	BuildRenderQueues();
	shadow_system.Render(draw_items, render_queues);
	RenderGeometryPass();
	RenderDeferredLightingPass();
	RenderAmbientOcclusionPass();
	RenderSkybox();
	RenderForwardQueue(render_queues.forward, false);
	RenderForwardQueue(render_queues.transparent, true);
	PresentSceneColor();

	glEnable(GL_DEPTH_TEST);
}

void OpenGLRenderBackend::RenderDepthOnly(const glm::mat4& view, const glm::mat4& projection,
	uint32_t native_depth_texture, int texture_width, int texture_height)
{
	if (native_depth_texture == 0 || texture_width <= 0 || texture_height <= 0)
	{
		return;
	}

	if (frame_resources.depth_only_fbo == 0)
	{
		glGenFramebuffers(1, &frame_resources.depth_only_fbo);
	}
	if (shader_passes.depth_only == nullptr)
	{
		shader_passes.depth_only = std::make_unique<DepthOnly>();
	}

	BuildRenderQueues();

	glBindFramebuffer(GL_FRAMEBUFFER, frame_resources.depth_only_fbo);
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

	shader_passes.depth_only->Use();
	for (uint32_t draw_index : render_queues.shadow_casters)
	{
		const OpenGLDrawItem& item = draw_items[draw_index];

		glBindVertexArray(item.vertex_array_obj);
		shader_passes.depth_only->Update(item.transform, view, projection, *item.material, item.albedo_tex);
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

} // namespace CEngine::Renderer
