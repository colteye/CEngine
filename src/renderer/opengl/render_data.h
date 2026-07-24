//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/renderer/opengl/render_data.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#ifndef CENGINE_RENDERER_OPENGL_RENDER_DATA_H
#define CENGINE_RENDERER_OPENGL_RENDER_DATA_H

#include "renderer/material.h"
#include "renderer/mesh_instance.h"

#include <glad/glad.h>

#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

namespace CEngine::Renderer::OpenGL
{

/**
 * @brief TODO: Describe DrawItem.
 */
struct DrawItem
{
    const Mesh *mesh = nullptr;
    const Material *material = nullptr;
    const Texture *lightmap = nullptr;
    glm::mat4 transform = glm::mat4(1.0f);
    Bounds world_bounds;
    GLsizei count = 0;
    uint32_t flags = 0;
    GLuint vertex_array_obj = 0;
    GLuint albedo_tex = 0;
    GLuint normal_tex = 0;
    GLuint metallic_roughness_ao_tex = 0;
    GLuint lightmap_tex = 0;
    glm::vec2 lightmap_scale = glm::vec2(1.0f);
    glm::vec2 lightmap_offset = glm::vec2(0.0f);
    float lightmap_rgbm_range = 8.0f;
    GLuint joint_palette_buffer = 0;
    GLuint joint_palette_texture = 0;
    std::uint32_t joint_count = 0;
};

/**
 * @brief TODO: Describe RenderQueue.
 */
enum class RenderQueue
{
    DeferredOpaque,
    ForwardOpaque,
    Transparent,
    None
};

/**
 * @brief TODO: Describe RenderQueues.
 */
struct RenderQueues
{
    std::vector<uint32_t> opaque_deferred;
    std::vector<uint32_t> forward;
    std::vector<uint32_t> transparent;
    std::vector<uint32_t> shadow_casters;

    /**
     * @brief TODO: Describe ClearAndReserve.
     *
     * @param draw_item_count TODO: Describe this parameter.
     */
    void ClearAndReserve(size_t draw_item_count);
};

} // namespace CEngine::Renderer::OpenGL
#endif
