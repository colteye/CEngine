//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/entity/trigger_entity.cpp
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#include "entity/trigger_entity.h"

#include "assets/store.h"
#include "context.h"
#include "physics/physics_system.h"
#include "scene/scene.h"

#include <algorithm>
#include <stdexcept>

namespace CEngine::Entities
{

std::string_view TriggerEntity::Classname() const
{
    return "trigger_volume";
}

bool TriggerEntity::AcceptsInput(std::string_view input) const
{
    return input == "Reset" || Entity::AcceptsInput(input);
}

bool TriggerEntity::HasOutput(std::string_view output) const
{
    return output == "OnEnter" || output == "OnExit" || output == "OnStay" || output == "OnTrigger" ||
           Entity::HasOutput(output);
}

bool TriggerEntity::HandleInput(Context &context, const Scene::EntityInput &input)
{
    if (input.name != "Reset")
    {
        return Entity::HandleInput(context, input);
    }
    fired_ = false;
    overlaps_.clear();
    if (!Enabled())
    {
        SetEnabled(context, true);
    }
    return true;
}

void TriggerEntity::Initialize(Context &context)
{
    if (context.physics == nullptr)
    {
        return;
    }
    if (context.scene == nullptr || context.assets == nullptr)
    {
        throw std::runtime_error("trigger volume requires scene assets");
    }
    const Assets::Reference *reference = context.scene->AssetReference(collision.index);
    if (reference == nullptr || reference->type != Assets::Type::Physics)
    {
        throw std::runtime_error("trigger volume collision reference is invalid");
    }
    shape_ = context.assets->LoadPhysics(*reference);
    if (!shape_)
    {
        throw std::runtime_error("could not load trigger volume geometry");
    }
    if (Enabled() && !CreateBody(context))
    {
        throw std::runtime_error("physics rejected trigger volume");
    }
}

bool TriggerEntity::CreateBody(Context &context)
{
    if (body_ || context.physics == nullptr || context.scene == nullptr || !shape_)
    {
        return static_cast<bool>(body_);
    }
    PhysicsBodyDesc desc;
    desc.motion_type = kinematic ? PhysicsMotionType::Kinematic : PhysicsMotionType::Static;
    desc.position = GetTransform().position;
    desc.rotation = GetTransform().rotation;
    desc.collision_layer = static_cast<std::uint8_t>(collision_layer);
    desc.sensor = true;
    body_ = context.physics->CreateBody(desc, *shape_);
    if (!body_)
    {
        return false;
    }
    context.scene->RegisterPhysicsBody(body_, GetHandle());
    return true;
}

void TriggerEntity::DestroyBody(Context &context)
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
    overlaps_.clear();
}

void TriggerEntity::Update(Context &context, float delta_seconds)
{
    if (kinematic && context.physics != nullptr && body_ && delta_seconds > 0.0f)
    {
        context.physics->MoveKinematic(body_, GetTransform().position, GetTransform().rotation, delta_seconds);
    }
}

TriggerEntity::Overlap *TriggerEntity::FindOverlap(Scene::EntityHandle entity)
{
    const auto found = std::find_if(overlaps_.begin(), overlaps_.end(),
                                    [entity](const Overlap &overlap) { return overlap.entity == entity; });
    return found == overlaps_.end() ? nullptr : &*found;
}

void TriggerEntity::OnPhysicsContact(Context &context, const Scene::EntityContact &contact)
{
    if (context.scene == nullptr || (fire_once && fired_))
    {
        return;
    }
    if (contact.phase == Scene::EntityContactPhase::Begin)
    {
        Overlap *overlap = FindOverlap(contact.other);
        if (overlap == nullptr)
        {
            overlaps_.push_back({contact.other, 1});
            context.scene->Emit(GetHandle(), "OnEnter", contact.other);
            context.scene->Emit(GetHandle(), "OnTrigger", contact.other);
            fired_ = true;
            if (fire_once)
            {
                SetEnabled(context, false);
            }
        }
        else
        {
            ++overlap->contacts;
        }
    }
    else if (contact.phase == Scene::EntityContactPhase::Persist && emit_stay)
    {
        context.scene->Emit(GetHandle(), "OnStay", contact.other);
    }
    else if (contact.phase == Scene::EntityContactPhase::End)
    {
        Overlap *overlap = FindOverlap(contact.other);
        if (overlap != nullptr && --overlap->contacts == 0)
        {
            context.scene->Emit(GetHandle(), "OnExit", contact.other);
            overlaps_.erase(std::remove_if(overlaps_.begin(), overlaps_.end(),
                                           [entity = contact.other](const Overlap &candidate) {
                                               return candidate.entity == entity;
                                           }),
                            overlaps_.end());
        }
    }
}

void TriggerEntity::OnEnabledChanged(Context &context, bool enabled)
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

void TriggerEntity::Shutdown(Context &context)
{
    DestroyBody(context);
    shape_.reset();
    fired_ = false;
}

} // namespace CEngine::Entities
