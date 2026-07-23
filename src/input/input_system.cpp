#include "input/input_system.h"

#include <cmath>

namespace CEngine::Input
{

ActionHandle InputSystem::RegisterAction(std::string name)
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
    const ActionHandle action{static_cast<std::uint32_t>(values_.size()), 1};
    actions_by_name_.emplace(std::move(name), action);
    values_.push_back(0.0f);
    return action;
}

bool InputSystem::BindKey(ActionHandle action, Key key, float scale)
{
    if (!action || action.Index() >= values_.size() || !std::isfinite(scale))
    {
        return false;
    }
    key_bindings_.push_back({action, key, scale});
    return true;
}

bool InputSystem::BindPointerAxis(ActionHandle action, PointerAxis axis, float scale)
{
    if (!action || action.Index() >= values_.size() || !std::isfinite(scale))
    {
        return false;
    }
    pointer_bindings_.push_back({action, axis, scale});
    return true;
}

void InputSystem::Set(ActionHandle action, float value)
{
    if (action && action.Index() < values_.size())
    {
        values_[action.Index()] = value;
    }
}

float InputSystem::Value(ActionHandle action) const
{
    return action && action.Index() < values_.size() ? values_[action.Index()] : 0.0f;
}

} // namespace CEngine::Input
