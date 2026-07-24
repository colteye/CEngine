//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/input/sdl/sdl_input_backend.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#ifndef CENGINE_INPUT_SDL_INPUT_BACKEND_H
#define CENGINE_INPUT_SDL_INPUT_BACKEND_H

#include "input/input_system.h"

#include <vector>

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
    void ProcessPlatformEvent(const void *event) override;
    [[nodiscard]] std::span<const InputEvent> Events() const override;
    [[nodiscard]] bool IsDown(Key key) const override;
    [[nodiscard]] glm::vec2 PointerDelta() const override;
    [[nodiscard]] glm::vec2 PointerPosition() const override;
    [[nodiscard]] bool IsPointerDown(PointerButton button) const override;
    void SetPointerCaptured(bool captured) override;

  private:
    void *window_ = nullptr;
    glm::vec2 pointer_delta_ = glm::vec2(0.0f);
    glm::vec2 pointer_position_ = glm::vec2(0.0f);
    glm::vec2 previous_pointer_ = glm::vec2(0.0f);
    std::vector<InputEvent> pending_events_;
    std::vector<InputEvent> events_;
    bool has_pointer_sample_ = false;
    bool pointer_captured_ = false;
};

} // namespace CEngine::Input

#endif
