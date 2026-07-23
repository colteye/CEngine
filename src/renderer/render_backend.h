//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/renderer/render_backend.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#ifndef RENDER_BACKEND_H
#define RENDER_BACKEND_H

#include "renderer/mesh_instance.h"

#include <cstdint>

struct GLFWwindow;

namespace CEngine::Renderer
{

class RenderSystem;

/**
 * @brief TODO: Describe IRenderBackend.
 */
class IRenderBackend
{
  public:
    /**
     * @brief TODO: Describe IRenderBackend.
     */
    IRenderBackend() = default;
    /**
     * @brief TODO: Describe ~IRenderBackend.
     */
    virtual ~IRenderBackend() = default;
    /**
     * @brief TODO: Describe IRenderBackend.
     */
    IRenderBackend(const IRenderBackend &) = delete;
    /**
     * @brief TODO: Describe operator=.
     *
     * @return TODO: Describe the return value.
     */
    IRenderBackend &operator=(const IRenderBackend &) = delete;
    /**
     * @brief TODO: Describe IRenderBackend.
     */
    IRenderBackend(IRenderBackend &&) = delete;
    /**
     * @brief TODO: Describe operator=.
     *
     * @return TODO: Describe the return value.
     */
    IRenderBackend &operator=(IRenderBackend &&) = delete;

    /**
     * @brief TODO: Describe Initialize.
     *
     * @param rendering TODO: Describe this parameter.
     * @param window TODO: Describe this parameter.
     * @param window_width TODO: Describe this parameter.
     * @param window_height TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    virtual bool Initialize(RenderSystem &rendering, GLFWwindow *window, int window_width, int window_height) = 0;
    /**
     * @brief TODO: Describe Shutdown.
     */
    virtual void Shutdown() = 0;
    /**
     * @brief TODO: Describe Resize.
     *
     * @param window_width TODO: Describe this parameter.
     * @param window_height TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    virtual bool Resize(int window_width, int window_height) = 0;
    /**
     * @brief TODO: Describe Render.
     */
    virtual void Render() = 0;
    /**
     * @brief TODO: Describe RegisterMeshInstance.
     *
     * @param slot TODO: Describe this parameter.
     * @param mesh_instance TODO: Describe this parameter.
     * @param world_bounds TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    virtual bool RegisterMeshInstance(std::uint32_t slot, const MeshInstance &mesh_instance,
                                      const Bounds &world_bounds) = 0;
    /**
     * @brief TODO: Describe RemoveMeshInstance.
     *
     * @param slot TODO: Describe this parameter.
     */
    virtual void RemoveMeshInstance(std::uint32_t slot) = 0;
    /**
     * @brief TODO: Describe UpdateMeshInstance.
     *
     * @param slot TODO: Describe this parameter.
     * @param transform TODO: Describe this parameter.
     * @param world_bounds TODO: Describe this parameter.
     * @param flags TODO: Describe this parameter.
     */
    virtual void UpdateMeshInstance(std::uint32_t slot, const glm::mat4 &transform, const Bounds &world_bounds,
                                    std::uint32_t flags) = 0;
};

} // namespace CEngine::Renderer

#endif
