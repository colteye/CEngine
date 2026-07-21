#include "renderer/render_system.h"

#if defined(CENGINE_ENABLE_OPENGL)
#include "renderer/opengl/opengl_render_backend.h"
#endif

#if defined(CENGINE_ENABLE_VULKAN)
#include "renderer/vulkan/vulkan_render_backend.h"
#endif

#include <algorithm>
#include <iostream>

namespace {
constexpr size_t kMaxGpuLights = 64;
}

namespace CEngine::Renderer {

static_assert(sizeof(GpuLight) == sizeof(glm::vec4) * 4, "GpuLight must stay tightly packed as four vec4 lanes.");

std::unique_ptr<IRenderBackend> RenderSystem::backend;

std::vector<Renderable> RenderSystem::renderables;
std::vector<LightRecord> RenderSystem::direct_lights;
std::vector<GpuLight> RenderSystem::gpu_lights;
std::vector<LightShadowGpuHandle> RenderSystem::light_shadow_handles;
RenderFrameConstants RenderSystem::frame_constants;
AmbientLighting RenderSystem::ambient_lighting;
uint64_t RenderSystem::light_revision = 1;
bool RenderSystem::lights_dirty = true;

void RenderSystem::Initialize(int window_width, int window_height)
{
#if defined(CENGINE_ENABLE_OPENGL)
	(void)Initialize(RenderBackendType::OpenGL, nullptr, window_width, window_height);
#elif defined(CENGINE_ENABLE_VULKAN)
	(void)Initialize(RenderBackendType::Vulkan, nullptr, window_width, window_height);
#else
	std::cout << "No render backend is enabled for this build.\n";
#endif
}

bool RenderSystem::Initialize(RenderBackendType backend_type, GLFWwindow* window, int window_width, int window_height)
{
	Shutdown();

	switch (backend_type)
	{
	case RenderBackendType::OpenGL:
#if defined(CENGINE_ENABLE_OPENGL)
		backend = std::make_unique<OpenGLRenderBackend>();
#else
		std::cout << "OpenGL backend was requested, but this build was compiled without OpenGL support.\n";
		return false;
#endif
		break;
	case RenderBackendType::Vulkan:
#if defined(CENGINE_ENABLE_VULKAN)
		backend = std::make_unique<VulkanRenderBackend>();
#else
		std::cout << "Vulkan backend was requested, but this build was compiled without Vulkan support.\n";
		return false;
#endif
		break;
	}

	if (backend == nullptr || !backend->Initialize(window, window_width, window_height))
	{
		backend.reset();
		return false;
	}

	gpu_lights.reserve(kMaxGpuLights);
	return true;
}

void RenderSystem::Shutdown()
{
	if (backend != nullptr)
	{
		backend->Shutdown();
		backend.reset();
	}

	renderables.clear();
	direct_lights.clear();
	gpu_lights.clear();
	light_shadow_handles.clear();
	light_revision = 1;
	lights_dirty = true;
}

void RenderSystem::Render()
{
	if (backend != nullptr)
	{
		backend->Render();
	}
}

void RenderSystem::RenderDepthOnly(const glm::mat4& view, const glm::mat4& projection,
	uint32_t native_depth_texture, int texture_width, int texture_height)
{
	if (backend != nullptr)
	{
		backend->RenderDepthOnly(view, projection, native_depth_texture, texture_width, texture_height);
	}
}

void RenderSystem::RegisterMesh(const Mesh* mesh)
{
	if (mesh == nullptr)
	{
		return;
	}

	for (const auto& it : mesh->GetMaterialMeshData())
	{
		Renderable renderable;
		renderable.mesh = mesh;
		renderable.material = it.first;
		renderable.transform = glm::mat4(1.0f);
		renderable.local_bounds = it.second.local_bounds.valid ? it.second.local_bounds : CalculateBounds(it.second.vertices);
		renderable.world_bounds = TransformBounds(renderable.local_bounds, renderable.transform);
		RegisterRenderable(renderable);
	}
}

RenderableHandle RenderSystem::RegisterRenderable(const Renderable& renderable)
{
	Renderable stored = renderable;
	if (stored.mesh != nullptr && !stored.local_bounds.valid)
	{
		stored.local_bounds = stored.mesh->GetLocalBounds();
	}
	stored.world_bounds = TransformBounds(stored.local_bounds, stored.transform);

	const size_t id = renderables.size();
	renderables.push_back(stored);

	if (backend != nullptr)
	{
		backend->RegisterRenderable(renderables.back());
	}

	return static_cast<RenderableHandle>(id);
}

void RenderSystem::RemoveRenderable(RenderableHandle handle)
{
	if (handle >= renderables.size()) return;
	renderables[handle] = {};
	if (backend != nullptr) backend->RemoveRenderable(handle);
}

void RenderSystem::UpdateRenderableTransform(RenderableHandle handle, const glm::mat4& transform)
{
	if (handle >= renderables.size())
	{
		return;
	}

	Renderable& renderable = renderables[handle];
	renderable.transform = transform;
	renderable.world_bounds = TransformBounds(renderable.local_bounds, renderable.transform);
	if (backend != nullptr)
	{
		backend->UpdateRenderableTransform(handle, renderable.transform, renderable.world_bounds);
	}
}

void RenderSystem::RegisterMaterial(Material* material)
{
	if (backend != nullptr)
	{
		backend->RegisterMaterial(material);
	}
}

void RenderSystem::RemoveMaterial(Material* material)
{
	if (backend != nullptr) backend->RemoveMaterial(material);
}

bool RenderSystem::RegisterLightmap(const Lightmap* lightmap)
{
	return backend == nullptr || backend->RegisterLightmap(lightmap);
}

void RenderSystem::RemoveLightmap(const Lightmap* lightmap)
{
	if (backend != nullptr) backend->RemoveLightmap(lightmap);
}

LightHandle RenderSystem::RegisterLight(const glm::vec3& light_pos, const glm::vec3& light_col, float light_pow)
{
	LightRecord light;
	light.type = LightType::Point;
	light.position = light_pos;
	light.color = light_col;
	light.intensity = light_pow;
	return RegisterLight(light);
}

void RenderSystem::UpdateLight(LightHandle id, const glm::vec3& light_pos, const glm::vec3& light_col, float light_pow)
{
	LightRecord light;
	light.type = LightType::Point;
	light.position = light_pos;
	light.color = light_col;
	light.intensity = light_pow;
	UpdateLight(id, light);
}

LightHandle RenderSystem::RegisterLight(const LightRecord& light)
{
	const size_t id = direct_lights.size();
	direct_lights.push_back(light);
	light_shadow_handles.emplace_back();
	lights_dirty = true;
	++light_revision;
	return static_cast<LightHandle>(id);
}

void RenderSystem::RemoveLight(LightHandle id)
{
	if (id >= direct_lights.size()) return;
	direct_lights[id].enabled = false;
	lights_dirty = true;
	++light_revision;
}

void RenderSystem::UpdateLight(LightHandle id, const LightRecord& light)
{
	if (id < direct_lights.size())
	{
		direct_lights[id] = light;
		lights_dirty = true;
		++light_revision;
	}
}

const std::vector<Renderable>& RenderSystem::GetRenderables()
{
	return renderables;
}

const std::vector<LightRecord>& RenderSystem::GetDirectLights()
{
	return direct_lights;
}

const std::vector<GpuLight>& RenderSystem::GetGpuLights()
{
	if (lights_dirty)
	{
		RebuildGpuLights();
	}
	return gpu_lights;
}

uint64_t RenderSystem::GetLightRevision()
{
	return light_revision;
}

size_t RenderSystem::GetMaxGpuLights()
{
	return kMaxGpuLights;
}

void RenderSystem::SetLightShadowHandles(const std::vector<LightShadowGpuHandle>& handles)
{
	if (light_shadow_handles == handles)
	{
		return;
	}

	light_shadow_handles = handles;
	if (light_shadow_handles.size() < direct_lights.size())
	{
		light_shadow_handles.resize(direct_lights.size());
	}

	lights_dirty = true;
	++light_revision;
}

void RenderSystem::SetFrameConstants(const RenderFrameConstants& constants)
{
	frame_constants = constants;
}

const RenderFrameConstants& RenderSystem::GetFrameConstants()
{
	return frame_constants;
}

void RenderSystem::SetAmbientLighting(const AmbientLighting& ambient)
{
	ambient_lighting = ambient;
}

const AmbientLighting& RenderSystem::GetAmbientLighting()
{
	return ambient_lighting;
}

void RenderSystem::RebuildGpuLights()
{
	gpu_lights.clear();

	for (size_t light_index = 0; light_index < direct_lights.size(); ++light_index)
	{
		const LightRecord& light = direct_lights[light_index];
		if (!light.enabled)
		{
			continue;
		}
		if (gpu_lights.size() >= kMaxGpuLights)
		{
			break;
		}

		GpuLight gpu_light;
		gpu_light.position_range = glm::vec4(light.position, light.range);
		gpu_light.direction_spot = glm::vec4(light.direction, light.spot_outer_cos);
		gpu_light.color_intensity = glm::vec4(light.color, light.intensity);
		LightShadowGpuHandle shadow_handle;
		if (light_index < light_shadow_handles.size())
		{
			shadow_handle = light_shadow_handles[light_index];
		}
		gpu_light.params = glm::vec4(static_cast<float>(light.type), light.spot_inner_cos,
			static_cast<float>(shadow_handle.index), static_cast<float>(shadow_handle.type));
		gpu_lights.push_back(gpu_light);
	}

	lights_dirty = false;
}

} // namespace CEngine::Renderer
