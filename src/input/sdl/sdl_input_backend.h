// Copyright (c) CEngine contributors.

#ifndef CENGINE_INPUT_SDL_INPUT_BACKEND_H
#define CENGINE_INPUT_SDL_INPUT_BACKEND_H

#include "input/input_backend.h"

namespace CEngine::Window
{
class WindowSystem;
}

namespace CEngine::Input
{

class SdlInputBackend final : public IInputBackend
{
  public:
    explicit SdlInputBackend(Window::WindowSystem &window);

    void BeginFrame() override;
    [[nodiscard]] bool IsDown(Key key) const override;
    [[nodiscard]] glm::vec2 PointerDelta() const override;
    void SetPointerCaptured(bool captured) override;

  private:
    void *window_ = nullptr;
    glm::vec2 pointer_delta_ = glm::vec2(0.0f);
    glm::vec2 previous_pointer_ = glm::vec2(0.0f);
    bool has_pointer_sample_ = false;
    bool pointer_captured_ = false;
};

} // namespace CEngine::Input

#endif
