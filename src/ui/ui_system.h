// Copyright (c) CEngine contributors.

#ifndef CENGINE_UI_UI_SYSTEM_H
#define CENGINE_UI_UI_SYSTEM_H

#include "handle.h"
#include "renderer/ui_frame.h"

#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace CEngine::Input
{
class InputSystem;
}
namespace CEngine::Window
{
class WindowSystem;
}

namespace CEngine::UI
{

struct UiScreenTag;
using UiScreenHandle = Handle<UiScreenTag>;

struct UiEvent
{
    UiScreenHandle screen;
    std::string action;
};

/**
 * Engine-owned game UI facade. RmlUi is private to the implementation; callers
 * exchange only CEngine handles, events, and renderer-neutral frame data.
 */
class UISystem
{
  public:
    UISystem();
    ~UISystem();
    UISystem(const UISystem &) = delete;
    UISystem &operator=(const UISystem &) = delete;
    UISystem(UISystem &&) = delete;
    UISystem &operator=(UISystem &&) = delete;

    bool Initialize(Window::WindowSystem &window, std::filesystem::path content_root);
    void Shutdown();
    [[nodiscard]] bool Initialized() const;

    bool LoadFont(std::string_view content_relative_path, bool fallback = false);
    UiScreenHandle LoadScreen(std::string_view content_relative_path);
    bool UnloadScreen(UiScreenHandle screen);
    bool Show(UiScreenHandle screen, bool modal = false);
    bool Hide(UiScreenHandle screen);

    /**
     * Bind a clicked RML element to a game-owned semantic action.
     */
    bool BindClick(UiScreenHandle screen, std::string_view element_id, std::string action);

    /**
     * Consume one normalized client input snapshot and update layout,
     * animation, focus, and interaction at display-frame cadence.
     */
    void Update(const Input::InputSystem &input);

    /**
     * Ask the private runtime to emit one renderer-neutral UI composition.
     */
    Renderer::UiFrame Compose();

    /**
     * Events remain valid until the next call to DrainEvents().
     */
    std::vector<UiEvent> DrainEvents();

  private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace CEngine::UI

#endif
