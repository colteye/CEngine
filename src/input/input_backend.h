#ifndef CENGINE_INPUT_INPUT_BACKEND_H
#define CENGINE_INPUT_INPUT_BACKEND_H

#include <glm/vec2.hpp>

namespace CEngine::Input
{

enum class Key;

class IInputBackend
{
  public:
    IInputBackend() = default;
    virtual ~IInputBackend() = default;
    IInputBackend(const IInputBackend &) = delete;
    IInputBackend &operator=(const IInputBackend &) = delete;
    IInputBackend(IInputBackend &&) = delete;
    IInputBackend &operator=(IInputBackend &&) = delete;

    virtual void BeginFrame() = 0;
    [[nodiscard]] virtual bool IsDown(Key key) const = 0;
    [[nodiscard]] virtual glm::vec2 PointerDelta() const = 0;
    virtual void SetPointerCaptured(bool captured) = 0;
};

} // namespace CEngine::Input

#endif
