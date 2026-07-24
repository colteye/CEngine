//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/window/window_system.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#ifndef CENGINE_WINDOW_WINDOW_SYSTEM_H
#define CENGINE_WINDOW_WINDOW_SYSTEM_H

#include "window/window_backend.h"

#include <memory>

namespace CEngine::Window
{

class WindowSystem
{
  public:
    explicit WindowSystem(std::unique_ptr<IWindowBackend> backend = {});
    ~WindowSystem();

    WindowSystem(const WindowSystem &) = delete;
    WindowSystem &operator=(const WindowSystem &) = delete;
    WindowSystem(WindowSystem &&) = delete;
    WindowSystem &operator=(WindowSystem &&) = delete;

    bool Initialize(const WindowDesc &desc = {});
    void Shutdown();
    void PollEvents(EventCallback callback = nullptr, void *user_data = nullptr);
    void WaitEvents(int timeout_milliseconds, EventCallback callback = nullptr, void *user_data = nullptr);
    [[nodiscard]] bool ShouldClose() const;
    void RequestClose();
    [[nodiscard]] glm::ivec2 Size() const;
    [[nodiscard]] glm::ivec2 DrawableSize() const;
    void SwapBuffers();
    [[nodiscard]] double TimeSeconds() const;

    // Opaque access is reserved for compiled integration backends such as
    // renderer surface creation and ImGui's SDL platform adapter.
    [[nodiscard]] void *NativeHandle() const;
    [[nodiscard]] void *NativeGraphicsContext() const;

  private:
    std::unique_ptr<IWindowBackend> backend_;
    bool initialized_ = false;
};

} // namespace CEngine::Window

#endif
