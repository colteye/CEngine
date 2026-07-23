// Copyright (c) CEngine contributors.

#include "window/window_system.h"

#include <iostream>
#include <memory>

namespace
{

class FakeWindowBackend final : public CEngine::Window::IWindowBackend
{
  public:
    bool Initialize(const CEngine::Window::WindowDesc &desc) override
    {
        initialized = true;
        size = {desc.width, desc.height};
        return desc.width > 0 && desc.height > 0;
    }
    void Shutdown() override
    {
        initialized = false;
    }
    void PollEvents(CEngine::Window::EventCallback callback, void *user_data) override
    {
        ++polls;
        if (callback != nullptr)
        {
            callback(&event, user_data);
        }
    }
    void WaitEvents(int, CEngine::Window::EventCallback callback, void *user_data) override
    {
        ++waits;
        if (callback != nullptr)
        {
            callback(&event, user_data);
        }
    }
    [[nodiscard]] bool ShouldClose() const override
    {
        return should_close;
    }
    void RequestClose() override
    {
        should_close = true;
    }
    [[nodiscard]] glm::ivec2 Size() const override
    {
        return size;
    }
    [[nodiscard]] glm::ivec2 DrawableSize() const override
    {
        return size * 2;
    }
    void SwapBuffers() override
    {
        ++swaps;
    }
    [[nodiscard]] double TimeSeconds() const override
    {
        return 12.5;
    }
    [[nodiscard]] void *NativeHandle() const override
    {
        return const_cast<int *>(&event);
    }
    [[nodiscard]] void *NativeGraphicsContext() const override
    {
        return nullptr;
    }

    glm::ivec2 size{0};
    int event = 7;
    int polls = 0;
    int waits = 0;
    int swaps = 0;
    bool initialized = false;
    bool should_close = false;
};

bool Expect(bool condition, const char *message)
{
    if (!condition)
    {
        std::cerr << message << '\n';
    }
    return condition;
}

void CountEvent(const void *event, void *user_data)
{
    if (event != nullptr)
    {
        ++*static_cast<int *>(user_data);
    }
}

} // namespace

int main()
{
    auto backend = std::make_unique<FakeWindowBackend>();
    FakeWindowBackend *fake = backend.get();
    CEngine::Window::WindowSystem window(std::move(backend));
    CEngine::Window::WindowDesc desc;
    desc.width = 640;
    desc.height = 360;
    if (!Expect(window.Initialize(desc), "injected window backend should initialize"))
    {
        return 1;
    }

    int events = 0;
    window.PollEvents(&CountEvent, &events);
    window.WaitEvents(5, &CountEvent, &events);
    window.SwapBuffers();
    const bool facade_ok =
        Expect(window.Size() == glm::ivec2(640, 360), "logical size should pass through") &&
        Expect(window.DrawableSize() == glm::ivec2(1280, 720), "drawable size should pass through") &&
        Expect(window.TimeSeconds() == 12.5, "monotonic time should pass through") &&
        Expect(window.NativeHandle() != nullptr, "opaque integration handle should pass through") &&
        Expect(events == 2 && fake->polls == 1 && fake->waits == 1 && fake->swaps == 1,
               "event and presentation calls should pass through");

    window.RequestClose();
    const bool close_ok = Expect(window.ShouldClose(), "close state should pass through");
    window.Shutdown();
    return facade_ok && close_ok && Expect(!fake->initialized, "shutdown should be symmetric") &&
                   Expect(window.ShouldClose() && window.NativeHandle() == nullptr,
                          "inactive facades should not expose retained backend state")
               ? 0
               : 1;
}
