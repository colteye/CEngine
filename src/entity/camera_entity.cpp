//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/entity/camera_entity.cpp
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#include "entity/camera_entity.h"

#include "context.h"
#include "renderer/render_system.h"
#include "scene/scene.h"

#include <glm/gtx/quaternion.hpp>

namespace CEngine::Entities
{

std::string_view CameraEntity::Classname() const
{
    return "camera";
}

bool CameraEntity::AcceptsInput(std::string_view input) const
{
    return input == "Activate" || Entity::AcceptsInput(input);
}

bool CameraEntity::HasOutput(std::string_view output) const
{
    return output == "OnActivated" || Entity::HasOutput(output);
}

bool CameraEntity::HandleInput(Context &context, const Scene::EntityInput &input)
{
    if (input.name != "Activate")
    {
        return Entity::HandleInput(context, input);
    }
    if (context.scene == nullptr || !context.scene->SetActiveEntity(GetHandle()))
    {
        return false;
    }
    context.scene->Emit(GetHandle(), "OnActivated", input.activator);
    Update(context, 0.0f);
    return true;
}

void CameraEntity::Update(Context &context, float /*delta_seconds*/)
{
    if (context.scene == nullptr || context.rendering == nullptr ||
        context.scene->Settings().active_entity != GetHandle())
    {
        return;
    }
    Renderer::Camera camera;
    camera.position = GetTransform().position;
    camera.direction = glm::normalize(GetTransform().rotation * glm::vec3(0.0f, 0.0f, -1.0f));
    camera.vertical_fov_radians = vertical_fov_radians;
    camera.near_clip = near_clip;
    camera.far_clip = far_clip;
    context.rendering->UpdateCamera(camera);
}

void CameraEntity::OnEnabledChanged(Context &context, bool enabled)
{
    if (!enabled && context.scene != nullptr &&
        context.scene->Settings().active_entity == GetHandle())
    {
        context.scene->SetActiveEntity({});
    }
}

} // namespace CEngine::Entities
