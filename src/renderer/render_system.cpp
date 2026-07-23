//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/renderer/render_system.cpp
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#include "renderer/render_system.h"
#include "window/window_system.h"

#ifdef CENGINE_ENABLE_OPENGL
#include "renderer/opengl/render_backend.h"
#endif

#ifdef CENGINE_ENABLE_VULKAN
#include "renderer/vulkan/vulkan_render_backend.h"
#endif

#include <algorithm>
#include <cmath>

namespace
{
/**
 * @brief TODO: Describe SameLight.
 *
 * @param left TODO: Describe this parameter.
 * @param right TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
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

/**
 * @brief TODO: Describe RenderSystem::~RenderSystem.
 */
RenderSystem::~RenderSystem()
{
    Shutdown();
}

/**
 * @brief TODO: Describe RenderSystem::Initialize.
 *
 * @param window TODO: Describe this parameter.
 * @param window_width TODO: Describe this parameter.
 * @param window_height TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool RenderSystem::Initialize(Window::WindowSystem &window)
{
    Shutdown();
    const glm::ivec2 drawable_size = window.DrawableSize();
    if (drawable_size.x <= 0 || drawable_size.y <= 0)
    {
        return false;
    }

#ifdef CENGINE_ENABLE_OPENGL
    backend_ = std::make_unique<OpenGL::RenderBackend>();
#elif defined(CENGINE_ENABLE_VULKAN)
    backend_ = std::make_unique<VulkanRenderBackend>();
#endif

    if (backend_ == nullptr || !backend_->Initialize(*this, window, drawable_size.x, drawable_size.y))
    {
        backend_.reset();
        return false;
    }

    gpu_lights_.reserve(MaxGpuLights);
    return true;
}

/**
 * @brief TODO: Describe RenderSystem::Shutdown.
 */
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
    camera_frame_data_ = {};
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

/**
 * @brief TODO: Describe RenderSystem::Resize.
 *
 * @param window_width TODO: Describe this parameter.
 * @param window_height TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool RenderSystem::Resize(int window_width, int window_height)
{
    return backend_ != nullptr && backend_->Resize(window_width, window_height);
}

/**
 * @brief TODO: Describe RenderSystem::Render.
 */
void RenderSystem::Render()
{
    if (backend_ != nullptr)
    {
        backend_->Render();
    }
}

/**
 * @brief TODO: Describe RenderSystem::RegisterMeshInstance.
 *
 * @param mesh_instance TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
MeshInstanceHandle RenderSystem::RegisterMeshInstance(const MeshInstance &mesh_instance)
{
    if (mesh_instance.mesh == nullptr || mesh_instance.mesh->Empty() || mesh_instance.material == nullptr)
    {
        return {};
    }

    const Bounds local_bounds = mesh_instance.mesh->local_bounds;
    const Bounds world_bounds = TransformBounds(local_bounds, mesh_instance.transform);

    std::uint32_t index = 0;
    std::uint32_t generation = 1;
    if (free_mesh_instances_.empty())
    {
        index = static_cast<std::uint32_t>(mesh_instances_.size());
        mesh_instances_.push_back(mesh_instance);
        mesh_instance_local_bounds_.push_back(local_bounds);
        mesh_instance_world_bounds_.push_back(world_bounds);
        mesh_instance_generations_.push_back(generation);
    }
    else
    {
        index = free_mesh_instances_.back();
        free_mesh_instances_.pop_back();
        generation = mesh_instance_generations_[index];
        mesh_instances_[index] = mesh_instance;
        mesh_instance_local_bounds_[index] = local_bounds;
        mesh_instance_world_bounds_[index] = world_bounds;
    }
    const MeshInstanceHandle handle{index, generation};

    if (backend_ != nullptr && !backend_->RegisterMeshInstance(index, mesh_instance, world_bounds))
    {
        mesh_instances_[index] = {};
        mesh_instance_local_bounds_[index] = {};
        mesh_instance_world_bounds_[index] = {};
        ++mesh_instance_generations_[index];
        if (mesh_instance_generations_[index] == 0)
        {
            ++mesh_instance_generations_[index];
        }
        free_mesh_instances_.push_back(index);
        return {};
    }

    ++mesh_instance_revision_;
    return handle;
}

/**
 * @brief TODO: Describe RenderSystem::RemoveMeshInstance.
 *
 * @param handle TODO: Describe this parameter.
 */
void RenderSystem::RemoveMeshInstance(MeshInstanceHandle handle)
{
    if (ResolveMeshInstance(handle) == nullptr)
    {
        return;
    }
    const std::uint32_t index = handle.Index();
    if (backend_ != nullptr)
    {
        backend_->RemoveMeshInstance(index);
    }
    mesh_instances_[index] = {};
    mesh_instance_local_bounds_[index] = {};
    mesh_instance_world_bounds_[index] = {};
    ++mesh_instance_generations_[index];
    if (mesh_instance_generations_[index] == 0)
    {
        ++mesh_instance_generations_[index];
    }
    free_mesh_instances_.push_back(index);
    ++mesh_instance_revision_;
}

/**
 * @brief TODO: Describe RenderSystem::UpdateMeshInstance.
 *
 * @param handle TODO: Describe this parameter.
 * @param transform TODO: Describe this parameter.
 * @param flags TODO: Describe this parameter.
 */
void RenderSystem::UpdateMeshInstance(MeshInstanceHandle handle, const glm::mat4 &transform, std::uint32_t flags)
{
    if (ResolveMeshInstance(handle) == nullptr)
    {
        return;
    }

    const std::uint32_t index = handle.Index();
    MeshInstance &mesh_instance = mesh_instances_[index];
    if (mesh_instance.flags == flags && mesh_instance.transform == transform)
    {
        return;
    }
    mesh_instance.transform = transform;
    mesh_instance.flags = flags;
    mesh_instance_world_bounds_[index] = TransformBounds(mesh_instance_local_bounds_[index], transform);
    if (backend_ != nullptr)
    {
        backend_->UpdateMeshInstance(index, transform, mesh_instance_world_bounds_[index], flags);
    }
    ++mesh_instance_revision_;
}

/**
 * @brief TODO: Describe RenderSystem::RegisterLight.
 *
 * @param light TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
LightHandle RenderSystem::RegisterLight(const Light &light)
{
    std::uint32_t index = 0;
    std::uint32_t generation = 1;
    if (free_lights_.empty())
    {
        index = static_cast<std::uint32_t>(direct_lights_.size());
        direct_lights_.push_back(light);
        light_shadow_handles_.emplace_back();
        light_generations_.push_back(generation);
    }
    else
    {
        index = free_lights_.back();
        free_lights_.pop_back();
        generation = light_generations_[index];
        direct_lights_[index] = light;
        light_shadow_handles_[index] = {};
    }
    lights_dirty_ = true;
    ++light_revision_;
    ++light_state_revision_;
    return {index, generation};
}

/**
 * @brief TODO: Describe RenderSystem::RemoveLight.
 *
 * @param id TODO: Describe this parameter.
 */
void RenderSystem::RemoveLight(LightHandle id)
{
    if (ResolveLight(id) == nullptr)
    {
        return;
    }
    const std::uint32_t index = id.Index();
    direct_lights_[index].enabled = false;
    light_shadow_handles_[index] = {};
    ++light_generations_[index];
    if (light_generations_[index] == 0)
    {
        ++light_generations_[index];
    }
    free_lights_.push_back(index);
    lights_dirty_ = true;
    ++light_revision_;
    ++light_state_revision_;
}

/**
 * @brief TODO: Describe RenderSystem::UpdateLight.
 *
 * @param id TODO: Describe this parameter.
 * @param light TODO: Describe this parameter.
 */
void RenderSystem::UpdateLight(LightHandle id, const Light &light)
{
    if (ResolveLight(id) == nullptr)
    {
        return;
    }
    const std::uint32_t index = id.Index();
    if (SameLight(direct_lights_[index], light))
    {
        return;
    }
    direct_lights_[index] = light;
    lights_dirty_ = true;
    ++light_revision_;
    ++light_state_revision_;
}

/**
 * @brief TODO: Describe RenderSystem::GetMeshInstances.
 *
 * @return TODO: Describe the return value.
 */
const std::vector<MeshInstance> &RenderSystem::GetMeshInstances() const
{
    return mesh_instances_;
}

/**
 * @brief TODO: Describe RenderSystem::GetMeshInstanceRevision.
 *
 * @return TODO: Describe the return value.
 */
std::uint64_t RenderSystem::GetMeshInstanceRevision() const
{
    return mesh_instance_revision_;
}

/**
 * @brief TODO: Describe RenderSystem::GetDirectLights.
 *
 * @return TODO: Describe the return value.
 */
const std::vector<Light> &RenderSystem::GetDirectLights() const
{
    return direct_lights_;
}

/**
 * @brief TODO: Describe RenderSystem::ResolveMeshInstance.
 *
 * @param handle TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
const MeshInstance *RenderSystem::ResolveMeshInstance(MeshInstanceHandle handle) const
{
    const std::uint32_t index = handle.Index();
    return handle && index < mesh_instances_.size() && mesh_instance_generations_[index] == handle.Generation()
               ? &mesh_instances_[index]
               : nullptr;
}

/**
 * @brief TODO: Describe RenderSystem::ResolveLight.
 *
 * @param handle TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
const Light *RenderSystem::ResolveLight(LightHandle handle) const
{
    const std::uint32_t index = handle.Index();
    return handle && index < direct_lights_.size() && light_generations_[index] == handle.Generation()
               ? &direct_lights_[index]
               : nullptr;
}

/**
 * @brief TODO: Describe RenderSystem::GetGpuLights.
 *
 * @return TODO: Describe the return value.
 */
const std::vector<GpuLight> &RenderSystem::GetGpuLights()
{
    if (lights_dirty_)
    {
        RebuildGpuLights();
    }
    return gpu_lights_;
}

/**
 * @brief TODO: Describe RenderSystem::GetLightRevision.
 *
 * @return TODO: Describe the return value.
 */
std::uint64_t RenderSystem::GetLightRevision() const
{
    return light_revision_;
}

/**
 * @brief TODO: Describe RenderSystem::GetLightStateRevision.
 *
 * @return TODO: Describe the return value.
 */
std::uint64_t RenderSystem::GetLightStateRevision() const
{
    return light_state_revision_;
}

/**
 * @brief TODO: Describe RenderSystem::SetLightShadowHandles.
 *
 * @param handles TODO: Describe this parameter.
 */
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

/**
 * @brief TODO: Describe RenderSystem::GetCameraFrameData.
 *
 * @return TODO: Describe the return value.
 */
const CameraFrameData &RenderSystem::GetCameraFrameData() const
{
    return camera_frame_data_;
}

/**
 * @brief TODO: Describe RenderSystem::UpdateCamera.
 *
 * @param camera TODO: Describe this parameter.
 */
void RenderSystem::UpdateCamera(const Camera &camera)
{
    active_camera_ = camera;
    camera_frame_data_ = active_camera_.BuildFrameData(camera_aspect_ratio_);
}

/**
 * @brief TODO: Describe RenderSystem::SetCameraAspectRatio.
 *
 * @param aspect_ratio TODO: Describe this parameter.
 */
void RenderSystem::SetCameraAspectRatio(float aspect_ratio)
{
    if (!std::isfinite(aspect_ratio) || aspect_ratio <= 0.0f)
    {
        return;
    }
    camera_aspect_ratio_ = aspect_ratio;
    camera_frame_data_ = active_camera_.BuildFrameData(camera_aspect_ratio_);
}

/**
 * @brief TODO: Describe RenderSystem::ActiveCamera.
 *
 * @return TODO: Describe the return value.
 */
const Camera &RenderSystem::ActiveCamera() const
{
    return active_camera_;
}

/**
 * @brief TODO: Describe RenderSystem::SetAmbientLighting.
 *
 * @param ambient TODO: Describe this parameter.
 */
void RenderSystem::SetAmbientLighting(const AmbientLighting &ambient)
{
    ambient_lighting_ = ambient;
}

/**
 * @brief TODO: Describe RenderSystem::GetAmbientLighting.
 *
 * @return TODO: Describe the return value.
 */
const AmbientLighting &RenderSystem::GetAmbientLighting() const
{
    return ambient_lighting_;
}

/**
 * @brief TODO: Describe RenderSystem::SetImageBasedLighting.
 *
 * @param lighting TODO: Describe this parameter.
 */
void RenderSystem::SetImageBasedLighting(const ImageBasedLighting &lighting)
{
    const bool resources_changed =
        image_based_lighting_.enabled != lighting.enabled || image_based_lighting_.panorama != lighting.panorama;
    image_based_lighting_ = lighting;
    image_based_lighting_resources_dirty_ = image_based_lighting_resources_dirty_ || resources_changed;
}

/**
 * @brief TODO: Describe RenderSystem::GetImageBasedLighting.
 *
 * @return TODO: Describe the return value.
 */
const ImageBasedLighting &RenderSystem::GetImageBasedLighting() const
{
    return image_based_lighting_;
}

/**
 * @brief TODO: Describe RenderSystem::SetExponentialHeightFog.
 *
 * @param fog TODO: Describe this parameter.
 */
void RenderSystem::SetExponentialHeightFog(const ExponentialHeightFog &fog)
{
    exponential_height_fog_ = fog;
}

/**
 * @brief TODO: Describe RenderSystem::GetExponentialHeightFog.
 *
 * @return TODO: Describe the return value.
 */
const ExponentialHeightFog &RenderSystem::GetExponentialHeightFog() const
{
    return exponential_height_fog_;
}

/**
 * @brief TODO: Describe RenderSystem::SetPostProcessSettings.
 *
 * @param settings TODO: Describe this parameter.
 */
void RenderSystem::SetPostProcessSettings(const PostProcessSettings &settings)
{
    post_process_settings_ = settings;
}

/**
 * @brief TODO: Describe RenderSystem::GetPostProcessSettings.
 *
 * @return TODO: Describe the return value.
 */
const PostProcessSettings &RenderSystem::GetPostProcessSettings() const
{
    return post_process_settings_;
}

/**
 * @brief TODO: Describe RenderSystem::SetSSAOSettings.
 *
 * @param settings TODO: Describe this parameter.
 */
void RenderSystem::SetSSAOSettings(const SSAOSettings &settings)
{
    ssao_settings_ = settings;
}

/**
 * @brief TODO: Describe RenderSystem::GetSSAOSettings.
 *
 * @return TODO: Describe the return value.
 */
const SSAOSettings &RenderSystem::GetSSAOSettings() const
{
    return ssao_settings_;
}

/**
 * @brief TODO: Describe RenderSystem::ConsumeImageBasedLightingResourcesDirty.
 *
 * @return TODO: Describe the return value.
 */
bool RenderSystem::ConsumeImageBasedLightingResourcesDirty()
{
    const bool result = image_based_lighting_resources_dirty_;
    image_based_lighting_resources_dirty_ = false;
    return result;
}

/**
 * @brief TODO: Describe RenderSystem::RebuildGpuLights.
 */
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
