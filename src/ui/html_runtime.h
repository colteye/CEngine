//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/ui/html_runtime.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#ifndef CENGINE_UI_HTML_RUNTIME_H
#define CENGINE_UI_HTML_RUNTIME_H

#include "ui/ui_system.h"

#include <filesystem>
#include <memory>
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

/**
 * Private concrete implementation of UISystem. Parser and layout types stay
 * behind this PIMPL boundary.
 */
class HtmlRuntime
{
  public:
    HtmlRuntime();
    ~HtmlRuntime();
    HtmlRuntime(const HtmlRuntime &) = delete;
    HtmlRuntime &operator=(const HtmlRuntime &) = delete;

    bool Initialize(Window::WindowSystem &window, std::filesystem::path content_root);
    void Shutdown();
    [[nodiscard]] bool Initialized() const;

    bool LoadFont(std::string_view path, bool fallback);
    UiScreenHandle LoadScreen(std::string_view path);
    bool UnloadScreen(UiScreenHandle screen);
    bool Show(UiScreenHandle screen, bool modal);
    bool Hide(UiScreenHandle screen);
    bool BindClick(UiScreenHandle screen, std::string_view element_id, std::string action);
    bool BindChange(UiScreenHandle screen, std::string_view element_id, std::string action);
    bool SetText(UiScreenHandle screen, std::string_view element_id, std::string_view text);
    bool SetValue(UiScreenHandle screen, std::string_view element_id, float value);
    bool SetChecked(UiScreenHandle screen, std::string_view element_id, bool checked);
    void Update(const Input::InputSystem &input);
    Renderer::UiFrame Compose();
    std::vector<UiEvent> DrainEvents();

  private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace CEngine::UI

#endif
