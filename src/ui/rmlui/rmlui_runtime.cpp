// Copyright (c) CEngine contributors.

#include "ui/rmlui/rmlui_runtime.h"

#include "input/input_system.h"
#include "log.h"
#include "ui/rmlui/rmlui_file_interface.h"
#include "ui/rmlui/rmlui_render_interface.h"
#include "ui/rmlui/rmlui_system_interface.h"
#include "window/window_system.h"

#include <RmlUi/Core.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/Input.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <utility>

namespace CEngine::UI
{
namespace
{

bool RuntimeActive = false;

Rml::Input::KeyIdentifier RmlKey(Input::Key key)
{
    using RmlKeyId = Rml::Input::KeyIdentifier;
    const auto value = static_cast<int>(key);
    const auto digit0 = static_cast<int>(Input::Key::Digit0);
    const auto a = static_cast<int>(Input::Key::A);
    const auto function1 = static_cast<int>(Input::Key::F1);
    const auto keypad0 = static_cast<int>(Input::Key::Keypad0);
    if (key >= Input::Key::Digit0 && key <= Input::Key::Digit9)
    {
        return static_cast<RmlKeyId>(Rml::Input::KI_0 + value - digit0);
    }
    if (key >= Input::Key::A && key <= Input::Key::Z)
    {
        return static_cast<RmlKeyId>(Rml::Input::KI_A + value - a);
    }
    if (key >= Input::Key::F1 && key <= Input::Key::F24)
    {
        return static_cast<RmlKeyId>(Rml::Input::KI_F1 + value - function1);
    }
    if (key >= Input::Key::Keypad0 && key <= Input::Key::Keypad9)
    {
        return static_cast<RmlKeyId>(Rml::Input::KI_NUMPAD0 + value - keypad0);
    }

    switch (key)
    {
    case Input::Key::Space:
        return Rml::Input::KI_SPACE;
    case Input::Key::Apostrophe:
        return Rml::Input::KI_OEM_7;
    case Input::Key::Comma:
        return Rml::Input::KI_OEM_COMMA;
    case Input::Key::Minus:
        return Rml::Input::KI_OEM_MINUS;
    case Input::Key::Period:
        return Rml::Input::KI_OEM_PERIOD;
    case Input::Key::Slash:
        return Rml::Input::KI_OEM_2;
    case Input::Key::Semicolon:
        return Rml::Input::KI_OEM_1;
    case Input::Key::Equal:
        return Rml::Input::KI_OEM_PLUS;
    case Input::Key::LeftBracket:
        return Rml::Input::KI_OEM_4;
    case Input::Key::Backslash:
        return Rml::Input::KI_OEM_5;
    case Input::Key::RightBracket:
        return Rml::Input::KI_OEM_6;
    case Input::Key::GraveAccent:
        return Rml::Input::KI_OEM_3;
    case Input::Key::World1:
    case Input::Key::World2:
        return Rml::Input::KI_OEM_102;
    case Input::Key::Escape:
        return Rml::Input::KI_ESCAPE;
    case Input::Key::Enter:
        return Rml::Input::KI_RETURN;
    case Input::Key::Tab:
        return Rml::Input::KI_TAB;
    case Input::Key::Backspace:
        return Rml::Input::KI_BACK;
    case Input::Key::Insert:
        return Rml::Input::KI_INSERT;
    case Input::Key::Delete:
        return Rml::Input::KI_DELETE;
    case Input::Key::Right:
        return Rml::Input::KI_RIGHT;
    case Input::Key::Left:
        return Rml::Input::KI_LEFT;
    case Input::Key::Down:
        return Rml::Input::KI_DOWN;
    case Input::Key::Up:
        return Rml::Input::KI_UP;
    case Input::Key::PageUp:
        return Rml::Input::KI_PRIOR;
    case Input::Key::PageDown:
        return Rml::Input::KI_NEXT;
    case Input::Key::Home:
        return Rml::Input::KI_HOME;
    case Input::Key::End:
        return Rml::Input::KI_END;
    case Input::Key::CapsLock:
        return Rml::Input::KI_CAPITAL;
    case Input::Key::ScrollLock:
        return Rml::Input::KI_SCROLL;
    case Input::Key::NumLock:
        return Rml::Input::KI_NUMLOCK;
    case Input::Key::PrintScreen:
        return Rml::Input::KI_SNAPSHOT;
    case Input::Key::Pause:
        return Rml::Input::KI_PAUSE;
    case Input::Key::KeypadDecimal:
        return Rml::Input::KI_DECIMAL;
    case Input::Key::KeypadDivide:
        return Rml::Input::KI_DIVIDE;
    case Input::Key::KeypadMultiply:
        return Rml::Input::KI_MULTIPLY;
    case Input::Key::KeypadSubtract:
        return Rml::Input::KI_SUBTRACT;
    case Input::Key::KeypadAdd:
        return Rml::Input::KI_ADD;
    case Input::Key::KeypadEnter:
        return Rml::Input::KI_NUMPADENTER;
    case Input::Key::KeypadEqual:
        return Rml::Input::KI_OEM_NEC_EQUAL;
    case Input::Key::LeftShift:
        return Rml::Input::KI_LSHIFT;
    case Input::Key::LeftControl:
        return Rml::Input::KI_LCONTROL;
    case Input::Key::LeftAlt:
        return Rml::Input::KI_LMENU;
    case Input::Key::LeftSuper:
        return Rml::Input::KI_LMETA;
    case Input::Key::RightShift:
        return Rml::Input::KI_RSHIFT;
    case Input::Key::RightControl:
        return Rml::Input::KI_RCONTROL;
    case Input::Key::RightAlt:
        return Rml::Input::KI_RMENU;
    case Input::Key::RightSuper:
        return Rml::Input::KI_RMETA;
    case Input::Key::Menu:
        return Rml::Input::KI_APPS;
    default:
        return Rml::Input::KI_UNKNOWN;
    }
}

int RmlModifiers(const Input::InputSystem &input)
{
    int modifiers = 0;
    if (input.IsDown(Input::Key::LeftControl) || input.IsDown(Input::Key::RightControl))
    {
        modifiers |= Rml::Input::KM_CTRL;
    }
    if (input.IsDown(Input::Key::LeftShift) || input.IsDown(Input::Key::RightShift))
    {
        modifiers |= Rml::Input::KM_SHIFT;
    }
    if (input.IsDown(Input::Key::LeftAlt) || input.IsDown(Input::Key::RightAlt))
    {
        modifiers |= Rml::Input::KM_ALT;
    }
    if (input.IsDown(Input::Key::LeftSuper) || input.IsDown(Input::Key::RightSuper))
    {
        modifiers |= Rml::Input::KM_META;
    }
    return modifiers;
}

int RmlPointerButton(Input::PointerButton button)
{
    switch (button)
    {
    case Input::PointerButton::Primary:
        return 0;
    case Input::PointerButton::Secondary:
        return 1;
    case Input::PointerButton::Middle:
        return 2;
    }
    return 0;
}

} // namespace

class RmlUiRuntime::Impl
{
  public:
    struct ClickListener final : Rml::EventListener
    {
        ClickListener(Impl &owner, UiScreenHandle screen, std::string action)
            : owner(owner), screen(screen), action(std::move(action))
        {
        }

        void ProcessEvent(Rml::Event &) override
        {
            owner.events.push_back({screen, action});
        }

        Impl &owner;
        UiScreenHandle screen;
        std::string action;
    };

    struct Binding
    {
        Rml::Element *element = nullptr;
        std::unique_ptr<ClickListener> listener;
    };

    struct Screen
    {
        Rml::ElementDocument *document = nullptr;
        std::vector<Binding> bindings;
        std::uint32_t generation = 1;
    };

    Screen *Resolve(UiScreenHandle handle)
    {
        if (!handle || handle.Index() >= screens.size())
        {
            return nullptr;
        }
        Screen &screen = screens[handle.Index()];
        return screen.document != nullptr && screen.generation == handle.Generation() ? &screen : nullptr;
    }

    const Screen *Resolve(UiScreenHandle handle) const
    {
        return const_cast<Impl *>(this)->Resolve(handle);
    }

    void DetachBindings(Screen &screen)
    {
        for (Binding &binding : screen.bindings)
        {
            if (binding.element != nullptr && binding.listener != nullptr)
            {
                binding.element->RemoveEventListener(Rml::EventId::Click, binding.listener.get());
            }
        }
        screen.bindings.clear();
    }

    Window::WindowSystem *window = nullptr;
    std::unique_ptr<RmlUiFileInterface> file_interface;
    std::unique_ptr<RmlUiSystemInterface> system_interface;
    std::unique_ptr<RmlUiRenderInterface> render_interface;
    Rml::Context *context = nullptr;
    std::vector<Screen> screens;
    std::vector<std::uint32_t> free_screens;
    std::vector<UiEvent> events;
    bool initialized = false;
};

RmlUiRuntime::RmlUiRuntime() : impl_(std::make_unique<Impl>())
{
}

RmlUiRuntime::~RmlUiRuntime()
{
    Shutdown();
}

bool RmlUiRuntime::Initialize(Window::WindowSystem &window, std::filesystem::path content_root)
{
    Shutdown();
    if (RuntimeActive || !std::filesystem::is_directory(content_root))
    {
        Logging::Logger::Get().Error(
            "ui", RuntimeActive ? "only one RmlUi runtime may be active" : "UI content root does not exist");
        return false;
    }

    impl_->window = &window;
    impl_->file_interface = std::make_unique<RmlUiFileInterface>(std::move(content_root));
    impl_->system_interface = std::make_unique<RmlUiSystemInterface>(window);
    impl_->render_interface = std::make_unique<RmlUiRenderInterface>();
    Rml::SetFileInterface(impl_->file_interface.get());
    Rml::SetSystemInterface(impl_->system_interface.get());
    Rml::SetRenderInterface(impl_->render_interface.get());
    if (!Rml::Initialise())
    {
        Logging::Logger::Get().Error("ui", "failed to initialize RmlUi");
        Rml::SetRenderInterface(nullptr);
        Rml::SetSystemInterface(nullptr);
        Rml::SetFileInterface(nullptr);
        impl_->render_interface.reset();
        impl_->system_interface.reset();
        impl_->file_interface.reset();
        impl_->window = nullptr;
        return false;
    }

    const glm::ivec2 size = window.Size();
    impl_->context = Rml::CreateContext("cengine-game-ui", {std::max(size.x, 1), std::max(size.y, 1)});
    if (impl_->context == nullptr)
    {
        Logging::Logger::Get().Error("ui", "failed to create the RmlUi context");
        Rml::Shutdown();
        Rml::SetRenderInterface(nullptr);
        Rml::SetSystemInterface(nullptr);
        Rml::SetFileInterface(nullptr);
        impl_->render_interface.reset();
        impl_->system_interface.reset();
        impl_->file_interface.reset();
        impl_->window = nullptr;
        return false;
    }

    RuntimeActive = true;
    impl_->initialized = true;
    return true;
}

void RmlUiRuntime::Shutdown()
{
    if (!impl_->initialized)
    {
        return;
    }
    for (Impl::Screen &screen : impl_->screens)
    {
        if (screen.document != nullptr)
        {
            impl_->DetachBindings(screen);
            screen.document->Close();
            screen.document = nullptr;
        }
    }
    if (impl_->context != nullptr)
    {
        impl_->context->Update();
        Rml::RemoveContext("cengine-game-ui");
        impl_->context = nullptr;
    }
    Rml::Shutdown();
    Rml::SetRenderInterface(nullptr);
    Rml::SetSystemInterface(nullptr);
    Rml::SetFileInterface(nullptr);

    impl_->screens.clear();
    impl_->free_screens.clear();
    impl_->events.clear();
    impl_->render_interface.reset();
    impl_->system_interface.reset();
    impl_->file_interface.reset();
    impl_->window = nullptr;
    impl_->initialized = false;
    RuntimeActive = false;
}

bool RmlUiRuntime::Initialized() const
{
    return impl_->initialized;
}

bool RmlUiRuntime::LoadFont(std::string_view path, bool fallback)
{
    return impl_->initialized && !path.empty() && Rml::LoadFontFace(Rml::String(path), fallback);
}

UiScreenHandle RmlUiRuntime::LoadScreen(std::string_view path)
{
    if (!impl_->initialized || path.empty())
    {
        return {};
    }
    Rml::ElementDocument *document = impl_->context->LoadDocument(Rml::String(path));
    if (document == nullptr)
    {
        Logging::Logger::Get().Error("ui", "failed to load UI screen: " + std::string(path));
        return {};
    }

    std::uint32_t index = 0;
    if (impl_->free_screens.empty())
    {
        if (impl_->screens.size() >= std::numeric_limits<std::uint32_t>::max())
        {
            document->Close();
            return {};
        }
        index = static_cast<std::uint32_t>(impl_->screens.size());
        impl_->screens.emplace_back();
    }
    else
    {
        index = impl_->free_screens.back();
        impl_->free_screens.pop_back();
    }
    Impl::Screen &screen = impl_->screens[index];
    screen.document = document;
    return {index, screen.generation};
}

bool RmlUiRuntime::UnloadScreen(UiScreenHandle handle)
{
    Impl::Screen *screen = impl_->Resolve(handle);
    if (screen == nullptr)
    {
        return false;
    }
    impl_->DetachBindings(*screen);
    screen->document->Close();
    screen->document = nullptr;
    ++screen->generation;
    if (screen->generation == 0)
    {
        ++screen->generation;
    }
    impl_->free_screens.push_back(handle.Index());
    return true;
}

bool RmlUiRuntime::Show(UiScreenHandle handle, bool modal)
{
    Impl::Screen *screen = impl_->Resolve(handle);
    if (screen == nullptr)
    {
        return false;
    }
    screen->document->Show(modal ? Rml::ModalFlag::Modal : Rml::ModalFlag::None, Rml::FocusFlag::Auto);
    return true;
}

bool RmlUiRuntime::Hide(UiScreenHandle handle)
{
    Impl::Screen *screen = impl_->Resolve(handle);
    if (screen == nullptr)
    {
        return false;
    }
    screen->document->Hide();
    return true;
}

bool RmlUiRuntime::BindClick(UiScreenHandle handle, std::string_view element_id, std::string action)
{
    Impl::Screen *screen = impl_->Resolve(handle);
    if (screen == nullptr || element_id.empty() || action.empty())
    {
        return false;
    }
    Rml::Element *element = screen->document->GetElementById(Rml::String(element_id));
    if (element == nullptr)
    {
        return false;
    }
    auto listener = std::make_unique<Impl::ClickListener>(*impl_, handle, std::move(action));
    element->AddEventListener(Rml::EventId::Click, listener.get());
    screen->bindings.push_back({element, std::move(listener)});
    return true;
}

void RmlUiRuntime::Update(const Input::InputSystem &input)
{
    if (!impl_->initialized)
    {
        return;
    }
    const glm::ivec2 size = impl_->window->Size();
    const Rml::Vector2i dimensions(std::max(size.x, 1), std::max(size.y, 1));
    if (impl_->context->GetDimensions() != dimensions)
    {
        impl_->context->SetDimensions(dimensions);
    }

    const int modifiers = RmlModifiers(input);
    const glm::vec2 pointer = input.PointerPosition();
    impl_->context->ProcessMouseMove(static_cast<int>(std::lround(pointer.x)),
                                    static_cast<int>(std::lround(pointer.y)), modifiers);
    for (const Input::InputEvent &event : input.Events())
    {
        switch (event.type)
        {
        case Input::InputEventType::PointerMove:
            impl_->context->ProcessMouseMove(static_cast<int>(std::lround(event.position.x)),
                                             static_cast<int>(std::lround(event.position.y)), modifiers);
            break;
        case Input::InputEventType::PointerButtonDown:
            impl_->context->ProcessMouseButtonDown(RmlPointerButton(event.button), modifiers);
            break;
        case Input::InputEventType::PointerButtonUp:
            impl_->context->ProcessMouseButtonUp(RmlPointerButton(event.button), modifiers);
            break;
        case Input::InputEventType::PointerWheel:
            impl_->context->ProcessMouseWheel({-event.wheel.x, -event.wheel.y}, modifiers);
            break;
        case Input::InputEventType::PointerLeave:
            impl_->context->ProcessMouseLeave();
            break;
        case Input::InputEventType::KeyDown:
            impl_->context->ProcessKeyDown(RmlKey(event.key), modifiers);
            if (event.key == Input::Key::Enter || event.key == Input::Key::KeypadEnter)
            {
                impl_->context->ProcessTextInput('\n');
            }
            break;
        case Input::InputEventType::KeyUp:
            impl_->context->ProcessKeyUp(RmlKey(event.key), modifiers);
            break;
        case Input::InputEventType::TextInput:
            impl_->context->ProcessTextInput(event.text);
            break;
        }
    }
    impl_->context->Update();
}

Renderer::UiFrame RmlUiRuntime::Compose()
{
    if (!impl_->initialized)
    {
        return {};
    }
    const Rml::Vector2i size = impl_->context->GetDimensions();
    impl_->render_interface->BeginFrame(size.x, size.y);
    impl_->context->Render();
    return impl_->render_interface->TakeFrame();
}

std::vector<UiEvent> RmlUiRuntime::DrainEvents()
{
    std::vector<UiEvent> result = std::move(impl_->events);
    impl_->events.clear();
    return result;
}

} // namespace CEngine::UI
