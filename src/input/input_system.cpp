#include "input/input_system.h"

#include <cmath>

namespace CEngine::Input
{

ActionId InputSystem::RegisterAction(std::string name)
{
    if (name.empty())
    {
        return {};
    }
    const auto existing = actions_by_name_.find(name);
    if (existing != actions_by_name_.end())
    {
        return existing->second;
    }
    const ActionId action{static_cast<std::uint32_t>(values_.size())};
    actions_by_name_.emplace(std::move(name), action);
    values_.push_back(0.0f);
    return action;
}

bool InputSystem::BindKey(ActionId action, Key key, float scale)
{
    if (!action || action.index >= values_.size() || !std::isfinite(scale))
    {
        return false;
    }
    key_bindings_.push_back({action, key, scale});
    return true;
}

bool InputSystem::BindPointerAxis(ActionId action, PointerAxis axis, float scale)
{
    if (!action || action.index >= values_.size() || !std::isfinite(scale))
    {
        return false;
    }
    pointer_bindings_.push_back({action, axis, scale});
    return true;
}

void InputSystem::Set(ActionId action, float value)
{
    if (action && action.index < values_.size())
    {
        values_[action.index] = value;
    }
}

float InputSystem::Value(ActionId action) const
{
    return action && action.index < values_.size() ? values_[action.index] : 0.0f;
}

} // namespace CEngine::Input
