#include "input/glfw/glfw_input_backend.h"

#include <GLFW/glfw3.h>

namespace CEngine::Input {
namespace {

int GlfwKey(Key key)
{
    switch (key)
    {
    case Key::Escape: return GLFW_KEY_ESCAPE;
    case Key::Tab: return GLFW_KEY_TAB;
    case Key::LeftShift: return GLFW_KEY_LEFT_SHIFT;
    case Key::RightShift: return GLFW_KEY_RIGHT_SHIFT;
    case Key::W: return GLFW_KEY_W;
    case Key::A: return GLFW_KEY_A;
    case Key::S: return GLFW_KEY_S;
    case Key::D: return GLFW_KEY_D;
    }
    return GLFW_KEY_UNKNOWN;
}

} // namespace

GlfwInputBackend::GlfwInputBackend(GLFWwindow* window) : window_(window) {}

void GlfwInputBackend::BeginFrame()
{
    pointer_delta_ = glm::vec2(0.0f);
    if (window_ == nullptr) return;

    const bool focused =
        glfwGetWindowAttrib(window_, GLFW_FOCUSED) == GLFW_TRUE;
    double cursor_x = 0.0;
    double cursor_y = 0.0;
    glfwGetCursorPos(window_, &cursor_x, &cursor_y);
    if (focused && was_focused_ && has_cursor_sample_)
        pointer_delta_ = {
            static_cast<float>(cursor_x - previous_cursor_x_),
            static_cast<float>(cursor_y - previous_cursor_y_)
        };
    previous_cursor_x_ = cursor_x;
    previous_cursor_y_ = cursor_y;
    has_cursor_sample_ = focused;
    was_focused_ = focused;
}

bool GlfwInputBackend::IsDown(Key key) const
{
    return window_ != nullptr &&
        glfwGetKey(window_, GlfwKey(key)) == GLFW_PRESS;
}

void GlfwInputBackend::SetPointerCaptured(bool captured)
{
    if (window_ == nullptr || pointer_captured_ == captured) return;
    pointer_captured_ = captured;
    glfwSetInputMode(window_, GLFW_CURSOR,
        captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    if (glfwRawMouseMotionSupported() == GLFW_TRUE)
        glfwSetInputMode(window_, GLFW_RAW_MOUSE_MOTION,
            captured ? GLFW_TRUE : GLFW_FALSE);
    has_cursor_sample_ = false;
    was_focused_ = false;
    pointer_delta_ = glm::vec2(0.0f);
}

} // namespace CEngine::Input
