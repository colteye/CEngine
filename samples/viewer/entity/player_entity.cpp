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

PlayerEntity::PlayerEntity(
    Actions actions, PlayerRuntimeConfig config)
    : actions_(actions), player_id_(config.id), team_(config.team),
      locally_controlled_(config.locally_controlled)
{
    GetTransform() = config.transform;
}

void PlayerEntity::ConfigureRuntime(
    std::uint64_t id, Generated::PlayerTeam team,
    bool locally_controlled, Actions actions)
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
            context.scene->RegisterPhysicsCharacter(
                character_, GetHandle());
        }
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
    auto &transform = GetTransform();
    const float speed = move_speed *
                        (locally_controlled_ && context.input != nullptr &&
                                 context.input->Value(actions_.sprint) > 0.5f
                             ? 3.0f
                             : 1.0f);
    const glm::vec3 movement =
        locally_controlled_ && context.input != nullptr
            ? planar_forward *
                      context.input->Value(actions_.move_forward) +
                  right * context.input->Value(actions_.move_right)
            : glm::vec3(0.0f);
    if (context.physics != nullptr && character_)
    {
        PhysicsCharacterState state;
        if (context.physics->GetCharacterState(character_, state))
        {
            glm::vec3 velocity = movement * speed;
            velocity.z = state.velocity.z;
            if (state.grounded && locally_controlled_ &&
                context.input != nullptr &&
                context.input->Value(actions_.jump) > 0.5f)
            {
                velocity.z = jump_speed;
            }
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
        glm::angleAxis(look_angles_.x, up) *
        glm::angleAxis(-look_angles_.y, glm::vec3(0.0f, 1.0f, 0.0f));
    transform.dirty = true;
    transform.UpdateWorldMatrix();

    if (context.rendering != nullptr && context.scene != nullptr &&
        context.scene->Settings().active_entity == GetHandle())
    {
        CEngine::Renderer::Camera camera;
        camera.position = transform.position + up * eye_height;
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
        if (context.scene != nullptr)
        {
            context.scene->UnregisterPhysicsCharacter(character_);
        }
        context.physics->DestroyCharacter(character_);
    }
    character_ = {};
}
} // namespace Viewer
