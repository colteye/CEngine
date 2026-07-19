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

class RenderSystem
{
public:
	static void Initialize(int window_width, int window_height);
	static bool Initialize(RenderBackendType backend_type, GLFWwindow* window, int window_width, int window_height);
	static void Shutdown();
	
	static void Render();

	static void RegisterMesh(const Mesh* mesh);
	static RenderableHandle RegisterRenderable(const Renderable& renderable);
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
	static void SetFrameConstants(const RenderFrameConstants& constants);
	static const RenderFrameConstants& GetFrameConstants();

private:
	static void RebuildGpuLights();

	static std::unique_ptr<IRenderBackend> backend;

	static std::vector<Renderable> renderables;
	static std::vector<LightRecord> direct_lights;
	static std::vector<GpuLight> gpu_lights;
	static RenderFrameConstants frame_constants;
	static uint64_t light_revision;
	static bool lights_dirty;
};
#endif
