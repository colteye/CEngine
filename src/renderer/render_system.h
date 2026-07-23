#ifndef RENDER_SYSTEM_H
#define RENDER_SYSTEM_H

#include "renderer/camera.h"
#include "renderer/light.h"
#include "renderer/render_backend.h"
#include "renderer/renderable.h"
#include "renderer/texture.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

struct GLFWwindow;

namespace CEngine::Renderer {

struct RenderFrameConstants
{
	glm::vec3 camera_position = glm::vec3(0.0f);
	glm::mat4 view = glm::mat4(1.0f);
	glm::mat4 proj = glm::mat4(1.0f);
	float near_clip = 0.1f;
	float far_clip = 100.0f;
};

struct AmbientLighting
{
	glm::vec3 sky_color = glm::vec3(0.36f, 0.42f, 0.52f);
	glm::vec3 ground_color = glm::vec3(0.12f, 0.10f, 0.08f);
	float intensity = 0.18f;
	bool enabled = true;
};

struct ImageBasedLighting
{
    const Texture* panorama = nullptr;
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

	~RenderSystem();
	bool Initialize(GLFWwindow* window, int window_width, int window_height);
	void Shutdown();
	bool Resize(int window_width, int window_height);
	
	void Render();

	RenderableHandle RegisterRenderable(const Renderable& renderable);
	void RemoveRenderable(RenderableHandle handle);
	void UpdateRenderable(RenderableHandle handle, const glm::mat4& transform,
		std::uint32_t flags);
	bool RegisterMaterial(Material* material);
	void RemoveMaterial(Material* material);
	bool RegisterLightmap(const Texture* lightmap);
	void RemoveLightmap(const Texture* lightmap);

	LightHandle RegisterLight(const Light& light);
	void RemoveLight(LightHandle id);
	void UpdateLight(LightHandle id, const Light& light);

	const std::vector<Renderable>& GetRenderables() const;
	std::uint64_t GetRenderableRevision() const;
	const std::vector<Light>& GetDirectLights() const;
	const Renderable* ResolveRenderable(RenderableHandle handle) const;
	const Light* ResolveLight(LightHandle handle) const;
	const std::vector<GpuLight>& GetGpuLights();
	std::uint64_t GetLightRevision() const;
	std::uint64_t GetLightStateRevision() const;
	void SetLightShadowHandles(const std::vector<LightShadowGpuHandle>& handles);
	const RenderFrameConstants& GetFrameConstants() const;
	void UpdateCamera(const Camera& camera);
	void SetCameraAspectRatio(float aspect_ratio);
	const Camera& ActiveCamera() const;
	void SetAmbientLighting(const AmbientLighting& ambient);
	const AmbientLighting& GetAmbientLighting() const;
	void SetImageBasedLighting(const ImageBasedLighting& lighting);
	const ImageBasedLighting& GetImageBasedLighting() const;
	void SetExponentialHeightFog(const ExponentialHeightFog& fog);
	const ExponentialHeightFog& GetExponentialHeightFog() const;
	void SetPostProcessSettings(const PostProcessSettings& settings);
	const PostProcessSettings& GetPostProcessSettings() const;
	void SetSSAOSettings(const SSAOSettings& settings);
	const SSAOSettings& GetSSAOSettings() const;
	bool ConsumeImageBasedLightingResourcesDirty();

private:
	void RebuildGpuLights();

	std::unique_ptr<IRenderBackend> backend;
	std::vector<Renderable> renderables;
	std::vector<std::uint32_t> renderable_generations;
	std::vector<std::uint32_t> free_renderables;
	std::uint64_t renderable_revision = 1;
	std::vector<Light> direct_lights;
	std::vector<std::uint32_t> light_generations;
	std::vector<std::uint32_t> free_lights;
	std::vector<GpuLight> gpu_lights;
	std::vector<LightShadowGpuHandle> light_shadow_handles;
	RenderFrameConstants frame_constants;
	Camera active_camera;
	float camera_aspect_ratio = 4.0f / 3.0f;
    AmbientLighting ambient_lighting;
    ImageBasedLighting image_based_lighting;
    ExponentialHeightFog exponential_height_fog;
	PostProcessSettings post_process_settings;
	SSAOSettings ssao_settings;
	bool image_based_lighting_resources_dirty = true;
	uint64_t light_revision = 1;
	std::uint64_t light_state_revision = 1;
	bool lights_dirty = true;
};

} // namespace CEngine::Renderer

#endif
