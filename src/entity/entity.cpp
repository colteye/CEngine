//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/entity/entity.cpp
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#include "entity/entity.h"

#include <glm/gtc/matrix_transform.hpp>

namespace CEngine::Scene
{

/**
 * @brief TODO: Describe Transform::UpdateWorldMatrix.
 */
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
