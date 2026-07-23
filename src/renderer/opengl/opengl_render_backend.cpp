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
	skybox_view = -1;
	skybox_projection = -1;
	skybox_intensity = -1;
	skybox_rotation = -1;
	skybox_environment_map = -1;
}

void OpenGLMaterialResources::Destroy()
{
	if (owns_albedo_tex && albedo_tex != 0)
	{
		glDeleteTextures(1, &albedo_tex);
	}
	if (owns_normal_tex && normal_tex != 0)
	{
		glDeleteTextures(1, &normal_tex);
	}
	if (owns_metallic_roughness_ao_tex &&
		metallic_roughness_ao_tex != 0)
	{
		glDeleteTextures(1, &metallic_roughness_ao_tex);
	}
	albedo_tex = 0;
	normal_tex = 0;
	metallic_roughness_ao_tex = 0;
	owns_albedo_tex = false;
	owns_normal_tex = false;
	owns_metallic_roughness_ao_tex = false;
}

OpenGLMeshResources::OpenGLMeshResources(OpenGLMeshResources&& other) noexcept
{
	*this = std::move(other);
}

OpenGLMeshResources& OpenGLMeshResources::operator=(
	OpenGLMeshResources&& other) noexcept
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
	vertex_count = other.vertex_count;
	references = other.references;

	other.vertex_array_obj = 0;
	other.vertex_buffer = 0;
	other.uv_buffer = 0;
	other.lightmap_uv_buffer = 0;
	other.normal_buffer = 0;
	other.tangent_buffer = 0;
	other.vertex_count = 0;
	other.references = 0;

	return *this;
}

OpenGLMeshResources::~OpenGLMeshResources()
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

void OpenGLMeshResources::Destroy()
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
	if (vertex_array_obj != 0)
	{
		glDeleteVertexArrays(1, &vertex_array_obj);
		vertex_array_obj = 0;
	}

	vertex_count = 0;
	references = 0;
}

bool OpenGLRenderBackend::Initialize(RenderSystem& in_rendering,
	GLFWwindow* /*window*/, int in_window_width, int in_window_height)
{
	rendering = &in_rendering;
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	glEnable(GL_CULL_FACE);
	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

	if (!CreateFrameResources(in_window_width, in_window_height))
	{
		DestroyFrameResources();
		return false;
	}
	if (!shadow_system.Initialize(*rendering))
	{
		DestroyFrameResources();
		return false;
	}
	default_white_texture = OpenGLTexture::CreateSolid(255, 255, 255);
	default_normal_texture = OpenGLTexture::CreateSolid(128, 128, 255);
	if (default_white_texture == 0 || default_normal_texture == 0)
	{
		if (default_white_texture != 0)
			glDeleteTextures(1, &default_white_texture);
		if (default_normal_texture != 0)
			glDeleteTextures(1, &default_normal_texture);
		default_white_texture = 0;
		default_normal_texture = 0;
		shadow_system.Destroy();
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
	if (default_white_texture != 0)
		glDeleteTextures(1, &default_white_texture);
	if (default_normal_texture != 0)
		glDeleteTextures(1, &default_normal_texture);
	default_white_texture = 0;
	default_normal_texture = 0;
	for (const auto& lightmap : lightmap_resources)
	{
		if (lightmap.second.texture != 0)
			glDeleteTextures(1, &lightmap.second.texture);
	}
	lightmap_resources.clear();
	mesh_resources.clear();

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

	draw_items.clear();
	camera_distance_squared.clear();
	render_queues.ClearAndReserve(0);
	window_width = 0;
	window_height = 0;
	rendering = nullptr;
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

	Material* material = renderable.material;
	const MeshData& mesh_data = mesh_data_it->second;
	auto mesh = mesh_resources.find(&mesh_data);
	if (mesh == mesh_resources.end())
	{
		OpenGLMeshResources uploaded = UploadMesh(mesh_data);
		if (uploaded.vertex_count == 0) return false;
		mesh = mesh_resources.emplace(&mesh_data, std::move(uploaded)).first;
	}

	OpenGLDrawItem draw_item;
	draw_item.mesh_data = &mesh_data;
	draw_item.material = material;
	draw_item.transform = renderable.transform;
	draw_item.world_bounds = renderable.world_bounds;
	draw_item.count = mesh->second.vertex_count;
	draw_item.flags = renderable.flags;
	draw_item.vertex_array_obj = mesh->second.vertex_array_obj;
	const OpenGLMaterialResources& resources = material_resources.at(material);
	draw_item.albedo_tex = resources.albedo_tex;
	draw_item.normal_tex = resources.normal_tex;
	draw_item.metallic_roughness_ao_tex = resources.metallic_roughness_ao_tex;
	if (renderable.lightmap != nullptr)
	{
		const auto lightmap = lightmap_resources.find(renderable.lightmap);
		if (lightmap != lightmap_resources.end())
			draw_item.lightmap_tex = lightmap->second.texture;
	}
	draw_item.lightmap_scale = renderable.lightmap_scale;
	draw_item.lightmap_offset = renderable.lightmap_offset;
	draw_item.lightmap_rgbm_range = renderable.lightmap_rgbm_range;
	if (slot >= draw_items.size())
		draw_items.resize(static_cast<std::size_t>(slot) + 1);
	draw_items[slot] = draw_item;
	++mesh->second.references;
	return true;
}

void OpenGLRenderBackend::UpdateRenderable(std::uint32_t slot,
	const glm::mat4& transform, const Bounds& world_bounds, std::uint32_t flags)
{
	if (slot >= draw_items.size())
	{
		return;
	}

	draw_items[slot].transform = transform;
	draw_items[slot].world_bounds = world_bounds;
	draw_items[slot].flags = flags;
}

void OpenGLRenderBackend::RemoveRenderable(std::uint32_t slot)
{
	if (slot >= draw_items.size()) return;
	const MeshData* mesh_data = draw_items[slot].mesh_data;
	if (mesh_data != nullptr)
	{
		const auto mesh = mesh_resources.find(mesh_data);
		if (mesh != mesh_resources.end())
		{
			assert(mesh->second.references > 0);
			if (--mesh->second.references == 0)
				mesh_resources.erase(mesh);
		}
	}
	draw_items[slot] = {};
}

OpenGLMeshResources OpenGLRenderBackend::UploadMesh(const MeshData& mesh)
{
	OpenGLMeshResources buffers;
	if (mesh.vertices.empty() ||
		mesh.uvs.size() != mesh.vertices.size() ||
		mesh.lightmap_uvs.size() != mesh.vertices.size() ||
		mesh.normals.size() != mesh.vertices.size() ||
		mesh.tangents.size() != mesh.vertices.size())
		return buffers;

	while (glGetError() != GL_NO_ERROR) {
	}

	glGenVertexArrays(1, &buffers.vertex_array_obj);
	glBindVertexArray(buffers.vertex_array_obj);

	glGenBuffers(1, &buffers.vertex_buffer);
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, buffers.vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, mesh.vertices.size() * sizeof(glm::vec3),
		mesh.vertices.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

	glGenBuffers(1, &buffers.uv_buffer);
	glEnableVertexAttribArray(1);
	glBindBuffer(GL_ARRAY_BUFFER, buffers.uv_buffer);
	glBufferData(GL_ARRAY_BUFFER, mesh.uvs.size() * sizeof(glm::vec2),
		mesh.uvs.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);

	glGenBuffers(1, &buffers.lightmap_uv_buffer);
	glEnableVertexAttribArray(4);
	glBindBuffer(GL_ARRAY_BUFFER, buffers.lightmap_uv_buffer);
	glBufferData(GL_ARRAY_BUFFER, mesh.lightmap_uvs.size() * sizeof(glm::vec2),
		mesh.lightmap_uvs.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);

	glGenBuffers(1, &buffers.normal_buffer);
	glEnableVertexAttribArray(2);
	glBindBuffer(GL_ARRAY_BUFFER, buffers.normal_buffer);
	glBufferData(GL_ARRAY_BUFFER, mesh.normals.size() * sizeof(glm::vec3),
		mesh.normals.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

	glGenBuffers(1, &buffers.tangent_buffer);
	glEnableVertexAttribArray(3);
	glBindBuffer(GL_ARRAY_BUFFER, buffers.tangent_buffer);
	glBufferData(GL_ARRAY_BUFFER, mesh.tangents.size() * sizeof(glm::vec3),
		mesh.tangents.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	if (glGetError() != GL_NO_ERROR)
	{
		buffers.Destroy();
		return {};
	}
	buffers.vertex_count = static_cast<GLsizei>(mesh.vertices.size());
	return buffers;
}

bool OpenGLRenderBackend::RegisterMaterial(Material* material)
{
	assert(material != nullptr);
	if (material_resources.find(material) != material_resources.end())
		return true;

	OpenGLMaterialResources resources;
	resources.owns_albedo_tex = !material->albedo.Empty();
	resources.owns_normal_tex = !material->normal.Empty();
	resources.owns_metallic_roughness_ao_tex =
		!material->metallic_roughness_ao.Empty();
	resources.albedo_tex = resources.owns_albedo_tex
		? OpenGLTexture::Load(material->albedo)
		: default_white_texture;
	resources.normal_tex = resources.owns_normal_tex
		? OpenGLTexture::Load(material->normal)
		: default_normal_texture;
	resources.metallic_roughness_ao_tex =
		resources.owns_metallic_roughness_ao_tex
		? OpenGLTexture::Load(material->metallic_roughness_ao)
		: default_white_texture;
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

bool OpenGLRenderBackend::RegisterLightmap(const Texture* lightmap)
{
	assert(lightmap != nullptr);
	const auto found = lightmap_resources.find(lightmap);
	if (found != lightmap_resources.end())
	{
		++found->second.references;
		return true;
	}

	const GLuint texture = OpenGLTexture::Load(*lightmap);
	if (texture == 0) return false;
	lightmap_resources.emplace(
		lightmap, OpenGLLightmapResources{texture, 1});
	return true;
}

void OpenGLRenderBackend::RemoveLightmap(const Texture* lightmap)
{
	const auto found = lightmap_resources.find(lightmap);
	if (found == lightmap_resources.end()) return;
	if (--found->second.references != 0) return;
	if (found->second.texture != 0)
		glDeleteTextures(1, &found->second.texture);
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
	const RenderFrameConstants& frame = rendering->GetFrameConstants();
	const Frustum camera_frustum = ExtractFrustum(frame.proj * frame.view);
	camera_distance_squared.resize(draw_items.size());

	for (uint32_t index = 0; index < draw_items.size(); ++index)
	{
		const OpenGLDrawItem& item = draw_items[index];
		if (item.material == nullptr || item.count == 0) continue;
		if ((item.flags & RenderableFlagVisible) == 0) continue;
		const glm::vec3 camera_offset =
			BoundsCenter(item.world_bounds, item.transform) -
			frame.camera_position;
		camera_distance_squared[index] =
			glm::dot(camera_offset, camera_offset);
		if ((item.flags & RenderableFlagShadowOnly) == 0 &&
			IntersectsFrustum(item.world_bounds, camera_frustum))
		{
			switch (ClassifyRenderMode(item.material->render_mode))
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

	const auto front_to_back = [&](uint32_t left, uint32_t right) {
		return camera_distance_squared[left] <
			camera_distance_squared[right];
	};
	std::sort(render_queues.opaque_deferred.begin(), render_queues.opaque_deferred.end(), front_to_back);
	std::sort(render_queues.forward.begin(), render_queues.forward.end(), front_to_back);
	std::sort(render_queues.transparent.begin(), render_queues.transparent.end(),
		[&](uint32_t left_index, uint32_t right_index) {
			return camera_distance_squared[left_index] >
				camera_distance_squared[right_index];
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
	shader_passes.pbr_geometry->UpdateFrame(*rendering);
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
		shader_passes.pbr_geometry->UpdateObject(
			item.transform, *item.material, item.lightmap_scale,
			item.lightmap_offset, item.lightmap_rgbm_range);
		DrawItem(item);
	}
	glBindVertexArray(0);
}

void OpenGLRenderBackend::RenderDeferredLightingPass()
{
	if (shader_passes.deferred_lighting == nullptr)
	{
		shader_passes.deferred_lighting =
			std::make_unique<DeferredLighting>();
	}

	glBindFramebuffer(GL_FRAMEBUFFER, frame_resources.opaque_lit_fbo);
	glViewport(0, 0, window_width, window_height);
	const GLenum attachment = GL_COLOR_ATTACHMENT0;
	glDrawBuffers(1, &attachment);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	glClear(GL_COLOR_BUFFER_BIT);

	shader_passes.deferred_lighting->Use();
	shader_passes.deferred_lighting->Update(*rendering,
		frame_resources.g_albedo, frame_resources.g_normal_roughness,
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
	shader_passes.ssao->UpdateCompute(*rendering);
	RenderScreenSpaceQuad();

	glBindFramebuffer(GL_FRAMEBUFFER, frame_resources.scene_color_fbo);
	shader_passes.ssao->UseComposite();
	shader_passes.ssao->UpdateComposite(*rendering);
	RenderScreenSpaceQuad();
}

bool OpenGLRenderBackend::BuildEnvironmentResources()
{
    const ImageBasedLighting& lighting = rendering->GetImageBasedLighting();
    auto& resources = environment_resources;
    if (lighting.panorama == nullptr) return false;
    resources.panorama = OpenGLTexture::Load(*lighting.panorama);
    if (resources.panorama == 0) return false;
    resources.panorama_to_cube = std::make_unique<ShaderProgram>();
    resources.irradiance = std::make_unique<ShaderProgram>();
    resources.prefilter = std::make_unique<ShaderProgram>();
    resources.skybox = std::make_unique<ShaderProgram>();
    if (!resources.panorama_to_cube->Load("shaders/opengl/environment_cube.vert", "shaders/opengl/equirectangular_to_cube.frag") ||
        !resources.irradiance->Load("shaders/opengl/environment_cube.vert", "shaders/opengl/irradiance_convolution.frag") ||
        !resources.prefilter->Load("shaders/opengl/environment_cube.vert", "shaders/opengl/prefilter_environment.frag") ||
        !resources.skybox->Load("shaders/opengl/skybox.vert", "shaders/opengl/skybox.frag")) return false;
	const GLuint skybox_program = resources.skybox->GetId();
	resources.skybox_view =
		glGetUniformLocation(skybox_program, "view");
	resources.skybox_projection =
		glGetUniformLocation(skybox_program, "projection");
	resources.skybox_intensity =
		glGetUniformLocation(skybox_program, "intensity");
	resources.skybox_rotation =
		glGetUniformLocation(skybox_program, "rotation_radians");
	resources.skybox_environment_map =
		glGetUniformLocation(skybox_program, "environment_map");

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
    if (!rendering->ConsumeImageBasedLightingResourcesDirty()) return;
    environment_resources.Destroy();
    if (rendering->GetImageBasedLighting().enabled && !BuildEnvironmentResources())
	{
        std::cerr << "Failed to build image-based lighting resources.\n";
		environment_resources.Destroy();
	}
}

void OpenGLRenderBackend::DrawEnvironmentCube() const
{
    glBindVertexArray(environment_resources.cube_vao);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

void OpenGLRenderBackend::RenderSkybox()
{
    const ImageBasedLighting& lighting = rendering->GetImageBasedLighting();
    if (!lighting.enabled || environment_resources.environment_map == 0 || !environment_resources.skybox) return;
    glBindFramebuffer(GL_FRAMEBUFFER, frame_resources.scene_color_fbo);
    glViewport(0, 0, window_width, window_height);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    environment_resources.skybox->Use();
    const RenderFrameConstants& frame = rendering->GetFrameConstants();
    glUniformMatrix4fv(environment_resources.skybox_view,
		1, GL_FALSE, glm::value_ptr(frame.view));
    glUniformMatrix4fv(environment_resources.skybox_projection,
		1, GL_FALSE, glm::value_ptr(frame.proj));
    glUniform1f(
		environment_resources.skybox_intensity, lighting.sky_intensity);
    glUniform1f(
		environment_resources.skybox_rotation, lighting.rotation_radians);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, environment_resources.environment_map);
    glUniform1i(environment_resources.skybox_environment_map, 0);
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
			shader->UpdateFrame(*rendering,
				shadow_system.GetGpuData(), shadow_system.GetAtlasTexture(),
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

	shader_passes.fullscreen_blit->Use();
	shader_passes.fullscreen_blit->Update(
		*rendering, frame_resources.scene_color, frame_resources.scene_depth);
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
	return item.material->casts_shadows &&
		(item.flags & RenderableFlagCastsShadow) != 0;
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
