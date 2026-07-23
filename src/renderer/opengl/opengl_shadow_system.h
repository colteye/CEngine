#ifndef OPENGL_SHADOW_SYSTEM_H
#define OPENGL_SHADOW_SYSTEM_H

#include "renderer/light.h"
#include "renderer/opengl/opengl_render_data.h"
#include "renderer/opengl/opengl_shadow_types.h"
#include "renderer/opengl/shaders/depth_only.h"
#include "renderer/opengl/shaders/point_shadow_depth.h"

#include <array>
#include <memory>
#include <vector>

#include <glad/glad.h>

namespace CEngine::Renderer {

class RenderSystem;

class OpenGLShadowSystem
{
public:
	struct AtlasTile {
		int x = 0;
		int y = 0;
		int size = 0;
	};

	OpenGLShadowSystem() = default;
	OpenGLShadowSystem(const OpenGLShadowSystem&) = delete;
	OpenGLShadowSystem& operator=(const OpenGLShadowSystem&) = delete;
	~OpenGLShadowSystem();

	bool Initialize(RenderSystem& rendering);
	void Destroy();
	void Render(const std::vector<OpenGLDrawItem>& draw_items, const OpenGLRenderQueues& queues);

	const OpenGLShadowGpuData& GetGpuData() const { return gpu_data; }
	GLuint GetAtlasTexture() const { return atlas_texture; }
	const std::array<GLuint, OpenGLShadows::kMaxPointShadows>& GetPointTextures() const { return point_textures; }

private:
	AtlasTile AllocateAtlasTile(int requested_size);
	int AllocatePointSlot(size_t light_index, int resolution);
	void RenderSpotShadow(size_t light_index, const Light& light,
		const std::vector<OpenGLDrawItem>& draw_items, const OpenGLRenderQueues& queues,
		std::vector<LightShadowGpuHandle>& handles);
	void RenderDirectionalShadow(size_t light_index, const Light& light,
		const std::vector<OpenGLDrawItem>& draw_items, const OpenGLRenderQueues& queues,
		std::vector<LightShadowGpuHandle>& handles);
	void RenderPointShadow(size_t light_index, const Light& light,
		const std::vector<OpenGLDrawItem>& draw_items, const OpenGLRenderQueues& queues,
		std::vector<LightShadowGpuHandle>& handles);
	void RenderDepthToAtlas(const AtlasTile& tile, const glm::mat4& view, const glm::mat4& projection,
		const std::vector<OpenGLDrawItem>& draw_items, const OpenGLRenderQueues& queues);
	void RenderPointFace(GLenum face, GLuint texture, int resolution, const glm::mat4& view,
		const glm::mat4& projection, const glm::vec3& position, float far_plane,
		const std::vector<OpenGLDrawItem>& draw_items, const OpenGLRenderQueues& queues);
	static bool SameMatrix(const glm::mat4& left, const glm::mat4& right);
	static bool SameRect(const glm::vec4& left, const glm::vec4& right);

	GLuint atlas_texture = 0;
	GLuint depth_fbo = 0;
	RenderSystem* rendering = nullptr;
	OpenGLShadowGpuData gpu_data;
	std::array<GLuint, OpenGLShadows::kMaxPointShadows> point_textures {};
	std::array<size_t, OpenGLShadows::kMaxPointShadows> point_light_indices {};
	std::array<int, OpenGLShadows::kMaxPointShadows> point_resolutions {};
	std::array<bool, OpenGLShadows::kMaxPointShadows> point_valid {};
	std::array<bool, OpenGLShadows::kMaxPointShadows> point_slot_used {};
	std::array<glm::mat4, OpenGLShadows::kMaxSpotShadows> cached_spot_matrices {};
	std::array<glm::vec4, OpenGLShadows::kMaxSpotShadows> cached_spot_rects {};
	std::array<bool, OpenGLShadows::kMaxSpotShadows> cached_spot_valid {};
	std::array<glm::mat4, OpenGLShadows::kMaxDirectionalCascades>
		cached_cascade_matrices {};
	std::array<glm::vec4, OpenGLShadows::kMaxDirectionalCascades>
		cached_cascade_rects {};
	std::array<bool, OpenGLShadows::kMaxDirectionalCascades>
		cached_cascade_valid {};
	std::unique_ptr<DepthOnly> depth_only;
	std::unique_ptr<PointShadowDepth> point_depth;
	int atlas_cursor_x = 0;
	int atlas_cursor_y = 0;
	int atlas_row_height = 0;
	std::uint64_t cached_renderable_revision = 0;
	std::uint64_t cached_light_state_revision = 0;
	bool shadow_content_changed = true;
};


} // namespace CEngine::Renderer
#endif
