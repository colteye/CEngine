// Copyright (c) CEngine contributors.

#include "imgui_platform.h"

#include "window/window_system.h"

#include <backends/imgui_impl_sdl3.h>

namespace Viewer::ImGuiPlatform
{

bool Initialize(CEngine::Window::WindowSystem &window)
{
    return ImGui_ImplSDL3_InitForOpenGL(static_cast<SDL_Window *>(window.NativeHandle()),
                                        window.NativeGraphicsContext());
}

void Shutdown()
{
    ImGui_ImplSDL3_Shutdown();
}

void NewFrame()
{
    ImGui_ImplSDL3_NewFrame();
}

void ProcessEvent(const void *event, void *)
{
    ImGui_ImplSDL3_ProcessEvent(static_cast<const SDL_Event *>(event));
}

} // namespace Viewer::ImGuiPlatform
