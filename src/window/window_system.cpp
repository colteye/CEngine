//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/window/window_system.cpp
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#include "window/window_system.h"

#include "window/sdl/sdl_window_backend.h"

namespace CEngine::Window
{

WindowSystem::WindowSystem(std::unique_ptr<IWindowBackend> backend) : backend_(std::move(backend))
{
}

WindowSystem::~WindowSystem()
{
    Shutdown();
}

bool WindowSystem::Initialize(const WindowDesc &desc)
{
    Shutdown();
    if (backend_ == nullptr)
    {
        backend_ = std::make_unique<SdlWindowBackend>();
    }
    if (!backend_->Initialize(desc))
    {
        return false;
    }
    initialized_ = true;
    return true;
}

void WindowSystem::Shutdown()
{
    if (initialized_ && backend_ != nullptr)
    {
        backend_->Shutdown();
        initialized_ = false;
    }
}

void WindowSystem::PollEvents(EventCallback callback, void *user_data)
{
    if (initialized_ && backend_ != nullptr)
    {
        backend_->PollEvents(callback, user_data);
    }
}

void WindowSystem::WaitEvents(int timeout_milliseconds, EventCallback callback, void *user_data)
{
    if (initialized_ && backend_ != nullptr)
    {
        backend_->WaitEvents(timeout_milliseconds, callback, user_data);
    }
}

bool WindowSystem::ShouldClose() const
{
    return !initialized_ || backend_ == nullptr || backend_->ShouldClose();
}

void WindowSystem::RequestClose()
{
    if (initialized_ && backend_ != nullptr)
    {
        backend_->RequestClose();
    }
}

glm::ivec2 WindowSystem::Size() const
{
    return initialized_ && backend_ != nullptr ? backend_->Size() : glm::ivec2(0);
}

glm::ivec2 WindowSystem::DrawableSize() const
{
    return initialized_ && backend_ != nullptr ? backend_->DrawableSize() : glm::ivec2(0);
}

void WindowSystem::SwapBuffers()
{
    if (initialized_ && backend_ != nullptr)
    {
        backend_->SwapBuffers();
    }
}

double WindowSystem::TimeSeconds() const
{
    return initialized_ && backend_ != nullptr ? backend_->TimeSeconds() : 0.0;
}

void *WindowSystem::NativeHandle() const
{
    return initialized_ && backend_ != nullptr ? backend_->NativeHandle() : nullptr;
}

void *WindowSystem::NativeGraphicsContext() const
{
    return initialized_ && backend_ != nullptr ? backend_->NativeGraphicsContext() : nullptr;
}

} // namespace CEngine::Window
