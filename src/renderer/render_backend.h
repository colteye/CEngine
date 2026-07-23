#ifndef RENDER_BACKEND_H
#define RENDER_BACKEND_H

#include "renderer/mesh_instance.h"

#include <cstdint>

struct GLFWwindow;

namespace CEngine::Renderer
{

class RenderSystem;

class IRenderBackend
{
  public:
    IRenderBackend() = default;
    virtual ~IRenderBackend() = default;
    IRenderBackend(const IRenderBackend &) = delete;
    IRenderBackend &operator=(const IRenderBackend &) = delete;
    IRenderBackend(IRenderBackend &&) = delete;
    IRenderBackend &operator=(IRenderBackend &&) = delete;

    virtual bool Initialize(RenderSystem &rendering, GLFWwindow *window, int window_width, int window_height) = 0;
    virtual void Shutdown() = 0;
    virtual bool Resize(int window_width, int window_height) = 0;
    virtual void Render() = 0;
    virtual bool RegisterMeshInstance(std::uint32_t slot, const MeshInstance &mesh_instance,
                                      const Bounds &world_bounds) = 0;
    virtual void RemoveMeshInstance(std::uint32_t slot) = 0;
    virtual void UpdateMeshInstance(std::uint32_t slot, const glm::mat4 &transform, const Bounds &world_bounds,
                                    std::uint32_t flags) = 0;
};

} // namespace CEngine::Renderer

#endif
