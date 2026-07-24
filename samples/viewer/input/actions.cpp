//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file samples/viewer/input/actions.cpp
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#include "input/actions.h"

#include "input/input_system.h"

#include <string>

namespace Viewer
{

/**
 * @brief TODO: Describe RegisterActions.
 *
 * @param input TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
Actions RegisterActions(
    CEngine::Input::InputSystem &input, std::string_view prefix)
{
    const auto action_name = [prefix](std::string_view name) {
        return prefix.empty()
                   ? std::string(name)
                   : std::string(prefix) + "." + std::string(name);
    };
    Actions actions;
    actions.move_forward =
        input.RegisterAction(action_name("move_forward"));
    actions.move_right =
        input.RegisterAction(action_name("move_right"));
    actions.look_yaw = input.RegisterAction(action_name("look_yaw"));
    actions.look_pitch =
        input.RegisterAction(action_name("look_pitch"));
    actions.sprint = input.RegisterAction(action_name("sprint"));
    actions.jump = input.RegisterAction(action_name("jump"));
    actions.crouch = input.RegisterAction(action_name("crouch"));
    actions.fire = input.RegisterAction(action_name("fire"));

    input.BindKey(actions.move_forward, CEngine::Input::Key::W, 1.0f);
    input.BindKey(actions.move_forward, CEngine::Input::Key::S, -1.0f);
    input.BindKey(actions.move_right, CEngine::Input::Key::D, 1.0f);
    input.BindKey(actions.move_right, CEngine::Input::Key::A, -1.0f);
    input.BindPointerAxis(actions.look_yaw, CEngine::Input::PointerAxis::X, -0.003f);
    input.BindPointerAxis(actions.look_pitch, CEngine::Input::PointerAxis::Y, -0.003f);
    input.BindKey(actions.sprint, CEngine::Input::Key::LeftShift);
    input.BindKey(actions.jump, CEngine::Input::Key::Space);
    input.BindKey(actions.crouch, CEngine::Input::Key::LeftControl);
    input.BindPointerButton(actions.fire, CEngine::Input::PointerButton::Primary);
    return actions;
}

} // namespace Viewer
