#include "renderer/render_system.h"

#ifdef CENGINE_ENABLE_OPENGL
#include "renderer/opengl/opengl_render_backend.h"
#endif

#ifdef CENGINE_ENABLE_VULKAN
#include "renderer/vulkan/vulkan_render_backend.h"
#endif

#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
bool SameLight(const CEngine::Renderer::Light &left, const CEngine::Renderer::Light &right)
{
    return left.type == right.type && left.position == right.position && left.direction == right.direction &&
           left.color == right.color && left.intensity == right.intensity && left.range == right.range &&
           left.spot_inner_cos == right.spot_inner_cos && left.spot_outer_cos == right.spot_outer_cos &&
           left.enabled == right.enabled && left.casts_shadows == right.casts_shadows &&
           left.shadow_resolution == right.shadow_resolution && left.shadow_bias == right.shadow_bias &&
           left.shadow_normal_bias == right.shadow_normal_bias;
}
} // namespace

namespace CEngine::Renderer
{

static_assert(sizeof(GpuLight) == sizeof(glm::vec4) * 4, "GpuLight must stay tightly packed as four vec4 lanes.");

RenderSystem::~RenderSystem()
{
    Shutdown();
}

bool RenderSystem::Initialize(GLFWwindow *window, int window_width, int window_height)
{
    Shutdown();

#ifdef CENGINE_ENABLE_OPENGL
    backend_ = std::make_unique<OpenGLRenderBackend>();
#elif defined(CENGINE_ENABLE_VULKAN)
    backend = std::make_unique<VulkanRenderBackend>();
#endif

    if (!backend_->Initialize(*this, window, window_width, window_height))
    {
        backend_.reset();
        return false;
    }

    gpu_lights_.reserve(MaxGpuLights);
    return true;
}

void RenderSystem::Shutdown()
{
    if (backend_ != nullptr)
    {
        backend_->Shutdown();
        backend_.reset();
    }

    mesh_instances_.clear();
    mesh_instance_local_bounds_.clear();
    mesh_instance_world_bounds_.clear();
    mesh_instance_generations_.clear();
    free_mesh_instances_.clear();
    mesh_instance_revision_ = 1;
    direct_lights_.clear();
    light_generations_.clear();
    free_lights_.clear();
    gpu_lights_.clear();
    light_shadow_handles_.clear();
    frame_constants_ = {};
    active_camera_ = {};
    camera_aspect_ratio_ = 4.0f / 3.0f;
    ambient_lighting_ = {};
    image_based_lighting_ = {};
    exponential_height_fog_ = {};
    post_process_settings_ = {};
    ssao_settings_ = {};
    image_based_lighting_resources_dirty_ = true;
    light_revision_ = 1;
    light_state_revision_ = 1;
    lights_dirty_ = true;
}

bool RenderSystem::Resize(int window_width, int window_height)
{
    return backend_ != nullptr && backend_->Resize(window_width, window_height);
}

void RenderSystem::Render()
{
    if (backend_ != nullptr)
    {
        backend_->Render();
    }
}

MeshInstanceHandle RenderSystem::RegisterMeshInstance(const MeshInstance &mesh_instance)
{
    if (mesh_instance.mesh == nullptr || mesh_instance.mesh->Empty() || mesh_instance.material == nullptr)
    {
        return {};
    }

    const Bounds local_bounds = mesh_instance.mesh->local_bounds;
    const Bounds world_bounds = TransformBounds(local_bounds, mesh_instance.transform);

    MeshInstanceHandle handle;
    if (free_mesh_instances_.empty())
    {
        handle = {static_cast<std::uint32_t>(mesh_instances_.size()), 1};
        mesh_instances_.push_back(mesh_instance);
        mesh_instance_local_bounds_.push_back(local_bounds);
        mesh_instance_world_bounds_.push_back(world_bounds);
        mesh_instance_generations_.push_back(handle.generation);
    }
    else
    {
        handle.index = free_mesh_instances_.back();
        free_mesh_instances_.pop_back();
        handle.generation = mesh_instance_generations_[handle.index];
        mesh_instances_[handle.index] = mesh_instance;
        mesh_instance_local_bounds_[handle.index] = local_bounds;
        mesh_instance_world_bounds_[handle.index] = world_bounds;
    }

    if (backend_ != nullptr && !backend_->RegisterMeshInstance(handle.index, mesh_instance, world_bounds))
    {
        mesh_instances_[handle.index] = {};
        mesh_instance_local_bounds_[handle.index] = {};
        mesh_instance_world_bounds_[handle.index] = {};
        ++mesh_instance_generations_[handle.index];
        if (mesh_instance_generations_[handle.index] == 0)
        {
            ++mesh_instance_generations_[handle.index];
        }
        free_mesh_instances_.push_back(handle.index);
        return {};
    }

    ++mesh_instance_revision_;
    return handle;
}

void RenderSystem::RemoveMeshInstance(MeshInstanceHandle handle)
{
    if (ResolveMeshInstance(handle) == nullptr)
    {
        return;
    }
    if (backend_ != nullptr)
    {
        backend_->RemoveMeshInstance(handle.index);
    }
    mesh_instances_[handle.index] = {};
    mesh_instance_local_bounds_[handle.index] = {};
    mesh_instance_world_bounds_[handle.index] = {};
    ++mesh_instance_generations_[handle.index];
    if (mesh_instance_generations_[handle.index] == 0)
    {
        ++mesh_instance_generations_[handle.index];
    }
    free_mesh_instances_.push_back(handle.index);
    ++mesh_instance_revision_;
}

void RenderSystem::UpdateMeshInstance(MeshInstanceHandle handle, const glm::mat4 &transform, std::uint32_t flags)
{
    if (ResolveMeshInstance(handle) == nullptr)
    {
        return;
    }

    MeshInstance &mesh_instance = mesh_instances_[handle.index];
    if (mesh_instance.flags == flags &&
        std::memcmp(&mesh_instance.transform[0][0], &transform[0][0], sizeof(glm::mat4)) == 0)
    {
        return;
    }
    mesh_instance.transform = transform;
    mesh_instance.flags = flags;
    mesh_instance_world_bounds_[handle.index] = TransformBounds(mesh_instance_local_bounds_[handle.index], transform);
    if (backend_ != nullptr)
    {
        backend_->UpdateMeshInstance(handle.index, transform, mesh_instance_world_bounds_[handle.index], flags);
    }
    ++mesh_instance_revision_;
}

LightHandle RenderSystem::RegisterLight(const Light &light)
{
    LightHandle handle;
    if (free_lights_.empty())
    {
        handle = {static_cast<std::uint32_t>(direct_lights_.size()), 1};
        direct_lights_.push_back(light);
        light_shadow_handles_.emplace_back();
        light_generations_.push_back(handle.generation);
    }
    else
    {
        handle.index = free_lights_.back();
        free_lights_.pop_back();
        handle.generation = light_generations_[handle.index];
        direct_lights_[handle.index] = light;
        light_shadow_handles_[handle.index] = {};
    }
    lights_dirty_ = true;
    ++light_revision_;
    ++light_state_revision_;
    return handle;
}

void RenderSystem::RemoveLight(LightHandle id)
{
    if (ResolveLight(id) == nullptr)
    {
        return;
    }
    direct_lights_[id.index].enabled = false;
    light_shadow_handles_[id.index] = {};
    ++light_generations_[id.index];
    if (light_generations_[id.index] == 0)
    {
        ++light_generations_[id.index];
    }
    free_lights_.push_back(id.index);
    lights_dirty_ = true;
    ++light_revision_;
    ++light_state_revision_;
}

void RenderSystem::UpdateLight(LightHandle id, const Light &light)
{
    if (ResolveLight(id) == nullptr)
    {
        return;
    }
    if (SameLight(direct_lights_[id.index], light))
    {
        return;
    }
    direct_lights_[id.index] = light;
    lights_dirty_ = true;
    ++light_revision_;
    ++light_state_revision_;
}

const std::vector<MeshInstance> &RenderSystem::GetMeshInstances() const
{
    return mesh_instances_;
}

std::uint64_t RenderSystem::GetMeshInstanceRevision() const
{
    return mesh_instance_revision_;
}

const std::vector<Light> &RenderSystem::GetDirectLights() const
{
    return direct_lights_;
}

const MeshInstance *RenderSystem::ResolveMeshInstance(MeshInstanceHandle handle) const
{
    return handle.index < mesh_instances_.size() && mesh_instance_generations_[handle.index] == handle.generation
               ? &mesh_instances_[handle.index]
               : nullptr;
}

const Light *RenderSystem::ResolveLight(LightHandle handle) const
{
    return handle.index < direct_lights_.size() && light_generations_[handle.index] == handle.generation
               ? &direct_lights_[handle.index]
               : nullptr;
}

const std::vector<GpuLight> &RenderSystem::GetGpuLights()
{
    if (lights_dirty_)
    {
        RebuildGpuLights();
    }
    return gpu_lights_;
}

std::uint64_t RenderSystem::GetLightRevision() const
{
    return light_revision_;
}

std::uint64_t RenderSystem::GetLightStateRevision() const
{
    return light_state_revision_;
}

void RenderSystem::SetLightShadowHandles(const std::vector<LightShadowBinding> &handles)
{
    if (light_shadow_handles_ == handles)
    {
        return;
    }

    light_shadow_handles_ = handles;
    if (light_shadow_handles_.size() < direct_lights_.size())
    {
        light_shadow_handles_.resize(direct_lights_.size());
    }

    lights_dirty_ = true;
    ++light_revision_;
}

const RenderFrameConstants &RenderSystem::GetFrameConstants() const
{
    return frame_constants_;
}

void RenderSystem::UpdateCamera(const Camera &camera)
{
    active_camera_ = camera;
    frame_constants_ = active_camera_.FrameConstants(camera_aspect_ratio_);
}

void RenderSystem::SetCameraAspectRatio(float aspect_ratio)
{
    if (!std::isfinite(aspect_ratio) || aspect_ratio <= 0.0f)
    {
        return;
    }
    camera_aspect_ratio_ = aspect_ratio;
    frame_constants_ = active_camera_.FrameConstants(camera_aspect_ratio_);
}

const Camera &RenderSystem::ActiveCamera() const
{
    return active_camera_;
}

void RenderSystem::SetAmbientLighting(const AmbientLighting &ambient)
{
    ambient_lighting_ = ambient;
}

const AmbientLighting &RenderSystem::GetAmbientLighting() const
{
    return ambient_lighting_;
}

void RenderSystem::SetImageBasedLighting(const ImageBasedLighting &lighting)
{
    const bool resources_changed =
        image_based_lighting_.enabled != lighting.enabled || image_based_lighting_.panorama != lighting.panorama;
    image_based_lighting_ = lighting;
    image_based_lighting_resources_dirty_ = image_based_lighting_resources_dirty_ || resources_changed;
}

const ImageBasedLighting &RenderSystem::GetImageBasedLighting() const
{
    return image_based_lighting_;
}

void RenderSystem::SetExponentialHeightFog(const ExponentialHeightFog &fog)
{
    exponential_height_fog_ = fog;
}

const ExponentialHeightFog &RenderSystem::GetExponentialHeightFog() const
{
    return exponential_height_fog_;
}

void RenderSystem::SetPostProcessSettings(const PostProcessSettings &settings)
{
    post_process_settings_ = settings;
}

const PostProcessSettings &RenderSystem::GetPostProcessSettings() const
{
    return post_process_settings_;
}

void RenderSystem::SetSSAOSettings(const SSAOSettings &settings)
{
    ssao_settings_ = settings;
}

const SSAOSettings &RenderSystem::GetSSAOSettings() const
{
    return ssao_settings_;
}

bool RenderSystem::ConsumeImageBasedLightingResourcesDirty()
{
    const bool result = image_based_lighting_resources_dirty_;
    image_based_lighting_resources_dirty_ = false;
    return result;
}

void RenderSystem::RebuildGpuLights()
{
    gpu_lights_.clear();

    for (size_t light_index = 0; light_index < direct_lights_.size(); ++light_index)
    {
        const Light &light = direct_lights_[light_index];
        if (!light.enabled)
        {
            continue;
        }
        if (gpu_lights_.size() >= MaxGpuLights)
        {
            break;
        }

        GpuLight gpu_light{};
        gpu_light.position_range = glm::vec4(light.position, light.range);
        gpu_light.direction_spot = glm::vec4(light.direction, light.spot_outer_cos);
        gpu_light.color_intensity = glm::vec4(light.color, light.intensity);
        LightShadowBinding shadow_handle;
        if (light_index < light_shadow_handles_.size())
        {
            shadow_handle = light_shadow_handles_[light_index];
        }
        gpu_light.params = glm::vec4(static_cast<float>(light.type), light.spot_inner_cos,
                                     static_cast<float>(shadow_handle.index), static_cast<float>(shadow_handle.type));
        gpu_lights_.push_back(gpu_light);
    }

    lights_dirty_ = false;
}

} // namespace CEngine::Renderer
