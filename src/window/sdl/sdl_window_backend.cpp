// Copyright (c) CEngine contributors.

#include "window/sdl/sdl_window_backend.h"

#ifdef CENGINE_ENABLE_OPENGL
#include <glad/glad.h>
#endif
#include <SDL3/SDL.h>

#include <algorithm>
#include <iostream>

namespace CEngine::Window
{

bool SdlWindowBackend::Initialize(const WindowDesc &desc)
{
    Shutdown();
    if (!SDL_WasInit(SDL_INIT_VIDEO))
    {
        if (!SDL_InitSubSystem(SDL_INIT_VIDEO))
        {
            std::cerr << "Failed to initialize SDL3 video: " << SDL_GetError() << '\n';
            return false;
        }
        owns_video_subsystem_ = true;
    }

    graphics_api_ = desc.graphics_api;
    SDL_WindowFlags flags = 0;
    flags |= graphics_api_ == GraphicsApi::OpenGL ? SDL_WINDOW_OPENGL : SDL_WINDOW_VULKAN;
    if (desc.resizable)
    {
        flags |= SDL_WINDOW_RESIZABLE;
    }
    if (desc.high_pixel_density)
    {
        flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
    }
    if (desc.hidden)
    {
        flags |= SDL_WINDOW_HIDDEN;
    }

    if (graphics_api_ == GraphicsApi::OpenGL)
    {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#ifdef __APPLE__
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
#endif
    }

    window_ = SDL_CreateWindow(desc.title.c_str(), std::max(desc.width, 1), std::max(desc.height, 1), flags);
    if (window_ == nullptr)
    {
        std::cerr << "Failed to create the SDL3 window: " << SDL_GetError() << '\n';
        Shutdown();
        return false;
    }

    if (graphics_api_ == GraphicsApi::OpenGL)
    {
        graphics_context_ = SDL_GL_CreateContext(window_);
        if (graphics_context_ == nullptr ||
            !SDL_GL_MakeCurrent(window_, reinterpret_cast<SDL_GLContext>(graphics_context_)))
        {
            std::cerr << "Failed to create the SDL3 OpenGL context: " << SDL_GetError() << '\n';
            Shutdown();
            return false;
        }
        SDL_GL_SetSwapInterval(desc.vertical_sync ? 1 : 0);
#ifdef CENGINE_ENABLE_OPENGL
        if (gladLoadGLLoader(reinterpret_cast<GLADloadproc>(SDL_GL_GetProcAddress)) == 0)
        {
            std::cerr << "Failed to load OpenGL through SDL3.\n";
            Shutdown();
            return false;
        }
#endif
    }

    should_close_ = false;
    return true;
}

void SdlWindowBackend::Shutdown()
{
    if (graphics_context_ != nullptr)
    {
        SDL_GL_DestroyContext(reinterpret_cast<SDL_GLContext>(graphics_context_));
        graphics_context_ = nullptr;
    }
    if (window_ != nullptr)
    {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    if (owns_video_subsystem_)
    {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        owns_video_subsystem_ = false;
    }
    should_close_ = false;
}

void SdlWindowBackend::HandleEvent(const void *opaque_event, EventCallback callback, void *user_data)
{
    const auto &event = *static_cast<const SDL_Event *>(opaque_event);
    if (event.type == SDL_EVENT_QUIT ||
        (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window_)))
    {
        should_close_ = true;
    }
    if (callback != nullptr)
    {
        callback(&event, user_data);
    }
}

void SdlWindowBackend::PollEvents(EventCallback callback, void *user_data)
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        HandleEvent(&event, callback, user_data);
    }
}

void SdlWindowBackend::WaitEvents(int timeout_milliseconds, EventCallback callback, void *user_data)
{
    SDL_Event event;
    if (SDL_WaitEventTimeout(&event, std::max(timeout_milliseconds, 0)))
    {
        HandleEvent(&event, callback, user_data);
    }
    PollEvents(callback, user_data);
}

bool SdlWindowBackend::ShouldClose() const
{
    return should_close_;
}

void SdlWindowBackend::RequestClose()
{
    should_close_ = true;
}

glm::ivec2 SdlWindowBackend::Size() const
{
    glm::ivec2 size(0);
    if (window_ != nullptr)
    {
        SDL_GetWindowSize(window_, &size.x, &size.y);
    }
    return size;
}

glm::ivec2 SdlWindowBackend::DrawableSize() const
{
    glm::ivec2 size(0);
    if (window_ != nullptr)
    {
        SDL_GetWindowSizeInPixels(window_, &size.x, &size.y);
    }
    return size;
}

void SdlWindowBackend::SwapBuffers()
{
    if (window_ != nullptr && graphics_api_ == GraphicsApi::OpenGL)
    {
        SDL_GL_SwapWindow(window_);
    }
}

double SdlWindowBackend::TimeSeconds() const
{
    return static_cast<double>(SDL_GetTicksNS()) / 1'000'000'000.0;
}

void *SdlWindowBackend::NativeHandle() const
{
    return window_;
}

void *SdlWindowBackend::NativeGraphicsContext() const
{
    return graphics_context_;
}

} // namespace CEngine::Window
