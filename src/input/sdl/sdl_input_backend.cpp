// Copyright (c) CEngine contributors.

#include "input/sdl/sdl_input_backend.h"

#include "input/input_system.h"
#include "window/window_system.h"

#include <SDL3/SDL.h>

#include <array>
#include <cstddef>

namespace CEngine::Input
{
namespace
{

constexpr std::array<SDL_Scancode, static_cast<std::size_t>(Key::Count)> KeyScancodes = {
    SDL_SCANCODE_SPACE,
    SDL_SCANCODE_APOSTROPHE,
    SDL_SCANCODE_COMMA,
    SDL_SCANCODE_MINUS,
    SDL_SCANCODE_PERIOD,
    SDL_SCANCODE_SLASH,
    SDL_SCANCODE_0,
    SDL_SCANCODE_1,
    SDL_SCANCODE_2,
    SDL_SCANCODE_3,
    SDL_SCANCODE_4,
    SDL_SCANCODE_5,
    SDL_SCANCODE_6,
    SDL_SCANCODE_7,
    SDL_SCANCODE_8,
    SDL_SCANCODE_9,
    SDL_SCANCODE_SEMICOLON,
    SDL_SCANCODE_EQUALS,
    SDL_SCANCODE_A,
    SDL_SCANCODE_B,
    SDL_SCANCODE_C,
    SDL_SCANCODE_D,
    SDL_SCANCODE_E,
    SDL_SCANCODE_F,
    SDL_SCANCODE_G,
    SDL_SCANCODE_H,
    SDL_SCANCODE_I,
    SDL_SCANCODE_J,
    SDL_SCANCODE_K,
    SDL_SCANCODE_L,
    SDL_SCANCODE_M,
    SDL_SCANCODE_N,
    SDL_SCANCODE_O,
    SDL_SCANCODE_P,
    SDL_SCANCODE_Q,
    SDL_SCANCODE_R,
    SDL_SCANCODE_S,
    SDL_SCANCODE_T,
    SDL_SCANCODE_U,
    SDL_SCANCODE_V,
    SDL_SCANCODE_W,
    SDL_SCANCODE_X,
    SDL_SCANCODE_Y,
    SDL_SCANCODE_Z,
    SDL_SCANCODE_LEFTBRACKET,
    SDL_SCANCODE_BACKSLASH,
    SDL_SCANCODE_RIGHTBRACKET,
    SDL_SCANCODE_GRAVE,
    SDL_SCANCODE_NONUSBACKSLASH,
    SDL_SCANCODE_INTERNATIONAL1,
    SDL_SCANCODE_ESCAPE,
    SDL_SCANCODE_RETURN,
    SDL_SCANCODE_TAB,
    SDL_SCANCODE_BACKSPACE,
    SDL_SCANCODE_INSERT,
    SDL_SCANCODE_DELETE,
    SDL_SCANCODE_RIGHT,
    SDL_SCANCODE_LEFT,
    SDL_SCANCODE_DOWN,
    SDL_SCANCODE_UP,
    SDL_SCANCODE_PAGEUP,
    SDL_SCANCODE_PAGEDOWN,
    SDL_SCANCODE_HOME,
    SDL_SCANCODE_END,
    SDL_SCANCODE_CAPSLOCK,
    SDL_SCANCODE_SCROLLLOCK,
    SDL_SCANCODE_NUMLOCKCLEAR,
    SDL_SCANCODE_PRINTSCREEN,
    SDL_SCANCODE_PAUSE,
    SDL_SCANCODE_F1,
    SDL_SCANCODE_F2,
    SDL_SCANCODE_F3,
    SDL_SCANCODE_F4,
    SDL_SCANCODE_F5,
    SDL_SCANCODE_F6,
    SDL_SCANCODE_F7,
    SDL_SCANCODE_F8,
    SDL_SCANCODE_F9,
    SDL_SCANCODE_F10,
    SDL_SCANCODE_F11,
    SDL_SCANCODE_F12,
    SDL_SCANCODE_F13,
    SDL_SCANCODE_F14,
    SDL_SCANCODE_F15,
    SDL_SCANCODE_F16,
    SDL_SCANCODE_F17,
    SDL_SCANCODE_F18,
    SDL_SCANCODE_F19,
    SDL_SCANCODE_F20,
    SDL_SCANCODE_F21,
    SDL_SCANCODE_F22,
    SDL_SCANCODE_F23,
    SDL_SCANCODE_F24,
    SDL_SCANCODE_UNKNOWN,
    SDL_SCANCODE_KP_0,
    SDL_SCANCODE_KP_1,
    SDL_SCANCODE_KP_2,
    SDL_SCANCODE_KP_3,
    SDL_SCANCODE_KP_4,
    SDL_SCANCODE_KP_5,
    SDL_SCANCODE_KP_6,
    SDL_SCANCODE_KP_7,
    SDL_SCANCODE_KP_8,
    SDL_SCANCODE_KP_9,
    SDL_SCANCODE_KP_PERIOD,
    SDL_SCANCODE_KP_DIVIDE,
    SDL_SCANCODE_KP_MULTIPLY,
    SDL_SCANCODE_KP_MINUS,
    SDL_SCANCODE_KP_PLUS,
    SDL_SCANCODE_KP_ENTER,
    SDL_SCANCODE_KP_EQUALS,
    SDL_SCANCODE_LSHIFT,
    SDL_SCANCODE_LCTRL,
    SDL_SCANCODE_LALT,
    SDL_SCANCODE_LGUI,
    SDL_SCANCODE_RSHIFT,
    SDL_SCANCODE_RCTRL,
    SDL_SCANCODE_RALT,
    SDL_SCANCODE_RGUI,
    SDL_SCANCODE_APPLICATION,
};

SDL_Window *SdlWindow(void *window)
{
    return static_cast<SDL_Window *>(window);
}

} // namespace

SdlInputBackend::SdlInputBackend(Window::WindowSystem &window) : window_(window.NativeHandle())
{
}

void SdlInputBackend::BeginFrame()
{
    pointer_delta_ = glm::vec2(0.0f);
    SDL_Window *window = SdlWindow(window_);
    if (window == nullptr || (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS) == 0)
    {
        has_pointer_sample_ = false;
        return;
    }

    float x = 0.0f;
    float y = 0.0f;
    if (pointer_captured_)
    {
        SDL_GetRelativeMouseState(&x, &y);
        pointer_delta_ = glm::vec2(x, y);
        return;
    }

    SDL_GetMouseState(&x, &y);
    const glm::vec2 current(x, y);
    if (has_pointer_sample_)
    {
        pointer_delta_ = current - previous_pointer_;
    }
    previous_pointer_ = current;
    has_pointer_sample_ = true;
}

bool SdlInputBackend::IsDown(Key key) const
{
    const auto index = static_cast<std::size_t>(key);
    if (index >= KeyScancodes.size())
    {
        return false;
    }
    const SDL_Scancode scancode = KeyScancodes[index];
    if (scancode == SDL_SCANCODE_UNKNOWN)
    {
        return false;
    }
    int count = 0;
    const bool *state = SDL_GetKeyboardState(&count);
    return state != nullptr && static_cast<int>(scancode) < count && state[scancode];
}

glm::vec2 SdlInputBackend::PointerDelta() const
{
    return pointer_delta_;
}

void SdlInputBackend::SetPointerCaptured(bool captured)
{
    SDL_Window *window = SdlWindow(window_);
    if (window == nullptr || pointer_captured_ == captured)
    {
        return;
    }
    if (SDL_SetWindowRelativeMouseMode(window, captured))
    {
        pointer_captured_ = captured;
        pointer_delta_ = glm::vec2(0.0f);
        has_pointer_sample_ = false;
    }
}

} // namespace CEngine::Input
