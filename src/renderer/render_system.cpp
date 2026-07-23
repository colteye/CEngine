#include "renderer/render_system.h"

#if defined(CENGINE_ENABLE_OPENGL)
#include "renderer/opengl/opengl_render_backend.h"
#endif

#if defined(CENGINE_ENABLE_VULKAN)
#include "renderer/vulkan/vulkan_render_backend.h"
#endif

#include <algorithm>

namespace {
constexpr size_t kMaxGpuLights = 64;
}

namespace CEngine::Renderer {

static_assert(sizeof(GpuLight) == sizeof(glm::vec4) * 4, "GpuLight must stay tightly packed as four vec4 lanes.");

std::unique_ptr<IRenderBackend> RenderSystem::backend;

std::vector<Renderable> RenderSystem::renderables;
std::vector<std::uint32_t> RenderSystem::renderable_generations;
std::vector<std::uint32_t> RenderSystem::free_renderables;
std::vector<LightRecord> RenderSystem::direct_lights;
std::vector<std::uint32_t> RenderSystem::light_generations;
std::vector<std::uint32_t> RenderSystem::free_lights;
std::vector<GpuLight> RenderSystem::gpu_lights;
std::vector<LightShadowGpuHandle> RenderSystem::light_shadow_handles;
RenderFrameConstants RenderSystem::frame_constants;
AmbientLighting RenderSystem::ambient_lighting;
ImageBasedLighting RenderSystem::image_based_lighting;
ExponentialHeightFog RenderSystem::exponential_height_fog;
PostProcessSettings RenderSystem::post_process_settings;
SSAOSettings RenderSystem::ssao_settings;
bool RenderSystem::image_based_lighting_resources_dirty = true;
uint64_t RenderSystem::light_revision = 1;
bool RenderSystem::lights_dirty = true;

bool RenderSystem::Initialize(GLFWwindow* window, int window_width, int window_height)
{
	Shutdown();

#if defined(CENGINE_ENABLE_OPENGL)
	backend = std::make_unique<OpenGLRenderBackend>();
#elif defined(CENGINE_ENABLE_VULKAN)
	backend = std::make_unique<VulkanRenderBackend>();
#endif

	if (!backend->Initialize(window, window_width, window_height))
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
	renderable_generations.clear();
	free_renderables.clear();
	direct_lights.clear();
	light_generations.clear();
	free_lights.clear();
	gpu_lights.clear();
	light_shadow_handles.clear();
	post_process_settings = {};
	ssao_settings = {};
	light_revision = 1;
	lights_dirty = true;
}

bool RenderSystem::Resize(int window_width, int window_height)
{
	return backend != nullptr && backend->Resize(window_width, window_height);
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

	RenderableHandle handle;
	if (free_renderables.empty())
	{
		handle = {static_cast<std::uint32_t>(renderables.size()), 1};
		renderables.push_back(stored);
		renderable_generations.push_back(handle.generation);
	}
	else
	{
		handle.index = free_renderables.back();
		free_renderables.pop_back();
		handle.generation = renderable_generations[handle.index];
		renderables[handle.index] = stored;
	}

	if (backend != nullptr && !backend->RegisterRenderable(handle.index, stored))
	{
		renderables[handle.index] = {};
		++renderable_generations[handle.index];
		if (renderable_generations[handle.index] == 0) ++renderable_generations[handle.index];
		free_renderables.push_back(handle.index);
		return {};
	}

	return handle;
}

void RenderSystem::RemoveRenderable(RenderableHandle handle)
{
	if (ResolveRenderable(handle) == nullptr) return;
	if (backend != nullptr) backend->RemoveRenderable(handle.index);
	renderables[handle.index] = {};
	++renderable_generations[handle.index];
	if (renderable_generations[handle.index] == 0) ++renderable_generations[handle.index];
	free_renderables.push_back(handle.index);
}

void RenderSystem::UpdateRenderableTransform(RenderableHandle handle, const glm::mat4& transform)
{
	if (ResolveRenderable(handle) == nullptr) return;

	Renderable& renderable = renderables[handle.index];
	renderable.transform = transform;
	renderable.world_bounds = TransformBounds(renderable.local_bounds, renderable.transform);
	if (backend != nullptr)
	{
		backend->UpdateRenderableTransform(handle.index, renderable.transform, renderable.world_bounds);
	}
}

bool RenderSystem::RegisterMaterial(Material* material)
{
	return backend == nullptr || backend->RegisterMaterial(material);
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
	LightHandle handle;
	if (free_lights.empty())
	{
		handle = {static_cast<std::uint32_t>(direct_lights.size()), 1};
		direct_lights.push_back(light);
		light_shadow_handles.emplace_back();
		light_generations.push_back(handle.generation);
	}
	else
	{
		handle.index = free_lights.back();
		free_lights.pop_back();
		handle.generation = light_generations[handle.index];
		direct_lights[handle.index] = light;
		light_shadow_handles[handle.index] = {};
	}
	lights_dirty = true;
	++light_revision;
	return handle;
}

void RenderSystem::RemoveLight(LightHandle id)
{
	if (ResolveLight(id) == nullptr) return;
	direct_lights[id.index].enabled = false;
	light_shadow_handles[id.index] = {};
	++light_generations[id.index];
	if (light_generations[id.index] == 0) ++light_generations[id.index];
	free_lights.push_back(id.index);
	lights_dirty = true;
	++light_revision;
}

void RenderSystem::UpdateLight(LightHandle id, const LightRecord& light)
{
	if (ResolveLight(id) == nullptr) return;
	direct_lights[id.index] = light;
	lights_dirty = true;
	++light_revision;
}

const std::vector<Renderable>& RenderSystem::GetRenderables()
{
	return renderables;
}

const std::vector<LightRecord>& RenderSystem::GetDirectLights()
{
	return direct_lights;
}

const Renderable* RenderSystem::ResolveRenderable(RenderableHandle handle)
{
	return handle.index < renderables.size() &&
		renderable_generations[handle.index] == handle.generation ? &renderables[handle.index] : nullptr;
}

const LightRecord* RenderSystem::ResolveLight(LightHandle handle)
{
	return handle.index < direct_lights.size() &&
		light_generations[handle.index] == handle.generation ? &direct_lights[handle.index] : nullptr;
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

void RenderSystem::SetImageBasedLighting(const ImageBasedLighting& lighting)
{
	const bool resources_changed = image_based_lighting.enabled != lighting.enabled ||
		image_based_lighting.panorama_path != lighting.panorama_path;
	image_based_lighting = lighting;
	image_based_lighting_resources_dirty = image_based_lighting_resources_dirty || resources_changed;
}

const ImageBasedLighting& RenderSystem::GetImageBasedLighting()
{
	return image_based_lighting;
}

void RenderSystem::SetExponentialHeightFog(const ExponentialHeightFog& fog)
{
	exponential_height_fog = fog;
}

const ExponentialHeightFog& RenderSystem::GetExponentialHeightFog()
{
	return exponential_height_fog;
}

void RenderSystem::SetPostProcessSettings(const PostProcessSettings& settings)
{
	post_process_settings = settings;
}

const PostProcessSettings& RenderSystem::GetPostProcessSettings()
{
	return post_process_settings;
}

void RenderSystem::SetSSAOSettings(const SSAOSettings& settings)
{
	ssao_settings = settings;
}

const SSAOSettings& RenderSystem::GetSSAOSettings()
{
	return ssao_settings;
}

bool RenderSystem::ConsumeImageBasedLightingResourcesDirty()
{
	const bool result = image_based_lighting_resources_dirty;
	image_based_lighting_resources_dirty = false;
	return result;
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
