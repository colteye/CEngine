#include "renderer/render_system.h"

#if defined(CENGINE_ENABLE_OPENGL)
#include "renderer/opengl/opengl_render_backend.h"
#endif

#if defined(CENGINE_ENABLE_VULKAN)
#include "renderer/vulkan/vulkan_render_backend.h"
#endif

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {
bool SameLight(const CEngine::Renderer::Light& left,
	const CEngine::Renderer::Light& right)
{
	return left.type == right.type &&
		left.position == right.position &&
		left.direction == right.direction &&
		left.color == right.color &&
		left.intensity == right.intensity &&
		left.range == right.range &&
		left.spot_inner_cos == right.spot_inner_cos &&
		left.spot_outer_cos == right.spot_outer_cos &&
		left.enabled == right.enabled &&
		left.casts_shadows == right.casts_shadows &&
		left.shadow_resolution == right.shadow_resolution &&
		left.shadow_bias == right.shadow_bias &&
		left.shadow_normal_bias == right.shadow_normal_bias;
}
}

namespace CEngine::Renderer {

static_assert(sizeof(GpuLight) == sizeof(glm::vec4) * 4, "GpuLight must stay tightly packed as four vec4 lanes.");

RenderSystem::~RenderSystem()
{
	Shutdown();
}

bool RenderSystem::Initialize(GLFWwindow* window, int window_width, int window_height)
{
	Shutdown();

#if defined(CENGINE_ENABLE_OPENGL)
	backend = std::make_unique<OpenGLRenderBackend>();
#elif defined(CENGINE_ENABLE_VULKAN)
	backend = std::make_unique<VulkanRenderBackend>();
#endif

	if (!backend->Initialize(*this, window, window_width, window_height))
	{
		backend.reset();
		return false;
	}

	gpu_lights.reserve(MaxGpuLights);
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
	renderable_revision = 1;
	direct_lights.clear();
	light_generations.clear();
	free_lights.clear();
	gpu_lights.clear();
	light_shadow_handles.clear();
	frame_constants = {};
	active_camera = {};
	camera_aspect_ratio = 4.0f / 3.0f;
	ambient_lighting = {};
	image_based_lighting = {};
	exponential_height_fog = {};
	post_process_settings = {};
	ssao_settings = {};
	image_based_lighting_resources_dirty = true;
	light_revision = 1;
	light_state_revision = 1;
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

	++renderable_revision;
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
	++renderable_revision;
}

void RenderSystem::UpdateRenderable(
	RenderableHandle handle, const glm::mat4& transform, std::uint32_t flags)
{
	if (ResolveRenderable(handle) == nullptr) return;

	Renderable& renderable = renderables[handle.index];
	if (renderable.flags == flags &&
		std::memcmp(&renderable.transform[0][0], &transform[0][0],
			sizeof(glm::mat4)) == 0)
		return;
	renderable.transform = transform;
	renderable.flags = flags;
	renderable.world_bounds = TransformBounds(renderable.local_bounds, renderable.transform);
	if (backend != nullptr)
	{
		backend->UpdateRenderable(
			handle.index, renderable.transform, renderable.world_bounds, flags);
	}
	++renderable_revision;
}

bool RenderSystem::RegisterMaterial(Material* material)
{
	return backend == nullptr || backend->RegisterMaterial(material);
}

void RenderSystem::RemoveMaterial(Material* material)
{
	if (backend != nullptr) backend->RemoveMaterial(material);
}

bool RenderSystem::RegisterLightmap(const Texture* lightmap)
{
	return backend == nullptr || backend->RegisterLightmap(lightmap);
}

void RenderSystem::RemoveLightmap(const Texture* lightmap)
{
	if (backend != nullptr) backend->RemoveLightmap(lightmap);
}

LightHandle RenderSystem::RegisterLight(const Light& light)
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
	++light_state_revision;
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
	++light_state_revision;
}

void RenderSystem::UpdateLight(LightHandle id, const Light& light)
{
	if (ResolveLight(id) == nullptr) return;
	if (SameLight(direct_lights[id.index], light)) return;
	direct_lights[id.index] = light;
	lights_dirty = true;
	++light_revision;
	++light_state_revision;
}

const std::vector<Renderable>& RenderSystem::GetRenderables() const
{
	return renderables;
}

std::uint64_t RenderSystem::GetRenderableRevision() const
{
	return renderable_revision;
}

const std::vector<Light>& RenderSystem::GetDirectLights() const
{
	return direct_lights;
}

const Renderable* RenderSystem::ResolveRenderable(RenderableHandle handle) const
{
	return handle.index < renderables.size() &&
		renderable_generations[handle.index] == handle.generation ? &renderables[handle.index] : nullptr;
}

const Light* RenderSystem::ResolveLight(LightHandle handle) const
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

std::uint64_t RenderSystem::GetLightRevision() const
{
	return light_revision;
}

std::uint64_t RenderSystem::GetLightStateRevision() const
{
	return light_state_revision;
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

const RenderFrameConstants& RenderSystem::GetFrameConstants() const
{
	return frame_constants;
}

void RenderSystem::UpdateCamera(const Camera& camera)
{
	active_camera = camera;
	frame_constants = active_camera.FrameConstants(camera_aspect_ratio);
}

void RenderSystem::SetCameraAspectRatio(float aspect_ratio)
{
	if (!std::isfinite(aspect_ratio) || aspect_ratio <= 0.0f) return;
	camera_aspect_ratio = aspect_ratio;
	frame_constants = active_camera.FrameConstants(camera_aspect_ratio);
}

const Camera& RenderSystem::ActiveCamera() const
{
	return active_camera;
}

void RenderSystem::SetAmbientLighting(const AmbientLighting& ambient)
{
	ambient_lighting = ambient;
}

const AmbientLighting& RenderSystem::GetAmbientLighting() const
{
	return ambient_lighting;
}

void RenderSystem::SetImageBasedLighting(const ImageBasedLighting& lighting)
{
	const bool resources_changed = image_based_lighting.enabled != lighting.enabled ||
		image_based_lighting.panorama != lighting.panorama;
	image_based_lighting = lighting;
	image_based_lighting_resources_dirty = image_based_lighting_resources_dirty || resources_changed;
}

const ImageBasedLighting& RenderSystem::GetImageBasedLighting() const
{
	return image_based_lighting;
}

void RenderSystem::SetExponentialHeightFog(const ExponentialHeightFog& fog)
{
	exponential_height_fog = fog;
}

const ExponentialHeightFog& RenderSystem::GetExponentialHeightFog() const
{
	return exponential_height_fog;
}

void RenderSystem::SetPostProcessSettings(const PostProcessSettings& settings)
{
	post_process_settings = settings;
}

const PostProcessSettings& RenderSystem::GetPostProcessSettings() const
{
	return post_process_settings;
}

void RenderSystem::SetSSAOSettings(const SSAOSettings& settings)
{
	ssao_settings = settings;
}

const SSAOSettings& RenderSystem::GetSSAOSettings() const
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
		const Light& light = direct_lights[light_index];
		if (!light.enabled)
		{
			continue;
		}
		if (gpu_lights.size() >= MaxGpuLights)
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
