// Copyright (c) CEngine contributors.

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
