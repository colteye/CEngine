//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/input/input_system.cpp
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#include "input/input_system.h"

#include <cmath>

namespace CEngine::Input
{

/**
 * @brief TODO: Describe InputSystem::RegisterAction.
 *
 * @param name TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
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

/**
 * @brief TODO: Describe InputSystem::BindKey.
 *
 * @param action TODO: Describe this parameter.
 * @param key TODO: Describe this parameter.
 * @param scale TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool InputSystem::BindKey(ActionHandle action, Key key, float scale)
{
    if (!action || action.Index() >= values_.size() || !std::isfinite(scale))
    {
        return false;
    }
    key_bindings_.push_back({action, key, scale});
    return true;
}

bool InputSystem::BindPointerButton(ActionHandle action, PointerButton button, float scale)
{
    if (!action || action.Index() >= values_.size() || !std::isfinite(scale))
    {
        return false;
    }
    pointer_button_bindings_.push_back({action, button, scale});
    return true;
}

/**
 * @brief TODO: Describe InputSystem::BindPointerAxis.
 *
 * @param action TODO: Describe this parameter.
 * @param axis TODO: Describe this parameter.
 * @param scale TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool InputSystem::BindPointerAxis(ActionHandle action, PointerAxis axis, float scale)
{
    if (!action || action.Index() >= values_.size() || !std::isfinite(scale))
    {
        return false;
    }
    pointer_bindings_.push_back({action, axis, scale});
    return true;
}

/**
 * @brief TODO: Describe InputSystem::Set.
 *
 * @param action TODO: Describe this parameter.
 * @param value TODO: Describe this parameter.
 */
void InputSystem::Set(ActionHandle action, float value)
{
    if (action && action.Index() < values_.size())
    {
        values_[action.Index()] = value;
    }
}

/**
 * @brief TODO: Describe InputSystem::Value.
 *
 * @param action TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
float InputSystem::Value(ActionHandle action) const
{
    return action && action.Index() < values_.size() ? values_[action.Index()] : 0.0f;
}

} // namespace CEngine::Input
