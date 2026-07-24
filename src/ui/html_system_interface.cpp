//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/ui/html_system_interface.cpp
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#include "ui/html_system_interface.h"

#include "log.h"
#include "window/window_system.h"

namespace CEngine::UI
{

HtmlSystemInterface::HtmlSystemInterface(Window::WindowSystem &window) : window_(&window)
{
}

double HtmlSystemInterface::GetElapsedTime()
{
    return window_ != nullptr ? window_->TimeSeconds() : 0.0;
}

bool HtmlSystemInterface::LogMessage(Rml::Log::Type type, const Rml::String &message)
{
    switch (type)
    {
    case Rml::Log::LT_ERROR:
    case Rml::Log::LT_ASSERT:
        Logging::Logger::Get().Error("ui", message);
        break;
    case Rml::Log::LT_WARNING:
        Logging::Logger::Get().Warning("ui", message);
        break;
    default:
        Logging::Logger::Get().Info("ui", message);
        break;
    }
    return true;
}

} // namespace CEngine::UI
