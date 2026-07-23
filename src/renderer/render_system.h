#ifndef RENDER_SYSTEM_H
#define RENDER_SYSTEM_H

#include "renderer/light.h"
#include "renderer/render_backend.h"
#include "renderer/renderable.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <vector>

struct GLFWwindow;

namespace CEngine::Renderer {

struct RenderFrameConstants
{
	glm::vec3 camera_position = glm::vec3(0.0f);
	glm::mat4 model = glm::mat4(1.0f);
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
    std::filesystem::path panorama_path;
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

// Presentation-only image treatment. This belongs to the game/view setup, not
// to scene entities, so one game can choose its look without authored data.
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
	static bool Initialize(GLFWwindow* window, int window_width, int window_height);
	static void Shutdown();
	static bool Resize(int window_width, int window_height);
	
	static void Render();
	static void RenderDepthOnly(const glm::mat4& view, const glm::mat4& projection,
		uint32_t native_depth_texture, int texture_width, int texture_height);

	static void RegisterMesh(const Mesh* mesh);
	static RenderableHandle RegisterRenderable(const Renderable& renderable);
	static void RemoveRenderable(RenderableHandle handle);
	static void UpdateRenderableTransform(RenderableHandle handle, const glm::mat4& transform);
	static bool RegisterMaterial(Material* material);
	static void RemoveMaterial(Material* material);
	static bool RegisterLightmap(const Lightmap* lightmap);
	static void RemoveLightmap(const Lightmap* lightmap);

	static LightHandle RegisterLight(const glm::vec3& light_pos, const glm::vec3& light_col, float light_pow);
	static void UpdateLight(LightHandle id, const glm::vec3& light_pos, const glm::vec3& light_col, float light_pow);
	static LightHandle RegisterLight(const LightRecord& light);
	static void RemoveLight(LightHandle id);
	static void UpdateLight(LightHandle id, const LightRecord& light);

	static const std::vector<Renderable>& GetRenderables();
	static const std::vector<LightRecord>& GetDirectLights();
	static const Renderable* ResolveRenderable(RenderableHandle handle);
	static const LightRecord* ResolveLight(LightHandle handle);
	static const std::vector<GpuLight>& GetGpuLights();
	static uint64_t GetLightRevision();
	static size_t GetMaxGpuLights();
	static void SetLightShadowHandles(const std::vector<LightShadowGpuHandle>& handles);
	static void SetFrameConstants(const RenderFrameConstants& constants);
	static const RenderFrameConstants& GetFrameConstants();
	static void SetAmbientLighting(const AmbientLighting& ambient);
	static const AmbientLighting& GetAmbientLighting();
	static void SetImageBasedLighting(const ImageBasedLighting& lighting);
	static const ImageBasedLighting& GetImageBasedLighting();
	static void SetExponentialHeightFog(const ExponentialHeightFog& fog);
	static const ExponentialHeightFog& GetExponentialHeightFog();
	static void SetPostProcessSettings(const PostProcessSettings& settings);
	static const PostProcessSettings& GetPostProcessSettings();
	static void SetSSAOSettings(const SSAOSettings& settings);
	static const SSAOSettings& GetSSAOSettings();
	static bool ConsumeImageBasedLightingResourcesDirty();

private:
	static void RebuildGpuLights();

	static std::unique_ptr<IRenderBackend> backend;

	static std::vector<Renderable> renderables;
	static std::vector<std::uint32_t> renderable_generations;
	static std::vector<std::uint32_t> free_renderables;
	static std::vector<LightRecord> direct_lights;
	static std::vector<std::uint32_t> light_generations;
	static std::vector<std::uint32_t> free_lights;
	static std::vector<GpuLight> gpu_lights;
	static std::vector<LightShadowGpuHandle> light_shadow_handles;
	static RenderFrameConstants frame_constants;
    static AmbientLighting ambient_lighting;
    static ImageBasedLighting image_based_lighting;
    static ExponentialHeightFog exponential_height_fog;
	static PostProcessSettings post_process_settings;
	static SSAOSettings ssao_settings;
    static bool image_based_lighting_resources_dirty;
	static uint64_t light_revision;
	static bool lights_dirty;
};

} // namespace CEngine::Renderer

#endif
