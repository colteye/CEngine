//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/ui/ui_system.cpp
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#include "ui/ui_system.h"

#include "ui/rmlui/rmlui_runtime.h"

#include <utility>

namespace CEngine::UI
{

class UISystem::Impl
{
  public:
    RmlUiRuntime runtime;
};

UISystem::UISystem() : impl_(std::make_unique<Impl>())
{
}

UISystem::~UISystem() = default;

bool UISystem::Initialize(Window::WindowSystem &window, std::filesystem::path content_root)
{
    return impl_->runtime.Initialize(window, std::move(content_root));
}

void UISystem::Shutdown()
{
    impl_->runtime.Shutdown();
}

bool UISystem::Initialized() const
{
    return impl_->runtime.Initialized();
}

bool UISystem::LoadFont(std::string_view path, bool fallback)
{
    return impl_->runtime.LoadFont(path, fallback);
}

UiScreenHandle UISystem::LoadScreen(std::string_view path)
{
    return impl_->runtime.LoadScreen(path);
}

bool UISystem::UnloadScreen(UiScreenHandle screen)
{
    return impl_->runtime.UnloadScreen(screen);
}

bool UISystem::Show(UiScreenHandle screen, bool modal)
{
    return impl_->runtime.Show(screen, modal);
}

bool UISystem::Hide(UiScreenHandle screen)
{
    return impl_->runtime.Hide(screen);
}

bool UISystem::BindClick(UiScreenHandle screen, std::string_view element_id, std::string action)
{
    return impl_->runtime.BindClick(screen, element_id, std::move(action));
}

bool UISystem::BindChange(UiScreenHandle screen, std::string_view element_id, std::string action)
{
    return impl_->runtime.BindChange(screen, element_id, std::move(action));
}

bool UISystem::SetText(UiScreenHandle screen, std::string_view element_id, std::string_view text)
{
    return impl_->runtime.SetText(screen, element_id, text);
}

bool UISystem::SetValue(UiScreenHandle screen, std::string_view element_id, float value)
{
    return impl_->runtime.SetValue(screen, element_id, value);
}

bool UISystem::SetChecked(UiScreenHandle screen, std::string_view element_id, bool checked)
{
    return impl_->runtime.SetChecked(screen, element_id, checked);
}

void UISystem::Update(const Input::InputSystem &input)
{
    impl_->runtime.Update(input);
}

Renderer::UiFrame UISystem::Compose()
{
    return impl_->runtime.Compose();
}

std::vector<UiEvent> UISystem::DrainEvents()
{
    return impl_->runtime.DrainEvents();
}

} // namespace CEngine::UI
