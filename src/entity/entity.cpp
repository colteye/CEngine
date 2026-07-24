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

#include "context.h"
#include "scene/scene.h"

#include <glm/gtc/matrix_transform.hpp>

namespace CEngine::Scene
{

bool Entity::AcceptsInput(std::string_view input) const
{
    return input == "Enable" || input == "Disable" || input == "Toggle";
}

bool Entity::HasOutput(std::string_view output) const
{
    return output == "OnEnabled" || output == "OnDisabled";
}

bool Entity::HandleInput(Context &context, const EntityInput &input)
{
    if (input.name == "Enable")
    {
        return SetEnabled(context, true);
    }
    if (input.name == "Disable")
    {
        return SetEnabled(context, false);
    }
    if (input.name == "Toggle")
    {
        return SetEnabled(context, !Enabled());
    }
    return false;
}

bool Entity::SetEnabled(Context &context, bool enabled)
{
    if (Enabled() == enabled)
    {
        return true;
    }
    if (enabled)
    {
        flags_ |= EntityEnabled;
    }
    else
    {
        flags_ &= ~EntityEnabled;
    }
    OnEnabledChanged(context, enabled);
    if (context.scene != nullptr)
    {
        context.scene->Emit(id_, enabled ? "OnEnabled" : "OnDisabled", id_);
    }
    return true;
}

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
