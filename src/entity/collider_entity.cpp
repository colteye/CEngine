//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/entity/collider_entity.cpp
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#include "entity/collider_entity.h"

#include "assets/store.h"
#include "context.h"
#include "physics/physics_system.h"
#include "scene/scene.h"

#include <stdexcept>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace CEngine::Entities
{
namespace
{

PhysicsMotionType RuntimeMotion(Generated::EngineEntities::ColliderMotion motion)
{
    switch (motion)
    {
    case Generated::EngineEntities::ColliderMotion::Static:
        return PhysicsMotionType::Static;
    case Generated::EngineEntities::ColliderMotion::Dynamic:
        return PhysicsMotionType::Dynamic;
    case Generated::EngineEntities::ColliderMotion::Kinematic:
        return PhysicsMotionType::Kinematic;
    }
    return PhysicsMotionType::Static;
}

} // namespace

std::string_view ColliderEntity::Classname() const
{
    return "collider";
}

bool ColliderEntity::HasOutput(std::string_view output) const
{
    return output == "OnContactBegin" || output == "OnContactEnd" || Entity::HasOutput(output);
}

void ColliderEntity::Initialize(Context &context)
{
    if (context.physics == nullptr)
    {
        return;
    }
    if (context.scene == nullptr || context.assets == nullptr)
    {
        throw std::runtime_error("collider requires scene assets");
    }
    const Assets::Reference *reference = context.scene->AssetReference(collision.index);
    if (reference == nullptr || reference->type != Assets::Type::Physics)
    {
        throw std::runtime_error("collider collision reference is invalid");
    }
    shape_ = context.assets->LoadPhysics(*reference);
    if (!shape_)
    {
        throw std::runtime_error("could not load collider geometry");
    }
    if (Enabled() && !CreateBody(context))
    {
        throw std::runtime_error("physics rejected collider body");
    }
}

bool ColliderEntity::CreateBody(Context &context)
{
    if (body_ || context.physics == nullptr || context.scene == nullptr ||
        !shape_)
    {
        return static_cast<bool>(body_);
    }
    PhysicsBodyDesc desc;
    desc.motion_type = RuntimeMotion(motion);
    desc.position = GetTransform().position;
    desc.rotation = GetTransform().rotation;
    desc.mass = mass;
    desc.friction = friction;
    desc.restitution = restitution;
    desc.linear_damping = linear_damping;
    desc.angular_damping = angular_damping;
    desc.gravity_factor = gravity_factor;
    desc.collision_layer = static_cast<std::uint8_t>(collision_layer);
    desc.continuous = continuous;
    desc.allow_sleeping = allow_sleeping;
    body_ = context.physics->CreateBody(desc, *shape_);
    if (!body_)
    {
        return false;
    }
    context.scene->RegisterPhysicsBody(body_, GetHandle());
    return true;
}

void ColliderEntity::Update(Context &context, float delta_seconds)
{
    if (context.physics != nullptr && body_ && motion == Generated::EngineEntities::ColliderMotion::Kinematic &&
        delta_seconds > 0.0f)
    {
        context.physics->MoveKinematic(body_, GetTransform().position, GetTransform().rotation, delta_seconds);
    }
}

void ColliderEntity::LateUpdate(Context &context, float /*delta_seconds*/)
{
    if (context.physics == nullptr || !body_ || motion != Generated::EngineEntities::ColliderMotion::Dynamic)
    {
        return;
    }
    PhysicsBodyState state;
    if (context.physics->GetBodyState(body_, state))
    {
        GetTransform().position = state.position;
        GetTransform().rotation = state.rotation;
        GetTransform().world_matrix = glm::translate(glm::mat4(1.0f), state.position) *
                                      glm::mat4_cast(state.rotation) *
                                      glm::scale(glm::mat4(1.0f), GetTransform().scale);
        GetTransform().dirty = false;
    }
}

void ColliderEntity::OnPhysicsContact(Context &context, const Scene::EntityContact &contact)
{
    if (context.scene == nullptr)
    {
        return;
    }
    if (contact.phase == Scene::EntityContactPhase::Begin)
    {
        context.scene->Emit(GetHandle(), "OnContactBegin", contact.other);
    }
    else if (contact.phase == Scene::EntityContactPhase::End)
    {
        context.scene->Emit(GetHandle(), "OnContactEnd", contact.other);
    }
}

void ColliderEntity::DestroyBody(Context &context)
{
    if (context.scene != nullptr && body_)
    {
        context.scene->UnregisterPhysicsBody(body_);
    }
    if (context.physics != nullptr && body_)
    {
        context.physics->DestroyBody(body_);
    }
    body_ = {};
}

void ColliderEntity::OnEnabledChanged(Context &context, bool enabled)
{
    if (enabled)
    {
        CreateBody(context);
    }
    else
    {
        DestroyBody(context);
    }
}

void ColliderEntity::Shutdown(Context &context)
{
    DestroyBody(context);
    shape_.reset();
}

} // namespace CEngine::Entities
