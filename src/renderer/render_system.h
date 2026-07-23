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

struct AmbientLighting
{
    glm::vec3 sky_color = glm::vec3(0.36f, 0.42f, 0.52f);
    glm::vec3 ground_color = glm::vec3(0.12f, 0.10f, 0.08f);
    float intensity = 0.18f;
    bool enabled = true;
};

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

struct SSAOSettings
{
    bool enabled = true;
    float radius = 0.65f;
    float bias = 0.025f;
    float intensity = 0.65f;
    float contrast = 1.0f;
};

class RenderSystem
{
  public:
    static constexpr std::size_t MaxGpuLights = 64;

    RenderSystem() = default;
    ~RenderSystem();
    RenderSystem(const RenderSystem &) = delete;
    RenderSystem &operator=(const RenderSystem &) = delete;
    RenderSystem(RenderSystem &&) = delete;
    RenderSystem &operator=(RenderSystem &&) = delete;

    bool Initialize(GLFWwindow *window, int window_width, int window_height);
    void Shutdown();
    bool Resize(int window_width, int window_height);

    void Render();

    MeshInstanceHandle RegisterMeshInstance(const MeshInstance &mesh_instance);
    void RemoveMeshInstance(MeshInstanceHandle handle);
    void UpdateMeshInstance(MeshInstanceHandle handle, const glm::mat4 &transform, std::uint32_t flags);

    LightHandle RegisterLight(const Light &light);
    void RemoveLight(LightHandle id);
    void UpdateLight(LightHandle id, const Light &light);

    [[nodiscard]] const std::vector<MeshInstance> &GetMeshInstances() const;
    [[nodiscard]] std::uint64_t GetMeshInstanceRevision() const;
    [[nodiscard]] const std::vector<Light> &GetDirectLights() const;
    [[nodiscard]] const MeshInstance *ResolveMeshInstance(MeshInstanceHandle handle) const;
    [[nodiscard]] const Light *ResolveLight(LightHandle handle) const;
    const std::vector<GpuLight> &GetGpuLights();
    [[nodiscard]] std::uint64_t GetLightRevision() const;
    [[nodiscard]] std::uint64_t GetLightStateRevision() const;
    void SetLightShadowHandles(const std::vector<LightShadowBinding> &handles);
    [[nodiscard]] const CameraFrameData &GetCameraFrameData() const;
    void UpdateCamera(const Camera &camera);
    void SetCameraAspectRatio(float aspect_ratio);
    [[nodiscard]] const Camera &ActiveCamera() const;
    void SetAmbientLighting(const AmbientLighting &ambient);
    [[nodiscard]] const AmbientLighting &GetAmbientLighting() const;
    void SetImageBasedLighting(const ImageBasedLighting &lighting);
    [[nodiscard]] const ImageBasedLighting &GetImageBasedLighting() const;
    void SetExponentialHeightFog(const ExponentialHeightFog &fog);
    [[nodiscard]] const ExponentialHeightFog &GetExponentialHeightFog() const;
    void SetPostProcessSettings(const PostProcessSettings &settings);
    [[nodiscard]] const PostProcessSettings &GetPostProcessSettings() const;
    void SetSSAOSettings(const SSAOSettings &settings);
    [[nodiscard]] const SSAOSettings &GetSSAOSettings() const;
    bool ConsumeImageBasedLightingResourcesDirty();

  private:
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
