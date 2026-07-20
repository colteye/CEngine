#ifndef RENDER_BACKEND_H
#define RENDER_BACKEND_H

#include "renderer/renderable.h"

#include <cstdint>

struct GLFWwindow;

namespace CEngine::Renderer {

enum class RenderBackendType
{
	OpenGL,
	Vulkan
};

class IRenderBackend
{
public:
	virtual ~IRenderBackend() = default;

	virtual bool Initialize(GLFWwindow* window, int window_width, int window_height) = 0;
	virtual void Shutdown() = 0;
	virtual void Render() = 0;
	virtual void RenderDepthOnly(const glm::mat4& view, const glm::mat4& projection,
		uint32_t native_depth_texture, int texture_width, int texture_height) = 0;
	virtual void RegisterRenderable(const Renderable& renderable) = 0;
	virtual void UpdateRenderableTransform(RenderableHandle handle, const glm::mat4& transform,
		const Bounds& world_bounds) = 0;
	virtual void RegisterMaterial(Material* material) = 0;
};

} // namespace CEngine::Renderer

#endif
