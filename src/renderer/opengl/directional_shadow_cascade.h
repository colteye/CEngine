#ifndef DIRECTIONAL_SHADOW_CASCADE_H
#define DIRECTIONAL_SHADOW_CASCADE_H

#include "renderer/mesh_instance.h"

#include <array>
#include <vector>

#include <glm/glm.hpp>

namespace CEngine::Renderer
{

struct DirectionalShadowCascade
{
    glm::mat4 view = glm::mat4(1.0f);
    glm::mat4 projection = glm::mat4(1.0f);
};

DirectionalShadowCascade BuildDirectionalShadowCascade(const std::array<glm::vec3, 8> &receiver_corners,
                                                       const glm::vec3 &light_direction, int resolution,
                                                       const std::vector<Bounds> &shadow_caster_bounds,
                                                       float unbounded_caster_depth);

} // namespace CEngine::Renderer

#endif
