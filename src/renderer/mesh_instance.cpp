//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/renderer/mesh_instance.cpp
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#include "renderer/mesh_instance.h"

#include <array>

namespace CEngine::Renderer
{

/**
 * @brief TODO: Describe TransformBounds.
 *
 * @param bounds TODO: Describe this parameter.
 * @param transform TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
Bounds TransformBounds(const Bounds &bounds, const glm::mat4 &transform)
{
    if (!bounds.valid)
    {
        return bounds;
    }

    const std::array<glm::vec3, 8> corners = {
        glm::vec3(bounds.min.x, bounds.min.y, bounds.min.z), glm::vec3(bounds.max.x, bounds.min.y, bounds.min.z),
        glm::vec3(bounds.min.x, bounds.max.y, bounds.min.z), glm::vec3(bounds.max.x, bounds.max.y, bounds.min.z),
        glm::vec3(bounds.min.x, bounds.min.y, bounds.max.z), glm::vec3(bounds.max.x, bounds.min.y, bounds.max.z),
        glm::vec3(bounds.min.x, bounds.max.y, bounds.max.z), glm::vec3(bounds.max.x, bounds.max.y, bounds.max.z),
    };

    Bounds transformed;
    for (const glm::vec3 &corner : corners)
    {
        const glm::vec3 world_corner = glm::vec3(transform * glm::vec4(corner, 1.0f));
        if (!transformed.valid)
        {
            transformed.min = world_corner;
            transformed.max = world_corner;
            transformed.valid = true;
        }
        else
        {
            transformed.min = glm::min(transformed.min, world_corner);
            transformed.max = glm::max(transformed.max, world_corner);
        }
    }

    return transformed;
}

/**
 * @brief TODO: Describe ExtractFrustum.
 *
 * @param view_projection TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
Frustum ExtractFrustum(const glm::mat4 &view_projection)
{
    const glm::mat4 rows = glm::transpose(view_projection);
    Frustum frustum;
    frustum.planes = {rows[3] + rows[0], rows[3] - rows[0], rows[3] + rows[1],
                      rows[3] - rows[1], rows[3] + rows[2], rows[3] - rows[2]};

    for (glm::vec4 &plane : frustum.planes)
    {
        const float normal_length = glm::length(glm::vec3(plane));
        if (normal_length > 0.0f)
        {
            plane /= normal_length;
        }
    }
    return frustum;
}

/**
 * @brief TODO: Describe IntersectsFrustum.
 *
 * @param bounds TODO: Describe this parameter.
 * @param frustum TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool IntersectsFrustum(const Bounds &bounds, const Frustum &frustum)
{
    // Missing bounds must remain visible; culling is only safe with valid spatial data.
    if (!bounds.valid)
    {
        return true;
    }

    for (const glm::vec4 &plane : frustum.planes)
    {
        const glm::vec3 positive(plane.x >= 0.0f ? bounds.max.x : bounds.min.x,
                                 plane.y >= 0.0f ? bounds.max.y : bounds.min.y,
                                 plane.z >= 0.0f ? bounds.max.z : bounds.min.z);
        if (glm::dot(glm::vec3(plane), positive) + plane.w < 0.0f)
        {
            return false;
        }
    }
    return true;
}

} // namespace CEngine::Renderer
