#ifndef CENGINE_INPUT_INPUT_BACKEND_H
#define CENGINE_INPUT_INPUT_BACKEND_H

#include <glm/vec2.hpp>

namespace CEngine::Input {

enum class Key {
    Escape,
    Tab,
    LeftShift,
    RightShift,
    W,
    A,
    S,
    D,
};

class IInputBackend {
public:
    virtual ~IInputBackend() = default;

    virtual void BeginFrame() = 0;
    virtual bool IsDown(Key key) const = 0;
    virtual glm::vec2 PointerDelta() const = 0;
    virtual void SetPointerCaptured(bool captured) = 0;
};

} // namespace CEngine::Input

#endif
