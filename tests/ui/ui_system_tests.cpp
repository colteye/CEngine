// Copyright (c) CEngine contributors.

#include "input/input_system.h"
#include "ui/ui_system.h"
#include "window/window_system.h"

#include <filesystem>
#include <iostream>
#include <memory>
#include <utility>
#include <vector>

namespace
{

bool Expect(bool condition, const char *message)
{
    if (!condition)
        std::cerr << message << '\n';
    return condition;
}

class FakeWindowBackend final : public CEngine::Window::IWindowBackend
{
  public:
    bool Initialize(const CEngine::Window::WindowDesc &desc) override
    {
        size = {desc.width, desc.height};
        active = true;
        return true;
    }
    void Shutdown() override
    {
        active = false;
    }
    void PollEvents(CEngine::Window::EventCallback, void *) override {}
    void WaitEvents(int, CEngine::Window::EventCallback, void *) override {}
    [[nodiscard]] bool ShouldClose() const override { return false; }
    void RequestClose() override {}
    [[nodiscard]] glm::ivec2 Size() const override { return size; }
    [[nodiscard]] glm::ivec2 DrawableSize() const override { return size; }
    void SwapBuffers() override {}
    [[nodiscard]] double TimeSeconds() const override { return 1.25; }
    [[nodiscard]] void *NativeHandle() const override { return active ? const_cast<FakeWindowBackend *>(this) : nullptr; }
    [[nodiscard]] void *NativeGraphicsContext() const override { return nullptr; }

    glm::ivec2 size{320, 180};
    bool active = false;
};

class FakeInputBackend final : public CEngine::Input::IInputBackend
{
  public:
    void BeginFrame() override {}
    [[nodiscard]] std::span<const CEngine::Input::InputEvent> Events() const override
    {
        return events;
    }
    [[nodiscard]] bool IsDown(CEngine::Input::Key) const override { return false; }
    [[nodiscard]] glm::vec2 PointerDelta() const override { return {}; }
    [[nodiscard]] glm::vec2 PointerPosition() const override { return pointer; }
    void SetPointerCaptured(bool) override {}

    glm::vec2 pointer{40.0f, 40.0f};
    std::vector<CEngine::Input::InputEvent> events;
};

} // namespace

int main(int argc, char **argv)
{
    if (!Expect(argc == 2, "UI test requires the content directory"))
        return 1;

    auto window_backend = std::make_unique<FakeWindowBackend>();
    CEngine::Window::WindowSystem window(std::move(window_backend));
    CEngine::Window::WindowDesc window_desc;
    window_desc.width = 320;
    window_desc.height = 180;
    if (!Expect(window.Initialize(window_desc), "fake window should initialize"))
        return 1;

    auto input_backend = std::make_unique<FakeInputBackend>();
    FakeInputBackend *fake_input = input_backend.get();
    CEngine::Input::InputSystem input(std::move(input_backend));
    CEngine::UI::UISystem ui;
    if (!Expect(ui.Initialize(window, std::filesystem::path(argv[1])), "UI should initialize") ||
        !Expect(ui.Initialized(), "UI should report initialized"))
        return 1;

    const CEngine::UI::UiScreenHandle screen = ui.LoadScreen("click.rml");
    if (!Expect(static_cast<bool>(screen), "RML screen should load") ||
        !Expect(ui.BindClick(screen, "start", "start_game"), "button action should bind") ||
        !Expect(ui.Show(screen, true), "screen should show"))
        return 1;

    input.BeginFrame();
    ui.Update(input);
    const CEngine::Renderer::UiFrame first_frame = ui.Compose();
    if (!Expect(first_frame.width == 320 && first_frame.height == 180,
                "UI frame should use logical window dimensions") ||
        !Expect(!first_frame.Empty(), "visible screen should produce geometry"))
        return 1;

    CEngine::Input::InputEvent move;
    move.type = CEngine::Input::InputEventType::PointerMove;
    move.position = fake_input->pointer;
    CEngine::Input::InputEvent down;
    down.type = CEngine::Input::InputEventType::PointerButtonDown;
    down.button = CEngine::Input::PointerButton::Primary;
    down.position = fake_input->pointer;
    CEngine::Input::InputEvent up = down;
    up.type = CEngine::Input::InputEventType::PointerButtonUp;
    fake_input->events = {move, down, up};
    input.BeginFrame();
    ui.Update(input);

    const std::vector<CEngine::UI::UiEvent> events = ui.DrainEvents();
    if (!Expect(events.size() == 1 && events[0].screen == screen &&
                    events[0].action == "start_game",
                "click should emit the bound semantic action") ||
        !Expect(ui.UnloadScreen(screen), "loaded screen should unload") ||
        !Expect(!ui.Show(screen), "unloaded screen handle should become stale"))
        return 1;

    ui.Shutdown();
    return Expect(!ui.Initialized(), "UI shutdown should be symmetric") ? 0 : 1;
}
