//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/entity/logic_entities.cpp
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#include "entity/logic_entities.h"

#include "context.h"
#include "scene/scene.h"

#include <algorithm>

namespace CEngine::Entities
{

std::string_view LogicRelayEntity::Classname() const
{
    return "logic_relay";
}

bool LogicRelayEntity::AcceptsInput(std::string_view input) const
{
    return input == "Trigger" || input == "Reset" || Entity::AcceptsInput(input);
}

bool LogicRelayEntity::HasOutput(std::string_view output) const
{
    return output == "OnTrigger" || Entity::HasOutput(output);
}

bool LogicRelayEntity::HandleInput(Context &context, const Scene::EntityInput &input)
{
    if (input.name == "Reset")
    {
        fired_ = false;
        return true;
    }
    if (input.name == "Trigger")
    {
        if (!Enabled() || (fire_once && fired_))
        {
            return false;
        }
        fired_ = true;
        if (context.scene != nullptr)
        {
            context.scene->Emit(GetHandle(), "OnTrigger", input.activator, input.parameter);
        }
        return true;
    }
    return Entity::HandleInput(context, input);
}

void LogicRelayEntity::Initialize(Context & /*context*/)
{
    fired_ = false;
}

std::string_view LogicTimerEntity::Classname() const
{
    return "logic_timer";
}

bool LogicTimerEntity::AcceptsInput(std::string_view input) const
{
    return input == "Reset" || input == "Fire" || Entity::AcceptsInput(input);
}

bool LogicTimerEntity::HasOutput(std::string_view output) const
{
    return output == "OnTimer" || Entity::HasOutput(output);
}

bool LogicTimerEntity::HandleInput(Context &context, const Scene::EntityInput &input)
{
    if (input.name == "Reset")
    {
        remaining_seconds_ = initial_delay_seconds > 0.0f ? initial_delay_seconds : interval_seconds;
        return true;
    }
    if (input.name == "Fire")
    {
        Fire(context, input.activator);
        return true;
    }
    return Entity::HandleInput(context, input);
}

void LogicTimerEntity::Initialize(Context &context)
{
    remaining_seconds_ = initial_delay_seconds > 0.0f ? initial_delay_seconds : interval_seconds;
    initialized_ = true;
    if (!start_enabled)
    {
        SetFlags(Flags() & ~Scene::EntityEnabled);
    }
    if (fire_on_start && Enabled())
    {
        Fire(context, GetHandle());
    }
}

void LogicTimerEntity::Fire(Context &context, Scene::EntityHandle activator)
{
    if (context.scene != nullptr)
    {
        context.scene->Emit(GetHandle(), "OnTimer", activator);
    }
    if (repeat)
    {
        remaining_seconds_ = interval_seconds;
    }
    else
    {
        SetEnabled(context, false);
    }
}

void LogicTimerEntity::Update(Context &context, float delta_seconds)
{
    if (!initialized_ || delta_seconds <= 0.0f)
    {
        return;
    }
    remaining_seconds_ -= delta_seconds;
    std::uint32_t iterations = 0;
    while (remaining_seconds_ <= 0.0f && Enabled() && iterations++ < 16u)
    {
        const float overshoot = -remaining_seconds_;
        Fire(context, GetHandle());
        if (repeat && Enabled())
        {
            remaining_seconds_ = std::max(interval_seconds - overshoot, 0.0f);
        }
    }
}

std::string_view LogicAutoEntity::Classname() const
{
    return "logic_auto";
}

bool LogicAutoEntity::AcceptsInput(std::string_view input) const
{
    return input == "Fire" || Entity::AcceptsInput(input);
}

bool LogicAutoEntity::HasOutput(std::string_view output) const
{
    return output == "OnSceneActivated" || Entity::HasOutput(output);
}

bool LogicAutoEntity::HandleInput(Context &context, const Scene::EntityInput &input)
{
    if (input.name == "Fire")
    {
        if (context.scene != nullptr)
        {
            context.scene->Emit(
                GetHandle(), "OnSceneActivated", input.activator, input.parameter);
        }
        return true;
    }
    return Entity::HandleInput(context, input);
}

void LogicAutoEntity::Initialize(Context & /*context*/)
{
    armed_ = true;
}

void LogicAutoEntity::Update(Context &context, float /*delta_seconds*/)
{
    if (!armed_)
    {
        return;
    }
    armed_ = false;
    if (context.scene != nullptr)
    {
        context.scene->Emit(GetHandle(), "OnSceneActivated", GetHandle());
    }
}

} // namespace CEngine::Entities
