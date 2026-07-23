//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/renderer/render_system.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#ifndef RENDER_SYSTEM_H
#define RENDER_SYSTEM_H

#include "renderer/camera.h"
#include "renderer/light.h"
#include "renderer/mesh_instance.h"
#include "renderer/render_backend.h"
#include "renderer/texture.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

struct GLFWwindow;

namespace CEngine::Renderer
{

/**
 * @brief TODO: Describe AmbientLighting.
 */
struct AmbientLighting
{
    glm::vec3 sky_color = glm::vec3(0.36f, 0.42f, 0.52f);
    glm::vec3 ground_color = glm::vec3(0.12f, 0.10f, 0.08f);
    float intensity = 0.18f;
    bool enabled = true;
};

/**
 * @brief TODO: Describe ImageBasedLighting.
 */
struct ImageBasedLighting
{
    std::shared_ptr<const Texture> panorama;
    // The panorama is both visible background and lighting source; they need
    // independent exposure controls for practical scene art direction.
    float sky_intensity = 1.0f;
    float lighting_intensity = 1.0f;
    float rotation_radians = 0.0f;
    bool enabled = false;
};

/**
 * @brief TODO: Describe ExponentialHeightFog.
 */
struct ExponentialHeightFog
{
    glm::vec3 inscattering_color = glm::vec3(0.5f, 0.6f, 0.7f);
    float density = 0.02f;
    float height_falloff = 0.2f;
    float base_height = 0.0f;
    float start_distance = 0.0f;
    float max_opacity = 1.0f;
    float cutoff_distance = 0.0f;
    bool enabled = false;
};

// Presentation-only image treatment. A scene can publish these values through
// its post_process entity, and a game may still override them at runtime.
/**
 * @brief TODO: Describe PostProcessSettings.
 */
struct PostProcessSettings
{
    bool bloom_enabled = true;
    float bloom_threshold = 1.0f;
    float bloom_intensity = 0.12f;
    bool tone_mapping_enabled = true;
    float exposure = 1.0f;
    float contrast = 1.0f;
    float saturation = 1.0f;
    bool depth_of_field_enabled = false;
    float focus_distance = 8.0f;
    float focus_range = 4.0f;
    float depth_of_field_strength = 1.0f;
    bool sun_lens_flare_enabled = true;
    float sun_lens_flare_intensity = 0.35f;
    float sun_disc_size = 0.012f;
    float sun_disc_softness = 0.006f;
};

/**
 * @brief TODO: Describe SSAOSettings.
 */
struct SSAOSettings
{
    bool enabled = true;
    float radius = 0.65f;
    float bias = 0.025f;
    float intensity = 0.65f;
    float contrast = 1.0f;
};

/**
 * @brief TODO: Describe RenderSystem.
 */
class RenderSystem
{
  public:
    static constexpr std::size_t MaxGpuLights = 64;

    /**
     * @brief TODO: Describe RenderSystem.
     */
    RenderSystem() = default;
    /**
     * @brief TODO: Describe ~RenderSystem.
     */
    ~RenderSystem();
    /**
     * @brief TODO: Describe RenderSystem.
     */
    RenderSystem(const RenderSystem &) = delete;
    /**
     * @brief TODO: Describe operator=.
     *
     * @return TODO: Describe the return value.
     */
    RenderSystem &operator=(const RenderSystem &) = delete;
    /**
     * @brief TODO: Describe RenderSystem.
     */
    RenderSystem(RenderSystem &&) = delete;
    /**
     * @brief TODO: Describe operator=.
     *
     * @return TODO: Describe the return value.
     */
    RenderSystem &operator=(RenderSystem &&) = delete;

    /**
     * @brief TODO: Describe Initialize.
     *
     * @param window TODO: Describe this parameter.
     * @param window_width TODO: Describe this parameter.
     * @param window_height TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    bool Initialize(GLFWwindow *window, int window_width, int window_height);
    /**
     * @brief TODO: Describe Shutdown.
     */
    void Shutdown();
    /**
     * @brief TODO: Describe Resize.
     *
     * @param window_width TODO: Describe this parameter.
     * @param window_height TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    bool Resize(int window_width, int window_height);

    /**
     * @brief TODO: Describe Render.
     */
    void Render();

    /**
     * @brief TODO: Describe RegisterMeshInstance.
     *
     * @param mesh_instance TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    MeshInstanceHandle RegisterMeshInstance(const MeshInstance &mesh_instance);
    /**
     * @brief TODO: Describe RemoveMeshInstance.
     *
     * @param handle TODO: Describe this parameter.
     */
    void RemoveMeshInstance(MeshInstanceHandle handle);
    /**
     * @brief TODO: Describe UpdateMeshInstance.
     *
     * @param handle TODO: Describe this parameter.
     * @param transform TODO: Describe this parameter.
     * @param flags TODO: Describe this parameter.
     */
    void UpdateMeshInstance(MeshInstanceHandle handle, const glm::mat4 &transform, std::uint32_t flags);

    /**
     * @brief TODO: Describe RegisterLight.
     *
     * @param light TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    LightHandle RegisterLight(const Light &light);
    /**
     * @brief TODO: Describe RemoveLight.
     *
     * @param id TODO: Describe this parameter.
     */
    void RemoveLight(LightHandle id);
    /**
     * @brief TODO: Describe UpdateLight.
     *
     * @param id TODO: Describe this parameter.
     * @param light TODO: Describe this parameter.
     */
    void UpdateLight(LightHandle id, const Light &light);

    /**
     * @brief TODO: Describe GetMeshInstances.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] const std::vector<MeshInstance> &GetMeshInstances() const;
    /**
     * @brief TODO: Describe GetMeshInstanceRevision.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] std::uint64_t GetMeshInstanceRevision() const;
    /**
     * @brief TODO: Describe GetDirectLights.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] const std::vector<Light> &GetDirectLights() const;
    /**
     * @brief TODO: Describe ResolveMeshInstance.
     *
     * @param handle TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] const MeshInstance *ResolveMeshInstance(MeshInstanceHandle handle) const;
    /**
     * @brief TODO: Describe ResolveLight.
     *
     * @param handle TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] const Light *ResolveLight(LightHandle handle) const;
    /**
     * @brief TODO: Describe GetGpuLights.
     *
     * @return TODO: Describe the return value.
     */
    const std::vector<GpuLight> &GetGpuLights();
    /**
     * @brief TODO: Describe GetLightRevision.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] std::uint64_t GetLightRevision() const;
    /**
     * @brief TODO: Describe GetLightStateRevision.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] std::uint64_t GetLightStateRevision() const;
    /**
     * @brief TODO: Describe SetLightShadowHandles.
     *
     * @param handles TODO: Describe this parameter.
     */
    void SetLightShadowHandles(const std::vector<LightShadowBinding> &handles);
    /**
     * @brief TODO: Describe GetCameraFrameData.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] const CameraFrameData &GetCameraFrameData() const;
    /**
     * @brief TODO: Describe UpdateCamera.
     *
     * @param camera TODO: Describe this parameter.
     */
    void UpdateCamera(const Camera &camera);
    /**
     * @brief TODO: Describe SetCameraAspectRatio.
     *
     * @param aspect_ratio TODO: Describe this parameter.
     */
    void SetCameraAspectRatio(float aspect_ratio);
    /**
     * @brief TODO: Describe ActiveCamera.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] const Camera &ActiveCamera() const;
    /**
     * @brief TODO: Describe SetAmbientLighting.
     *
     * @param ambient TODO: Describe this parameter.
     */
    void SetAmbientLighting(const AmbientLighting &ambient);
    /**
     * @brief TODO: Describe GetAmbientLighting.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] const AmbientLighting &GetAmbientLighting() const;
    /**
     * @brief TODO: Describe SetImageBasedLighting.
     *
     * @param lighting TODO: Describe this parameter.
     */
    void SetImageBasedLighting(const ImageBasedLighting &lighting);
    /**
     * @brief TODO: Describe GetImageBasedLighting.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] const ImageBasedLighting &GetImageBasedLighting() const;
    /**
     * @brief TODO: Describe SetExponentialHeightFog.
     *
     * @param fog TODO: Describe this parameter.
     */
    void SetExponentialHeightFog(const ExponentialHeightFog &fog);
    /**
     * @brief TODO: Describe GetExponentialHeightFog.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] const ExponentialHeightFog &GetExponentialHeightFog() const;
    /**
     * @brief TODO: Describe SetPostProcessSettings.
     *
     * @param settings TODO: Describe this parameter.
     */
    void SetPostProcessSettings(const PostProcessSettings &settings);
    /**
     * @brief TODO: Describe GetPostProcessSettings.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] const PostProcessSettings &GetPostProcessSettings() const;
    /**
     * @brief TODO: Describe SetSSAOSettings.
     *
     * @param settings TODO: Describe this parameter.
     */
    void SetSSAOSettings(const SSAOSettings &settings);
    /**
     * @brief TODO: Describe GetSSAOSettings.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] const SSAOSettings &GetSSAOSettings() const;
    /**
     * @brief TODO: Describe ConsumeImageBasedLightingResourcesDirty.
     *
     * @return TODO: Describe the return value.
     */
    bool ConsumeImageBasedLightingResourcesDirty();

  private:
    /**
     * @brief TODO: Describe RebuildGpuLights.
     */
    void RebuildGpuLights();

    std::unique_ptr<IRenderBackend> backend_;
    std::vector<MeshInstance> mesh_instances_;
    std::vector<Bounds> mesh_instance_local_bounds_;
    std::vector<Bounds> mesh_instance_world_bounds_;
    std::vector<std::uint32_t> mesh_instance_generations_;
    std::vector<std::uint32_t> free_mesh_instances_;
    std::uint64_t mesh_instance_revision_ = 1;
    std::vector<Light> direct_lights_;
    std::vector<std::uint32_t> light_generations_;
    std::vector<std::uint32_t> free_lights_;
    std::vector<GpuLight> gpu_lights_;
    std::vector<LightShadowBinding> light_shadow_handles_;
    CameraFrameData camera_frame_data_;
    Camera active_camera_;
    float camera_aspect_ratio_ = 4.0f / 3.0f;
    AmbientLighting ambient_lighting_;
    ImageBasedLighting image_based_lighting_;
    ExponentialHeightFog exponential_height_fog_;
    PostProcessSettings post_process_settings_;
    SSAOSettings ssao_settings_;
    bool image_based_lighting_resources_dirty_ = true;
    uint64_t light_revision_ = 1;
    std::uint64_t light_state_revision_ = 1;
    bool lights_dirty_ = true;
};

} // namespace CEngine::Renderer

#endif
