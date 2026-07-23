//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file samples/viewer/input/actions.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#ifndef CENGINE_SAMPLES_VIEWER_INPUT_ACTIONS_H
#define CENGINE_SAMPLES_VIEWER_INPUT_ACTIONS_H

#include "input/input_system.h"

namespace CEngine::Input
{
class InputSystem;
}

namespace Viewer
{

/**
 * @brief TODO: Describe Actions.
 */
struct Actions
{
    CEngine::Input::ActionHandle move_forward;
    CEngine::Input::ActionHandle move_right;
    CEngine::Input::ActionHandle look_yaw;
    CEngine::Input::ActionHandle look_pitch;
    CEngine::Input::ActionHandle sprint;
    CEngine::Input::ActionHandle jump;
};

/**
 * @brief TODO: Describe RegisterActions.
 *
 * @param input TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
Actions RegisterActions(CEngine::Input::InputSystem &input);

} // namespace Viewer

#endif
