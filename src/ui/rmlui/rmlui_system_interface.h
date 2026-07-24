//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/ui/rmlui/rmlui_system_interface.h
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

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
