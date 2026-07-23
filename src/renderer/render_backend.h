#ifndef RENDER_BACKEND_H
#define RENDER_BACKEND_H

#include "renderer/renderable.h"

#include <cstdint>

struct GLFWwindow;

namespace CEngine::Renderer {

class RenderSystem;

class IRenderBackend
{
public:
	virtual ~IRenderBackend() = default;

	virtual bool Initialize(RenderSystem& rendering,
		GLFWwindow* window, int window_width, int window_height) = 0;
	virtual void Shutdown() = 0;
	virtual bool Resize(int window_width, int window_height) = 0;
	virtual void Render() = 0;
	virtual bool RegisterRenderable(std::uint32_t slot, const Renderable& renderable) = 0;
	virtual void RemoveRenderable(std::uint32_t slot) = 0;
	virtual void UpdateRenderable(std::uint32_t slot, const glm::mat4& transform,
		const Bounds& world_bounds, std::uint32_t flags) = 0;
	virtual bool RegisterMaterial(Material* material) = 0;
	virtual void RemoveMaterial(Material* material) = 0;
	virtual bool RegisterLightmap(const Texture* lightmap) = 0;
	virtual void RemoveLightmap(const Texture* lightmap) = 0;
};

} // namespace CEngine::Renderer

#endif
