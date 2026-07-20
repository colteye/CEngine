#ifndef RENDER_SYSTEM_H
#define RENDER_SYSTEM_H

#include "light.h"
#include "render_backend.h"
#include "renderable.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

struct GLFWwindow;

struct RenderFrameConstants
{
	glm::vec3 camera_position = glm::vec3(0.0f);
	glm::mat4 model = glm::mat4(1.0f);
	glm::mat4 view = glm::mat4(1.0f);
	glm::mat4 proj = glm::mat4(1.0f);
};

struct AmbientLighting
{
	glm::vec3 sky_color = glm::vec3(0.36f, 0.42f, 0.52f);
	glm::vec3 ground_color = glm::vec3(0.12f, 0.10f, 0.08f);
	float intensity = 0.18f;
	bool enabled = true;
};

class RenderSystem
{
public:
	static void Initialize(int window_width, int window_height);
	static bool Initialize(RenderBackendType backend_type, GLFWwindow* window, int window_width, int window_height);
	static void Shutdown();
	
	static void Render();
	static void RenderDepthOnly(const glm::mat4& view, const glm::mat4& projection,
		uint32_t native_depth_texture, int texture_width, int texture_height);

	static void RegisterMesh(const Mesh* mesh);
	static RenderableHandle RegisterRenderable(const Renderable& renderable);
	static void UpdateRenderableTransform(RenderableHandle handle, const glm::mat4& transform);
	static void RegisterMaterial(Material* material);

	static LightHandle RegisterLight(const glm::vec3& light_pos, const glm::vec3& light_col, float light_pow);
	static void UpdateLight(LightHandle id, const glm::vec3& light_pos, const glm::vec3& light_col, float light_pow);
	static LightHandle RegisterLight(const LightRecord& light);
	static void UpdateLight(LightHandle id, const LightRecord& light);

	static const std::vector<Renderable>& GetRenderables();
	static const std::vector<LightRecord>& GetDirectLights();
	static const std::vector<GpuLight>& GetGpuLights();
	static uint64_t GetLightRevision();
	static size_t GetMaxGpuLights();
	static void SetLightShadowHandles(const std::vector<LightShadowGpuHandle>& handles);
	static void SetFrameConstants(const RenderFrameConstants& constants);
	static const RenderFrameConstants& GetFrameConstants();
	static void SetAmbientLighting(const AmbientLighting& ambient);
	static const AmbientLighting& GetAmbientLighting();

private:
	static void RebuildGpuLights();

	static std::unique_ptr<IRenderBackend> backend;

	static std::vector<Renderable> renderables;
	static std::vector<LightRecord> direct_lights;
	static std::vector<GpuLight> gpu_lights;
	static std::vector<LightShadowGpuHandle> light_shadow_handles;
	static RenderFrameConstants frame_constants;
	static AmbientLighting ambient_lighting;
	static uint64_t light_revision;
	static bool lights_dirty;
};
#endif
