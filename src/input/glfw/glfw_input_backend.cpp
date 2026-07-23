//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/input/glfw/glfw_input_backend.cpp
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#include "input/glfw/glfw_input_backend.h"
#include "input/input_system.h"

#include <GLFW/glfw3.h>

namespace CEngine::Input
{
namespace
{

/**
 * @brief TODO: Describe GlfwKey.
 *
 * @param key TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
int GlfwKey(Key key)
{
    switch (key)
    {
    case Key::Space:
        return GLFW_KEY_SPACE;
    case Key::Apostrophe:
        return GLFW_KEY_APOSTROPHE;
    case Key::Comma:
        return GLFW_KEY_COMMA;
    case Key::Minus:
        return GLFW_KEY_MINUS;
    case Key::Period:
        return GLFW_KEY_PERIOD;
    case Key::Slash:
        return GLFW_KEY_SLASH;
    case Key::Digit0:
        return GLFW_KEY_0;
    case Key::Digit1:
        return GLFW_KEY_1;
    case Key::Digit2:
        return GLFW_KEY_2;
    case Key::Digit3:
        return GLFW_KEY_3;
    case Key::Digit4:
        return GLFW_KEY_4;
    case Key::Digit5:
        return GLFW_KEY_5;
    case Key::Digit6:
        return GLFW_KEY_6;
    case Key::Digit7:
        return GLFW_KEY_7;
    case Key::Digit8:
        return GLFW_KEY_8;
    case Key::Digit9:
        return GLFW_KEY_9;
    case Key::Semicolon:
        return GLFW_KEY_SEMICOLON;
    case Key::Equal:
        return GLFW_KEY_EQUAL;
    case Key::A:
        return GLFW_KEY_A;
    case Key::B:
        return GLFW_KEY_B;
    case Key::C:
        return GLFW_KEY_C;
    case Key::D:
        return GLFW_KEY_D;
    case Key::E:
        return GLFW_KEY_E;
    case Key::F:
        return GLFW_KEY_F;
    case Key::G:
        return GLFW_KEY_G;
    case Key::H:
        return GLFW_KEY_H;
    case Key::I:
        return GLFW_KEY_I;
    case Key::J:
        return GLFW_KEY_J;
    case Key::K:
        return GLFW_KEY_K;
    case Key::L:
        return GLFW_KEY_L;
    case Key::M:
        return GLFW_KEY_M;
    case Key::N:
        return GLFW_KEY_N;
    case Key::O:
        return GLFW_KEY_O;
    case Key::P:
        return GLFW_KEY_P;
    case Key::Q:
        return GLFW_KEY_Q;
    case Key::R:
        return GLFW_KEY_R;
    case Key::S:
        return GLFW_KEY_S;
    case Key::T:
        return GLFW_KEY_T;
    case Key::U:
        return GLFW_KEY_U;
    case Key::V:
        return GLFW_KEY_V;
    case Key::W:
        return GLFW_KEY_W;
    case Key::X:
        return GLFW_KEY_X;
    case Key::Y:
        return GLFW_KEY_Y;
    case Key::Z:
        return GLFW_KEY_Z;
    case Key::LeftBracket:
        return GLFW_KEY_LEFT_BRACKET;
    case Key::Backslash:
        return GLFW_KEY_BACKSLASH;
    case Key::RightBracket:
        return GLFW_KEY_RIGHT_BRACKET;
    case Key::GraveAccent:
        return GLFW_KEY_GRAVE_ACCENT;
    case Key::World1:
        return GLFW_KEY_WORLD_1;
    case Key::World2:
        return GLFW_KEY_WORLD_2;
    case Key::Escape:
        return GLFW_KEY_ESCAPE;
    case Key::Enter:
        return GLFW_KEY_ENTER;
    case Key::Tab:
        return GLFW_KEY_TAB;
    case Key::Backspace:
        return GLFW_KEY_BACKSPACE;
    case Key::Insert:
        return GLFW_KEY_INSERT;
    case Key::Delete:
        return GLFW_KEY_DELETE;
    case Key::Right:
        return GLFW_KEY_RIGHT;
    case Key::Left:
        return GLFW_KEY_LEFT;
    case Key::Down:
        return GLFW_KEY_DOWN;
    case Key::Up:
        return GLFW_KEY_UP;
    case Key::PageUp:
        return GLFW_KEY_PAGE_UP;
    case Key::PageDown:
        return GLFW_KEY_PAGE_DOWN;
    case Key::Home:
        return GLFW_KEY_HOME;
    case Key::End:
        return GLFW_KEY_END;
    case Key::CapsLock:
        return GLFW_KEY_CAPS_LOCK;
    case Key::ScrollLock:
        return GLFW_KEY_SCROLL_LOCK;
    case Key::NumLock:
        return GLFW_KEY_NUM_LOCK;
    case Key::PrintScreen:
        return GLFW_KEY_PRINT_SCREEN;
    case Key::Pause:
        return GLFW_KEY_PAUSE;
    case Key::F1:
        return GLFW_KEY_F1;
    case Key::F2:
        return GLFW_KEY_F2;
    case Key::F3:
        return GLFW_KEY_F3;
    case Key::F4:
        return GLFW_KEY_F4;
    case Key::F5:
        return GLFW_KEY_F5;
    case Key::F6:
        return GLFW_KEY_F6;
    case Key::F7:
        return GLFW_KEY_F7;
    case Key::F8:
        return GLFW_KEY_F8;
    case Key::F9:
        return GLFW_KEY_F9;
    case Key::F10:
        return GLFW_KEY_F10;
    case Key::F11:
        return GLFW_KEY_F11;
    case Key::F12:
        return GLFW_KEY_F12;
    case Key::F13:
        return GLFW_KEY_F13;
    case Key::F14:
        return GLFW_KEY_F14;
    case Key::F15:
        return GLFW_KEY_F15;
    case Key::F16:
        return GLFW_KEY_F16;
    case Key::F17:
        return GLFW_KEY_F17;
    case Key::F18:
        return GLFW_KEY_F18;
    case Key::F19:
        return GLFW_KEY_F19;
    case Key::F20:
        return GLFW_KEY_F20;
    case Key::F21:
        return GLFW_KEY_F21;
    case Key::F22:
        return GLFW_KEY_F22;
    case Key::F23:
        return GLFW_KEY_F23;
    case Key::F24:
        return GLFW_KEY_F24;
    case Key::F25:
        return GLFW_KEY_F25;
    case Key::Keypad0:
        return GLFW_KEY_KP_0;
    case Key::Keypad1:
        return GLFW_KEY_KP_1;
    case Key::Keypad2:
        return GLFW_KEY_KP_2;
    case Key::Keypad3:
        return GLFW_KEY_KP_3;
    case Key::Keypad4:
        return GLFW_KEY_KP_4;
    case Key::Keypad5:
        return GLFW_KEY_KP_5;
    case Key::Keypad6:
        return GLFW_KEY_KP_6;
    case Key::Keypad7:
        return GLFW_KEY_KP_7;
    case Key::Keypad8:
        return GLFW_KEY_KP_8;
    case Key::Keypad9:
        return GLFW_KEY_KP_9;
    case Key::KeypadDecimal:
        return GLFW_KEY_KP_DECIMAL;
    case Key::KeypadDivide:
        return GLFW_KEY_KP_DIVIDE;
    case Key::KeypadMultiply:
        return GLFW_KEY_KP_MULTIPLY;
    case Key::KeypadSubtract:
        return GLFW_KEY_KP_SUBTRACT;
    case Key::KeypadAdd:
        return GLFW_KEY_KP_ADD;
    case Key::KeypadEnter:
        return GLFW_KEY_KP_ENTER;
    case Key::KeypadEqual:
        return GLFW_KEY_KP_EQUAL;
    case Key::LeftShift:
        return GLFW_KEY_LEFT_SHIFT;
    case Key::LeftControl:
        return GLFW_KEY_LEFT_CONTROL;
    case Key::LeftAlt:
        return GLFW_KEY_LEFT_ALT;
    case Key::LeftSuper:
        return GLFW_KEY_LEFT_SUPER;
    case Key::RightShift:
        return GLFW_KEY_RIGHT_SHIFT;
    case Key::RightControl:
        return GLFW_KEY_RIGHT_CONTROL;
    case Key::RightAlt:
        return GLFW_KEY_RIGHT_ALT;
    case Key::RightSuper:
        return GLFW_KEY_RIGHT_SUPER;
    case Key::Menu:
        return GLFW_KEY_MENU;
    case Key::Count:
        break;
    }
    return GLFW_KEY_UNKNOWN;
}

} // namespace

/**
 * @brief TODO: Describe GlfwInputBackend::GlfwInputBackend.
 *
 * @param window TODO: Describe this parameter.
 */
GlfwInputBackend::GlfwInputBackend(GLFWwindow *window) : window_(window)
{
}

/**
 * @brief TODO: Describe GlfwInputBackend::BeginFrame.
 */
void GlfwInputBackend::BeginFrame()
{
    pointer_delta_ = glm::vec2(0.0f);
    if (window_ == nullptr)
    {
        return;
    }

    const bool focused = glfwGetWindowAttrib(window_, GLFW_FOCUSED) == GLFW_TRUE;
    double cursor_x = 0.0;
    double cursor_y = 0.0;
    glfwGetCursorPos(window_, &cursor_x, &cursor_y);
    if (focused && was_focused_ && has_cursor_sample_)
    {
        pointer_delta_ = {static_cast<float>(cursor_x - previous_cursor_x_),
                          static_cast<float>(cursor_y - previous_cursor_y_)};
    }
    previous_cursor_x_ = cursor_x;
    previous_cursor_y_ = cursor_y;
    has_cursor_sample_ = focused;
    was_focused_ = focused;
}

/**
 * @brief TODO: Describe GlfwInputBackend::IsDown.
 *
 * @param key TODO: Describe this parameter.
 * @return TODO: Describe the return value.
 */
bool GlfwInputBackend::IsDown(Key key) const
{
    const int glfw_key = GlfwKey(key);
    return window_ != nullptr && glfw_key != GLFW_KEY_UNKNOWN && glfwGetKey(window_, glfw_key) == GLFW_PRESS;
}

/**
 * @brief TODO: Describe GlfwInputBackend::SetPointerCaptured.
 *
 * @param captured TODO: Describe this parameter.
 */
void GlfwInputBackend::SetPointerCaptured(bool captured)
{
    if (window_ == nullptr || pointer_captured_ == captured)
    {
        return;
    }
    pointer_captured_ = captured;
    glfwSetInputMode(window_, GLFW_CURSOR, captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    if (glfwRawMouseMotionSupported() == GLFW_TRUE)
    {
        glfwSetInputMode(window_, GLFW_RAW_MOUSE_MOTION, captured ? GLFW_TRUE : GLFW_FALSE);
    }
    has_cursor_sample_ = false;
    was_focused_ = false;
    pointer_delta_ = glm::vec2(0.0f);
}

} // namespace CEngine::Input
