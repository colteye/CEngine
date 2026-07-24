//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/renderer/ui_frame.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#ifndef CENGINE_RENDERER_UI_FRAME_H
#define CENGINE_RENDERER_UI_FRAME_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>

namespace CEngine::Renderer
{

/**
 * Immutable premultiplied sRGBA8 pixels retained by a UI frame. RGB channels
 * use the sRGB transfer function and alpha is linear. UI runtimes may generate
 * these dynamically for font atlases without publishing backend texture IDs.
 */
struct UiTexture
{
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba;

    [[nodiscard]] bool Valid() const
    {
        return width > 0 && height > 0 &&
               rgba.size() == static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u;
    }
};

/**
 * Packed two-dimensional vertex in top-left-origin drawable pixel space.
 * Colors use premultiplied sRGBA8, matching RmlUi. The renderer converts them
 * to linear premultiplied values before interpolation and blending.
 */
struct UiVertex
{
    float position_x = 0.0f;
    float position_y = 0.0f;
    std::uint8_t red = 0;
    std::uint8_t green = 0;
    std::uint8_t blue = 0;
    std::uint8_t alpha = 0;
    float texture_u = 0.0f;
    float texture_v = 0.0f;
};

struct UiScissor
{
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

/**
 * One ordered UI draw. Indices address the frame-wide vertex array. A null
 * texture means vertex color only.
 */
struct UiBatch
{
    std::uint32_t first_index = 0;
    std::uint32_t index_count = 0;
    std::shared_ptr<const UiTexture> texture;
    UiScissor scissor;
    glm::vec2 translation = glm::vec2(0.0f);
    glm::mat4 transform = glm::mat4(1.0f);
    bool scissor_enabled = false;
};

/**
 * Complete immutable UI composition for one display frame.
 */
struct UiFrame
{
    int width = 0;
    int height = 0;
    std::vector<UiVertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<UiBatch> batches;

    [[nodiscard]] bool Empty() const
    {
        return batches.empty();
    }

    void Clear()
    {
        width = 0;
        height = 0;
        vertices.clear();
        indices.clear();
        batches.clear();
    }
};

} // namespace CEngine::Renderer

#endif
