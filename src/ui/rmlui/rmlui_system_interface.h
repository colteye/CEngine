// Copyright (c) CEngine contributors.

#ifndef CENGINE_UI_RMLUI_SYSTEM_INTERFACE_H
#define CENGINE_UI_RMLUI_SYSTEM_INTERFACE_H

#include <RmlUi/Core/SystemInterface.h>

namespace CEngine::Window
{
class WindowSystem;
}

namespace CEngine::UI
{

class RmlUiSystemInterface final : public Rml::SystemInterface
{
  public:
    explicit RmlUiSystemInterface(Window::WindowSystem &window);

    double GetElapsedTime() override;
    bool LogMessage(Rml::Log::Type type, const Rml::String &message) override;

  private:
    Window::WindowSystem *window_ = nullptr;
};

} // namespace CEngine::UI

#endif
