//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
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
#include "scene/scene.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/quaternion.hpp>

namespace Viewer
{

PlayerEntity::PlayerEntity(Actions actions, PlayerRuntimeConfig config)
    : actions_(actions), player_id_(config.id), team_(config.team), locally_controlled_(config.locally_controlled)
{
    GetTransform() = config.transform;
}

void PlayerEntity::ConfigureRuntime(std::uint64_t id, Generated::PlayerTeam team, bool locally_controlled,
                                    Actions actions)
{
    player_id_ = id;
    team_ = team;
    locally_controlled_ = locally_controlled;
    actions_ = actions;
}

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
        glm::normalize(glm::vec3(GetTransform().world_matrix * glm::vec4(1.0f, 0.0f, 0.0f, 0.0f)));
    look_angles_ = {std::atan2(direction.y, direction.x), std::asin(std::clamp(direction.z, -1.0f, 1.0f))};
    current_eye_height_ = eye_height;
    crouched_ = false;
    jump_was_down_ = false;
    if (context.physics != nullptr)
    {
        PhysicsCharacterDesc desc;
        desc.position = GetTransform().position;
        desc.radius = character_radius;
        desc.height = character_height;
        desc.mass = character_mass;
        desc.max_strength = max_strength;
        desc.max_slope_radians = max_slope_radians;
        desc.step_height = step_height;
        desc.stick_to_floor_distance = stick_to_floor_distance;
        character_ = context.physics->CreateCharacter(desc);
        if (character_ && context.scene != nullptr)
        {
            context.scene->RegisterPhysicsCharacter(character_, GetHandle());
        }
    }
    weapon_.Initialize(context);
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
    if (locally_controlled_ && context.input != nullptr)
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
    const glm::vec3 view_up = glm::normalize(glm::cross(right, forward));
    auto &transform = GetTransform();
    const bool has_local_input = locally_controlled_ && context.input != nullptr;
    const bool wants_crouch = has_local_input && context.input->Value(actions_.crouch) > 0.5f;
    if (wants_crouch != crouched_)
    {
        if (context.physics == nullptr || !character_ ||
            context.physics->SetCharacterHeight(character_, wants_crouch ? crouch_height : character_height))
        {
            crouched_ = wants_crouch;
        }
    }

    const bool sprinting = has_local_input && !crouched_ && context.input->Value(actions_.sprint) > 0.5f;
    float movement_speed_multiplier = sprinting ? 3.0f : 1.0f;
    if (crouched_)
    {
        movement_speed_multiplier = crouch_speed_multiplier;
    }
    const float speed = move_speed * movement_speed_multiplier;
    glm::vec3 movement = locally_controlled_ && context.input != nullptr
                             ? planar_forward * context.input->Value(actions_.move_forward) +
                                   right * context.input->Value(actions_.move_right)
                             : glm::vec3(0.0f);
    const float movement_length = glm::length(movement);
    if (movement_length > 1.0f)
    {
        movement /= movement_length;
    }
    if (context.physics != nullptr && character_)
    {
        PhysicsCharacterState state;
        if (context.physics->GetCharacterState(character_, state))
        {
            glm::vec3 velocity = movement * speed;
            velocity.z = state.velocity.z;
            const bool jump_down = has_local_input && context.input->Value(actions_.jump) > 0.5f;
            if (state.grounded && jump_down && !jump_was_down_)
            {
                velocity.z = jump_speed;
            }
            jump_was_down_ = jump_down;
            context.physics->SetCharacterVelocity(character_, velocity);
            context.physics->UpdateCharacter(character_, delta_seconds);
            if (context.physics->GetCharacterState(character_, state))
            {
                transform.position = state.position;
            }
        }
    }
    else
    {
        transform.position += movement * speed * delta_seconds;
    }
    transform.rotation =
        glm::angleAxis(look_angles_.x, up) * glm::angleAxis(-look_angles_.y, glm::vec3(0.0f, 1.0f, 0.0f));
    transform.dirty = true;
    transform.UpdateWorldMatrix();

    const float target_eye_height = crouched_ ? std::min(crouch_eye_height, crouch_height) : eye_height;
    const float maximum_eye_delta = std::max(crouch_transition_speed, 0.0f) * std::max(delta_seconds, 0.0f);
    current_eye_height_ += std::clamp(target_eye_height - current_eye_height_, -maximum_eye_delta, maximum_eye_delta);
    const glm::vec3 eye_position = transform.position + up * current_eye_height_;

    if (context.rendering != nullptr && context.scene != nullptr &&
        context.scene->Settings().active_entity == GetHandle())
    {
        CEngine::Renderer::Camera camera;
        camera.position = eye_position;
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

    PhysicsBallWeaponPose weapon_pose;
    weapon_pose.eye_position = eye_position;
    weapon_pose.aim_direction = forward;
    weapon_pose.right = right;
    weapon_pose.up = view_up;
    weapon_pose.first_person = view_mode == Generated::PlayerViewMode::FirstPerson;
    weapon_.Update(context, delta_seconds, has_local_input && context.input->Value(actions_.fire) > 0.5f, weapon_pose);
}

void PlayerEntity::LateUpdate(CEngine::Context &context, float /*delta_seconds*/)
{
    weapon_.LateUpdate(context);
}

/**
 * @brief TODO: Describe PlayerEntity::Shutdown.
 *
 * @param context TODO: Describe this parameter.
 */
void PlayerEntity::Shutdown(CEngine::Context &context)
{
    weapon_.Shutdown(context);
    if (context.physics != nullptr && character_)
    {
        if (context.scene != nullptr)
        {
            context.scene->UnregisterPhysicsCharacter(character_);
        }
        context.physics->DestroyCharacter(character_);
    }
    character_ = {};
}
} // namespace Viewer
