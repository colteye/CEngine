//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/ui/ui_system.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

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
    std::string value;
    bool checked = false;
};

/**
 * Engine-owned HTML/CSS game UI facade. Callers exchange only CEngine handles,
 * events, and renderer-neutral frame data.
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
     * Bind a clicked HTML element to a game-owned semantic action.
     */
    bool BindClick(UiScreenHandle screen, std::string_view element_id, std::string action);
    bool BindChange(UiScreenHandle screen, std::string_view element_id, std::string action);

    bool SetText(UiScreenHandle screen, std::string_view element_id, std::string_view text);
    bool SetValue(UiScreenHandle screen, std::string_view element_id, float value);
    bool SetChecked(UiScreenHandle screen, std::string_view element_id, bool checked);

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
