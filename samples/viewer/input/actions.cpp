#include "input/actions.h"

#include "input/input_system.h"

namespace Viewer
{

Actions RegisterActions(CEngine::Input::InputSystem &input)
{
    Actions actions;
    actions.move_forward = input.RegisterAction("move_forward");
    actions.move_right = input.RegisterAction("move_right");
    actions.look_yaw = input.RegisterAction("look_yaw");
    actions.look_pitch = input.RegisterAction("look_pitch");
    actions.sprint = input.RegisterAction("sprint");
    actions.jump = input.RegisterAction("jump");

    input.BindKey(actions.move_forward, CEngine::Input::Key::W, 1.0f);
    input.BindKey(actions.move_forward, CEngine::Input::Key::S, -1.0f);
    input.BindKey(actions.move_right, CEngine::Input::Key::D, 1.0f);
    input.BindKey(actions.move_right, CEngine::Input::Key::A, -1.0f);
    input.BindPointerAxis(actions.look_yaw, CEngine::Input::PointerAxis::X, -0.003f);
    input.BindPointerAxis(actions.look_pitch, CEngine::Input::PointerAxis::Y, -0.003f);
    input.BindKey(actions.sprint, CEngine::Input::Key::LeftShift);
    input.BindKey(actions.jump, CEngine::Input::Key::Space);
    return actions;
}

} // namespace Viewer
