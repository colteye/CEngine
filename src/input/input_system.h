#ifndef CENGINE_INPUT_INPUT_SYSTEM_H
#define CENGINE_INPUT_INPUT_SYSTEM_H

#include "handle.h"
#include "input/input_backend.h"

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace CEngine::Input
{

struct ActionTag;
using ActionHandle = Handle<ActionTag>;

enum class Key
{
    Space,
    Apostrophe,
    Comma,
    Minus,
    Period,
    Slash,
    Digit0,
    Digit1,
    Digit2,
    Digit3,
    Digit4,
    Digit5,
    Digit6,
    Digit7,
    Digit8,
    Digit9,
    Semicolon,
    Equal,
    A,
    B,
    C,
    D,
    E,
    F,
    G,
    H,
    I,
    J,
    K,
    L,
    M,
    N,
    O,
    P,
    Q,
    R,
    S,
    T,
    U,
    V,
    W,
    X,
    Y,
    Z,
    LeftBracket,
    Backslash,
    RightBracket,
    GraveAccent,
    World1,
    World2,
    Escape,
    Enter,
    Tab,
    Backspace,
    Insert,
    Delete,
    Right,
    Left,
    Down,
    Up,
    PageUp,
    PageDown,
    Home,
    End,
    CapsLock,
    ScrollLock,
    NumLock,
    PrintScreen,
    Pause,
    F1,
    F2,
    F3,
    F4,
    F5,
    F6,
    F7,
    F8,
    F9,
    F10,
    F11,
    F12,
    F13,
    F14,
    F15,
    F16,
    F17,
    F18,
    F19,
    F20,
    F21,
    F22,
    F23,
    F24,
    F25,
    Keypad0,
    Keypad1,
    Keypad2,
    Keypad3,
    Keypad4,
    Keypad5,
    Keypad6,
    Keypad7,
    Keypad8,
    Keypad9,
    KeypadDecimal,
    KeypadDivide,
    KeypadMultiply,
    KeypadSubtract,
    KeypadAdd,
    KeypadEnter,
    KeypadEqual,
    LeftShift,
    LeftControl,
    LeftAlt,
    LeftSuper,
    RightShift,
    RightControl,
    RightAlt,
    RightSuper,
    Menu,
    Count,
};

enum class PointerAxis
{
    X,
    Y,
};

class InputSystem
{
  public:
    explicit InputSystem(std::unique_ptr<IInputBackend> backend = {}) : backend_(std::move(backend))
    {
    }

    void BeginFrame()
    {
        if (backend_ != nullptr)
        {
            backend_->BeginFrame();
        }
        std::fill(values_.begin(), values_.end(), 0.0f);
        if (backend_ == nullptr)
        {
            return;
        }
        for (const KeyBinding &binding : key_bindings_)
        {
            if (backend_->IsDown(binding.key))
            {
                values_[binding.action.Index()] += binding.scale;
            }
        }
        const glm::vec2 pointer_delta = backend_->PointerDelta();
        for (const PointerBinding &binding : pointer_bindings_)
        {
            values_[binding.action.Index()] +=
                (binding.axis == PointerAxis::X ? pointer_delta.x : pointer_delta.y) * binding.scale;
        }
    }
    [[nodiscard]] bool IsDown(Key key) const
    {
        return backend_ != nullptr && backend_->IsDown(key);
    }
    [[nodiscard]] glm::vec2 PointerDelta() const
    {
        return backend_ != nullptr ? backend_->PointerDelta() : glm::vec2(0.0f);
    }
    void SetPointerCaptured(bool captured)
    {
        if (backend_ != nullptr)
        {
            backend_->SetPointerCaptured(captured);
        }
    }
    ActionHandle RegisterAction(std::string name);
    bool BindKey(ActionHandle action, Key key, float scale = 1.0f);
    bool BindPointerAxis(ActionHandle action, PointerAxis axis, float scale = 1.0f);
    void Set(ActionHandle action, float value);
    [[nodiscard]] float Value(ActionHandle action) const;

  private:
    struct KeyBinding
    {
        ActionHandle action;
        Key key{};
        float scale{};
    };
    struct PointerBinding
    {
        ActionHandle action;
        PointerAxis axis{};
        float scale{};
    };

    std::unique_ptr<IInputBackend> backend_;
    std::unordered_map<std::string, ActionHandle> actions_by_name_;
    std::vector<float> values_;
    std::vector<KeyBinding> key_bindings_;
    std::vector<PointerBinding> pointer_bindings_;
};

} // namespace CEngine::Input

#endif
