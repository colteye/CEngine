// Copyright (c) CEngine contributors.

#include "ui/rmlui/rmlui_system_interface.h"

#include "log.h"
#include "window/window_system.h"

namespace CEngine::UI
{

RmlUiSystemInterface::RmlUiSystemInterface(Window::WindowSystem &window) : window_(&window)
{
}

double RmlUiSystemInterface::GetElapsedTime()
{
    return window_ != nullptr ? window_->TimeSeconds() : 0.0;
}

bool RmlUiSystemInterface::LogMessage(Rml::Log::Type type, const Rml::String &message)
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
