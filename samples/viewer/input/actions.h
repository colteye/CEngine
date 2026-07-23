#ifndef CENGINE_SAMPLES_VIEWER_INPUT_ACTIONS_H
#define CENGINE_SAMPLES_VIEWER_INPUT_ACTIONS_H

#include "input/input_system.h"

namespace CEngine::Input
{
class InputSystem;
}

namespace Viewer
{

struct Actions
{
    CEngine::Input::ActionHandle move_forward;
    CEngine::Input::ActionHandle move_right;
    CEngine::Input::ActionHandle look_yaw;
    CEngine::Input::ActionHandle look_pitch;
    CEngine::Input::ActionHandle sprint;
    CEngine::Input::ActionHandle jump;
};

Actions RegisterActions(CEngine::Input::InputSystem &input);

} // namespace Viewer

#endif
