//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file tests/ui/ui_system_tests.cpp
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

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
    void PollEvents(CEngine::Window::EventCallback, void *) override
    {
    }
    void WaitEvents(int, CEngine::Window::EventCallback, void *) override
    {
    }
    [[nodiscard]] bool ShouldClose() const override
    {
        return false;
    }
    void RequestClose() override
    {
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
    }
    [[nodiscard]] double TimeSeconds() const override
    {
        return 1.25;
    }
    [[nodiscard]] void *NativeHandle() const override
    {
        return active ? const_cast<FakeWindowBackend *>(this) : nullptr;
    }
    [[nodiscard]] void *NativeGraphicsContext() const override
    {
        return nullptr;
    }

    glm::ivec2 size{320, 180};
    bool active = false;
};

class FakeInputBackend final : public CEngine::Input::IInputBackend
{
  public:
    void BeginFrame() override
    {
    }
    [[nodiscard]] std::span<const CEngine::Input::InputEvent> Events() const override
    {
        return events;
    }
    [[nodiscard]] bool IsDown(CEngine::Input::Key) const override
    {
        return false;
    }
    [[nodiscard]] glm::vec2 PointerDelta() const override
    {
        return {};
    }
    [[nodiscard]] glm::vec2 PointerPosition() const override
    {
        return pointer;
    }
    void SetPointerCaptured(bool) override
    {
    }

    glm::vec2 pointer{40.0f, 40.0f};
    std::vector<CEngine::Input::InputEvent> events;
};

} // namespace

int main(int argc, char **argv)
{
    if (!Expect(argc == 2, "UI test requires the project content directory"))
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
        !Expect(ui.Initialized(), "UI should report initialized") ||
        !Expect(ui.LoadFont("samples/viewer/ui/fonts/LatoLatin-Regular.ttf"),
                "test font should load"))
        return 1;

    if (!Expect(!ui.LoadScreen("tests/ui/data/click.rml"), "legacy RML screens should be rejected"))
        return 1;

    const CEngine::UI::UiScreenHandle screen = ui.LoadScreen("tests/ui/data/click.html");
    if (!Expect(static_cast<bool>(screen), "HTML screen should load") ||
        !Expect(ui.BindClick(screen, "start", "start_game"), "button action should bind") ||
        !Expect(ui.SetText(screen, "status", "Ready"), "text content should update") ||
        !Expect(ui.SetValue(screen, "amount", 7.0f), "range value should update") ||
        !Expect(ui.BindChange(screen, "amount", "set_amount"), "range action should bind") ||
        !Expect(ui.BindChange(screen, "toggle", "set_toggle"), "checkbox action should bind") ||
        !Expect(ui.Show(screen, true), "screen should show"))
        return 1;

    input.BeginFrame();
    ui.Update(input);
    const CEngine::Renderer::UiFrame first_frame = ui.Compose();
    if (!Expect(first_frame.width == 640 && first_frame.height == 360,
                "UI frame should use full drawable pixel dimensions") ||
        !Expect(!first_frame.Empty(), "visible screen should produce geometry"))
        return 1;

    CEngine::Input::InputEvent down;
    down.type = CEngine::Input::InputEventType::PointerButtonDown;
    down.button = CEngine::Input::PointerButton::Primary;
    down.position = {40.0f, 40.0f};
    CEngine::Input::InputEvent up = down;
    up.type = CEngine::Input::InputEventType::PointerButtonUp;

    down.position = {15.0f, 30.0f};
    up.position = down.position;
    fake_input->events = {down, up};
    input.BeginFrame();
    ui.Update(input);
    if (!Expect(ui.DrainEvents().empty(),
                "CSS px should scale as logical pixels on a high-DPI drawable"))
        return 1;

    down.position = {40.0f, 40.0f};
    up.position = down.position;
    fake_input->pointer = {300.0f, 150.0f};
    fake_input->events = {down};
    input.BeginFrame();
    ui.Update(input);
    if (!Expect(ui.DrainEvents().empty(), "pointer down should begin a hold without clicking"))
        return 1;

    fake_input->events.clear();
    input.BeginFrame();
    ui.Update(input);
    if (!Expect(ui.DrainEvents().empty(), "held pointer state should persist across an event-free frame"))
        return 1;

    fake_input->events = {up};
    input.BeginFrame();
    ui.Update(input);
    const std::vector<CEngine::UI::UiEvent> events = ui.DrainEvents();
    if (!Expect(events.size() == 1 && events[0].screen == screen && events[0].action == "start_game",
                "release inside after a held press should emit the bound semantic action"))
        return 1;

    down.position = {40.0f, 40.0f};
    up.position = {250.0f, 140.0f};
    fake_input->events = {down, up};
    input.BeginFrame();
    ui.Update(input);
    if (!Expect(ui.DrainEvents().empty(), "release outside the pressed element must not click"))
        return 1;

    down.position = {250.0f, 140.0f};
    up.position = {40.0f, 40.0f};
    fake_input->events = {down, up};
    input.BeginFrame();
    ui.Update(input);
    if (!Expect(ui.DrainEvents().empty(), "press outside then release inside must not click"))
        return 1;

    if (!Expect(ui.SetValue(screen, "amount", 9.0f), "bound range should accept a new value"))
        return 1;
    const std::vector<CEngine::UI::UiEvent> range_events = ui.DrainEvents();
    if (!Expect(range_events.size() == 1 && range_events[0].action == "set_amount" &&
                    !range_events[0].value.empty(),
                "range changes should carry their value"))
        return 1;

    if (!Expect(ui.SetChecked(screen, "toggle", true), "bound checkbox should accept checked state"))
        return 1;
    const std::vector<CEngine::UI::UiEvent> toggle_events = ui.DrainEvents();
    if (!Expect(toggle_events.size() == 1 && toggle_events[0].action == "set_toggle" &&
                    toggle_events[0].checked,
                "checkbox changes should carry their checked state") ||
        !Expect(ui.UnloadScreen(screen), "loaded screen should unload") ||
        !Expect(!ui.Show(screen), "unloaded screen handle should become stale"))
        return 1;

    ui.Shutdown();
    if (!Expect(!ui.Initialized(), "UI shutdown should be symmetric") ||
        !Expect(ui.Initialize(window, std::filesystem::path(argv[1])),
                "UI should initialize with viewer content") ||
        !Expect(ui.LoadFont("samples/viewer/ui/fonts/LatoLatin-Regular.ttf"),
                "viewer regular font should load") ||
        !Expect(ui.LoadFont("samples/viewer/ui/fonts/LatoLatin-Bold.ttf"),
                "viewer bold font should load"))
        return 1;

    const CEngine::UI::UiScreenHandle start_menu = ui.LoadScreen("samples/viewer/ui/start_menu.html");
    const CEngine::UI::UiScreenHandle hud = ui.LoadScreen("samples/viewer/ui/hud.html");
    const CEngine::UI::UiScreenHandle tuning = ui.LoadScreen("samples/viewer/ui/tuning.html");
    if (!Expect(static_cast<bool>(start_menu), "viewer start menu HTML should load") ||
        !Expect(static_cast<bool>(hud), "viewer HUD HTML should load") ||
        !Expect(static_cast<bool>(tuning), "viewer tuning HTML should load") ||
        !Expect(ui.SetText(hud, "fps-value", "60"), "viewer HUD text should update") ||
        !Expect(ui.Show(hud), "viewer HUD should show") ||
        !Expect(ui.Show(tuning), "viewer tuning panel should show") ||
        !Expect(ui.Show(start_menu, true), "viewer start menu should show modally"))
        return 1;

    fake_input->events.clear();
    input.BeginFrame();
    ui.Update(input);
    const CEngine::Renderer::UiFrame viewer_frame = ui.Compose();
    if (!Expect(viewer_frame.width == 640 && viewer_frame.height == 360,
                "viewer UI should compose at drawable dimensions") ||
        !Expect(!viewer_frame.Empty(), "viewer HTML/CSS screens should produce geometry"))
        return 1;

    ui.Shutdown();
    return Expect(!ui.Initialized(), "viewer UI shutdown should be symmetric") ? 0 : 1;
}
