//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/window/sdl/sdl_window_backend.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#ifndef CENGINE_WINDOW_SDL_WINDOW_BACKEND_H
#define CENGINE_WINDOW_SDL_WINDOW_BACKEND_H

#include "window/window_backend.h"

struct SDL_Window;

namespace CEngine::Window
{

class SdlWindowBackend final : public IWindowBackend
{
  public:
    bool Initialize(const WindowDesc &desc) override;
    void Shutdown() override;
    void PollEvents(EventCallback callback, void *user_data) override;
    void WaitEvents(int timeout_milliseconds, EventCallback callback, void *user_data) override;
    [[nodiscard]] bool ShouldClose() const override;
    void RequestClose() override;
    [[nodiscard]] glm::ivec2 Size() const override;
    [[nodiscard]] glm::ivec2 DrawableSize() const override;
    void SwapBuffers() override;
    [[nodiscard]] double TimeSeconds() const override;
    [[nodiscard]] void *NativeHandle() const override;
    [[nodiscard]] void *NativeGraphicsContext() const override;

  private:
    void HandleEvent(const void *event, EventCallback callback, void *user_data);

    SDL_Window *window_ = nullptr;
    void *graphics_context_ = nullptr;
    GraphicsApi graphics_api_ = GraphicsApi::OpenGL;
    bool should_close_ = false;
    bool owns_video_subsystem_ = false;
};

} // namespace CEngine::Window

#endif
