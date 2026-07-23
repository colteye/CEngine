#include "entity/player_entity.h"

#include "engine_context.h"
#include "input/input_system.h"
#include "renderer/camera.h"
#include "renderer/render_system.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/quaternion.hpp>

namespace Viewer {
std::string_view PlayerEntity::Classname() const { return "player"; }

void PlayerEntity::Initialize(CEngine::EngineContext&)
{
    const glm::vec3 direction = glm::normalize(
        glm::vec3(GetTransform().world_matrix * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f)));
    look_angles_ = {
        std::atan2(direction.y, direction.x),
        std::asin(std::clamp(direction.z, -1.0f, 1.0f))
    };
}

void PlayerEntity::Update(CEngine::EngineContext& context, float delta_seconds)
{
    if (!enabled) return;
    if (context.input != nullptr)
        look_angles_ += glm::vec2(
            context.input->Value(actions_.look_yaw),
            context.input->Value(actions_.look_pitch));
    look_angles_.y = std::clamp(look_angles_.y, -1.55f, 1.55f);

    const glm::vec3 forward(
        std::cos(look_angles_.y) * std::cos(look_angles_.x),
        std::cos(look_angles_.y) * std::sin(look_angles_.x),
        std::sin(look_angles_.y));
    const glm::vec3 right = glm::normalize(
        glm::cross(forward, glm::vec3(0.0f, 0.0f, 1.0f)));
    auto& transform = GetTransform();
    if (context.input != nullptr)
    {
        const float speed =
            move_speed *
            (context.input->Value(actions_.sprint) > 0.5f ? 3.0f : 1.0f);
        transform.position +=
            (forward * context.input->Value(actions_.move_forward) +
                right * context.input->Value(actions_.move_right)) *
            speed * delta_seconds;
    }
    transform.rotation =
        glm::quatLookAt(glm::normalize(forward), glm::vec3(0.0f, 0.0f, 1.0f));
    transform.dirty = true;
    transform.UpdateWorldMatrix();

    if (context.rendering != nullptr)
    {
        CEngine::Renderer::Camera camera;
        camera.position = transform.position;
        if (view_mode == PlayerViewMode::ThirdPerson)
            camera.position -= forward * std::max(third_person_distance, 0.0f);
        camera.direction = forward;
        camera.vertical_fov_radians = vertical_fov_radians;
        camera.near_clip = near_clip;
        camera.far_clip = far_clip;
        context.rendering->UpdateCamera(camera);
    }
}
} // namespace Viewer
