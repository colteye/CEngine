#include "entity/entity.h"

#include <glm/gtc/matrix_transform.hpp>

namespace CEngine::Scene
{

void Transform::UpdateWorldMatrix()
{
    if (!dirty)
    {
        return;
    }
    world_matrix = glm::translate(glm::mat4(1.0f), position) * glm::mat4_cast(glm::normalize(rotation)) *
                   glm::scale(glm::mat4(1.0f), scale);
    dirty = false;
}

} // namespace CEngine::Scene
