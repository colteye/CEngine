#ifndef CENGINE_SAMPLES_VIEWER_INPUT_ACTIONS_H
#define CENGINE_SAMPLES_VIEWER_INPUT_ACTIONS_H

#include "input/action.h"

namespace CEngine::Input { class InputSystem; }

namespace Viewer {

struct Actions {
    CEngine::Input::ActionId move_forward;
    CEngine::Input::ActionId move_right;
    CEngine::Input::ActionId look_yaw;
    CEngine::Input::ActionId look_pitch;
    CEngine::Input::ActionId sprint;
};

Actions RegisterActions(CEngine::Input::InputSystem& input);

} // namespace Viewer

#endif
