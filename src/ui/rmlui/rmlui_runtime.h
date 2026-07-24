// Copyright (c) CEngine contributors.

#ifndef CENGINE_UI_RMLUI_RUNTIME_H
#define CENGINE_UI_RMLUI_RUNTIME_H

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
 * Private concrete implementation of UISystem. RmlUi types stay behind this
 * PIMPL boundary so the public engine API can survive a future native runtime.
 */
class RmlUiRuntime
{
  public:
    RmlUiRuntime();
    ~RmlUiRuntime();
    RmlUiRuntime(const RmlUiRuntime &) = delete;
    RmlUiRuntime &operator=(const RmlUiRuntime &) = delete;

    bool Initialize(Window::WindowSystem &window, std::filesystem::path content_root);
    void Shutdown();
    [[nodiscard]] bool Initialized() const;

    bool LoadFont(std::string_view path, bool fallback);
    UiScreenHandle LoadScreen(std::string_view path);
    bool UnloadScreen(UiScreenHandle screen);
    bool Show(UiScreenHandle screen, bool modal);
    bool Hide(UiScreenHandle screen);
    bool BindClick(UiScreenHandle screen, std::string_view element_id, std::string action);
    void Update(const Input::InputSystem &input);
    Renderer::UiFrame Compose();
    std::vector<UiEvent> DrainEvents();

  private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace CEngine::UI

#endif
