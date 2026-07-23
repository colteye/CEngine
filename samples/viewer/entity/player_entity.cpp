//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file samples/viewer/entity/player_entity.cpp
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#include "entity/player_entity.h"

#include "context.h"
#include "input/input_system.h"
#include "physics/physics_system.h"
#include "renderer/camera.h"
#include "renderer/render_system.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/quaternion.hpp>

namespace Viewer
{
/**
 * @brief TODO: Describe PlayerEntity::Classname.
 *
 * @return TODO: Describe the return value.
 */
std::string_view PlayerEntity::Classname() const
{
    return "player";
}

/**
 * @brief TODO: Describe PlayerEntity::Initialize.
 *
 * @param context TODO: Describe this parameter.
 */
void PlayerEntity::Initialize(CEngine::Context &context)
{
    const glm::vec3 direction =
        glm::normalize(glm::vec3(GetTransform().world_matrix * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f)));
    look_angles_ = {std::atan2(direction.y, direction.x), std::asin(std::clamp(direction.z, -1.0f, 1.0f))};
    if (context.physics != nullptr)
    {
        PhysicsCharacterDesc desc;
        desc.position = GetTransform().position - glm::vec3(0.0f, 0.0f, eye_height);
        desc.radius = character_radius;
        desc.height = character_height;
        desc.mass = character_mass;
        desc.max_strength = max_strength;
        desc.max_slope_radians = max_slope_radians;
        desc.step_height = step_height;
        desc.stick_to_floor_distance = stick_to_floor_distance;
        character_ = context.physics->CreateCharacter(desc);
    }
}

/**
 * @brief TODO: Describe PlayerEntity::Update.
 *
 * @param context TODO: Describe this parameter.
 * @param delta_seconds TODO: Describe this parameter.
 */
void PlayerEntity::Update(CEngine::Context &context, float delta_seconds)
{
    if (!enabled)
    {
        return;
    }
    if (context.input != nullptr)
    {
        look_angles_ += glm::vec2(context.input->Value(actions_.look_yaw), context.input->Value(actions_.look_pitch));
    }
    look_angles_.y = std::clamp(look_angles_.y, -1.55f, 1.55f);

    const glm::vec3 forward(std::cos(look_angles_.y) * std::cos(look_angles_.x),
                            std::cos(look_angles_.y) * std::sin(look_angles_.x), std::sin(look_angles_.y));
    const glm::vec3 up(0.0f, 0.0f, 1.0f);
    const glm::vec3 planar_forward =
        glm::normalize(glm::vec3(std::cos(look_angles_.x), std::sin(look_angles_.x), 0.0f));
    const glm::vec3 right = glm::normalize(glm::cross(planar_forward, up));
    auto &transform = GetTransform();
    const float speed =
        move_speed * (context.input != nullptr && context.input->Value(actions_.sprint) > 0.5f ? 3.0f : 1.0f);
    const glm::vec3 movement = context.input != nullptr ? planar_forward * context.input->Value(actions_.move_forward) +
                                                              right * context.input->Value(actions_.move_right)
                                                        : glm::vec3(0.0f);
    if (context.physics != nullptr && character_)
    {
        PhysicsCharacterState state;
        if (context.physics->GetCharacterState(character_, state))
        {
            glm::vec3 velocity = movement * speed;
            velocity.z = state.velocity.z;
            if (state.grounded && context.input != nullptr && context.input->Value(actions_.jump) > 0.5f)
            {
                velocity.z = jump_speed;
            }
            context.physics->SetCharacterVelocity(character_, velocity);
            context.physics->UpdateCharacter(character_, delta_seconds);
            if (context.physics->GetCharacterState(character_, state))
            {
                transform.position = state.position + glm::vec3(0.0f, 0.0f, eye_height);
            }
        }
    }
    else
    {
        transform.position += movement * speed * delta_seconds;
    }
    transform.rotation = glm::quatLookAt(glm::normalize(forward), glm::vec3(0.0f, 0.0f, 1.0f));
    transform.dirty = true;
    transform.UpdateWorldMatrix();

    if (context.rendering != nullptr)
    {
        CEngine::Renderer::Camera camera;
        camera.position = transform.position;
        if (view_mode == Generated::PlayerViewMode::ThirdPerson)
        {
            camera.position -= forward * std::max(third_person_distance, 0.0f);
        }
        camera.direction = forward;
        camera.vertical_fov_radians = vertical_fov_radians;
        camera.near_clip = near_clip;
        camera.far_clip = far_clip;
        context.rendering->UpdateCamera(camera);
    }
}

/**
 * @brief TODO: Describe PlayerEntity::Shutdown.
 *
 * @param context TODO: Describe this parameter.
 */
void PlayerEntity::Shutdown(CEngine::Context &context)
{
    if (context.physics != nullptr && character_)
    {
        context.physics->DestroyCharacter(character_);
    }
    character_ = {};
}
} // namespace Viewer
