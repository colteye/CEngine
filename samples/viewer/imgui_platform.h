// Copyright (c) CEngine contributors.

#ifndef CENGINE_VIEWER_IMGUI_PLATFORM_H
#define CENGINE_VIEWER_IMGUI_PLATFORM_H

namespace CEngine::Window
{
class WindowSystem;
}

namespace Viewer::ImGuiPlatform
{

bool Initialize(CEngine::Window::WindowSystem &window);
void Shutdown();
void NewFrame();
void ProcessEvent(const void *event, void *user_data);

} // namespace Viewer::ImGuiPlatform

#endif
