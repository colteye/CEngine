//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ \
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/input/glfw/glfw_input_backend.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#ifndef CENGINE_INPUT_GLFW_INPUT_BACKEND_H
#define CENGINE_INPUT_GLFW_INPUT_BACKEND_H

#include "input/input_backend.h"

struct GLFWwindow;

namespace CEngine::Input
{

/**
 * @brief TODO: Describe GlfwInputBackend.
 */
class GlfwInputBackend final : public IInputBackend
{
  public:
    /**
     * @brief TODO: Describe GlfwInputBackend.
     *
     * @param window TODO: Describe this parameter.
     */
    explicit GlfwInputBackend(GLFWwindow *window);

    /**
     * @brief TODO: Describe BeginFrame.
     */
    void BeginFrame() override;
    /**
     * @brief TODO: Describe IsDown.
     *
     * @param key TODO: Describe this parameter.
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] bool IsDown(Key key) const override;
    /**
     * @brief TODO: Describe PointerDelta.
     *
     * @return TODO: Describe the return value.
     */
    [[nodiscard]] glm::vec2 PointerDelta() const override
    {
        return pointer_delta_;
    }
    /**
     * @brief TODO: Describe SetPointerCaptured.
     *
     * @param captured TODO: Describe this parameter.
     */
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
