#include "directional_shadow_cascade.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <glm/gtc/matrix_transform.hpp>

namespace CEngine::Renderer {

namespace {

glm::vec3 NormalizeOrDefault(const glm::vec3& value, const glm::vec3& fallback) {
    const float length = glm::length(value);
    if (length <= 0.00001f) {
        return fallback;
    }
    return value / length;
}

glm::vec3 StableUpForDirection(const glm::vec3& direction) {
    if (std::abs(glm::dot(direction, glm::vec3(0.0f, 0.0f, 1.0f))) > 0.95f) {
        return glm::vec3(0.0f, 1.0f, 0.0f);
    }
    return glm::vec3(0.0f, 0.0f, 1.0f);
}

std::array<glm::vec3, 8> BoundsCorners(const Bounds& bounds) {
    return {
        glm::vec3(bounds.min.x, bounds.min.y, bounds.min.z),
        glm::vec3(bounds.max.x, bounds.min.y, bounds.min.z),
        glm::vec3(bounds.min.x, bounds.max.y, bounds.min.z),
        glm::vec3(bounds.max.x, bounds.max.y, bounds.min.z),
        glm::vec3(bounds.min.x, bounds.min.y, bounds.max.z),
        glm::vec3(bounds.max.x, bounds.min.y, bounds.max.z),
        glm::vec3(bounds.min.x, bounds.max.y, bounds.max.z),
        glm::vec3(bounds.max.x, bounds.max.y, bounds.max.z),
    };
}

} // namespace

DirectionalShadowCascade BuildDirectionalShadowCascade(
    const std::array<glm::vec3, 8>& receiver_corners, const glm::vec3& requested_light_direction,
    int resolution, const std::vector<Bounds>& shadow_caster_bounds, float unbounded_caster_depth) {
    const glm::vec3 light_direction =
        NormalizeOrDefault(requested_light_direction, glm::vec3(-0.4f, -1.0f, -0.2f));
    const glm::vec3 light_right =
        NormalizeOrDefault(glm::cross(light_direction, StableUpForDirection(light_direction)),
                           glm::vec3(1.0f, 0.0f, 0.0f));
    const glm::vec3 light_up = glm::cross(light_right, light_direction);

    glm::vec3 center(0.0f);
    for (const glm::vec3& corner : receiver_corners) {
        center += corner;
    }
    center /= static_cast<float>(receiver_corners.size());

    float radius = 0.0f;
    for (const glm::vec3& corner : receiver_corners) {
        radius = std::max(radius, glm::length(corner - center));
    }
    radius = std::max(0.0625f, std::ceil(radius * 16.0f) / 16.0f);

    const float world_units_per_texel =
        (radius * 2.0f) / static_cast<float>(std::max(resolution, 1));
    if (world_units_per_texel > 0.0f) {
        const float right_coordinate = glm::dot(center, light_right);
        const float up_coordinate = glm::dot(center, light_up);
        center += light_right *
                  (std::round(right_coordinate / world_units_per_texel) * world_units_per_texel -
                   right_coordinate);
        center +=
            light_up * (std::round(up_coordinate / world_units_per_texel) * world_units_per_texel -
                        up_coordinate);
    }

    // This view establishes the light-space orientation and snapped XY origin.
    // Its position along the light direction is moved after the caster depth
    // range is known.
    const glm::mat4 centered_view = glm::lookAt(center, center + light_direction, light_up);
    glm::vec3 receiver_min(std::numeric_limits<float>::max());
    glm::vec3 receiver_max(std::numeric_limits<float>::lowest());
    for (const glm::vec3& corner : receiver_corners) {
        const glm::vec3 light_space = glm::vec3(centered_view * glm::vec4(corner, 1.0f));
        receiver_min = glm::min(receiver_min, light_space);
        receiver_max = glm::max(receiver_max, light_space);
    }

    const float xy_padding = world_units_per_texel * 3.0f;
    const float min_x = -radius - xy_padding;
    const float max_x = radius + xy_padding;
    const float min_y = -radius - xy_padding;
    const float max_y = radius + xy_padding;
    float caster_max_z = receiver_max.z;
    bool has_unbounded_caster = false;

    for (const Bounds& bounds : shadow_caster_bounds) {
        if (!bounds.valid) {
            has_unbounded_caster = true;
            continue;
        }

        glm::vec3 caster_min(std::numeric_limits<float>::max());
        glm::vec3 caster_max(std::numeric_limits<float>::lowest());
        for (const glm::vec3& corner : BoundsCorners(bounds)) {
            const glm::vec3 light_space = glm::vec3(centered_view * glm::vec4(corner, 1.0f));
            caster_min = glm::min(caster_min, light_space);
            caster_max = glm::max(caster_max, light_space);
        }

        const bool overlaps_xy = caster_max.x >= min_x && caster_min.x <= max_x &&
                                 caster_max.y >= min_y && caster_min.y <= max_y;
        if (overlaps_xy) {
            // A directional-light blocker can affect this receiver footprint
            // only from the light-facing side, which is +Z in this view.
            caster_max_z = std::max(caster_max_z, caster_max.z);
        }
    }

    if (has_unbounded_caster) {
        caster_max_z =
            std::max(caster_max_z, receiver_max.z + std::max(0.0f, unbounded_caster_depth));
    }

    const float depth_padding = std::max(1.0f, world_units_per_texel * 4.0f);
    const float light_offset = caster_max_z + depth_padding;
    DirectionalShadowCascade cascade;
    cascade.view = glm::lookAt(center - light_direction * light_offset, center, light_up);
    const float near_depth = depth_padding;
    const float far_depth =
        std::max(near_depth + 1.0f, light_offset - receiver_min.z + depth_padding);
    cascade.projection = glm::ortho(min_x, max_x, min_y, max_y, near_depth, far_depth);
    return cascade;
}

} // namespace CEngine::Renderer
