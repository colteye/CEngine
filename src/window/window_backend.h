// Copyright (c) CEngine contributors.

#ifndef CENGINE_WINDOW_WINDOW_BACKEND_H
#define CENGINE_WINDOW_WINDOW_BACKEND_H

#include <glm/vec2.hpp>

#include <string>

namespace CEngine::Window
{

enum class GraphicsApi
{
    OpenGL,
    Vulkan,
};

struct WindowDesc
{
    std::string title = "CEngine";
    int width = 1024;
    int height = 768;
    GraphicsApi graphics_api =
#ifdef CENGINE_ENABLE_VULKAN
        GraphicsApi::Vulkan;
#else
        GraphicsApi::OpenGL;
#endif
    bool resizable = true;
    bool high_pixel_density = true;
    bool hidden = false;
    bool vertical_sync = true;
};

using EventCallback = void (*)(const void *event, void *user_data);

class IWindowBackend
{
  public:
    IWindowBackend() = default;
    virtual ~IWindowBackend() = default;
    IWindowBackend(const IWindowBackend &) = delete;
    IWindowBackend &operator=(const IWindowBackend &) = delete;
    IWindowBackend(IWindowBackend &&) = delete;
    IWindowBackend &operator=(IWindowBackend &&) = delete;

    virtual bool Initialize(const WindowDesc &desc) = 0;
    virtual void Shutdown() = 0;
    virtual void PollEvents(EventCallback callback, void *user_data) = 0;
    virtual void WaitEvents(int timeout_milliseconds, EventCallback callback, void *user_data) = 0;
    [[nodiscard]] virtual bool ShouldClose() const = 0;
    virtual void RequestClose() = 0;
    [[nodiscard]] virtual glm::ivec2 Size() const = 0;
    [[nodiscard]] virtual glm::ivec2 DrawableSize() const = 0;
    virtual void SwapBuffers() = 0;
    [[nodiscard]] virtual double TimeSeconds() const = 0;
    [[nodiscard]] virtual void *NativeHandle() const = 0;
    [[nodiscard]] virtual void *NativeGraphicsContext() const = 0;
};

} // namespace CEngine::Window

#endif
