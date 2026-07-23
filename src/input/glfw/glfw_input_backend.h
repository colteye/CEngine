#ifndef CENGINE_INPUT_GLFW_INPUT_BACKEND_H
#define CENGINE_INPUT_GLFW_INPUT_BACKEND_H

#include "input/input_backend.h"

struct GLFWwindow;

namespace CEngine::Input
{

class GlfwInputBackend final : public IInputBackend
{
  public:
    explicit GlfwInputBackend(GLFWwindow *window);

    void BeginFrame() override;
    [[nodiscard]] bool IsDown(Key key) const override;
    [[nodiscard]] glm::vec2 PointerDelta() const override
    {
        return pointer_delta_;
    }
    void SetPointerCaptured(bool captured) override;

  private:
    GLFWwindow *window_ = nullptr;
    glm::vec2 pointer_delta_ = glm::vec2(0.0f);
    double previous_cursor_x_ = 0.0;
    double previous_cursor_y_ = 0.0;
    bool has_cursor_sample_ = false;
    bool was_focused_ = false;
    bool pointer_captured_ = false;
};

} // namespace CEngine::Input

#endif
