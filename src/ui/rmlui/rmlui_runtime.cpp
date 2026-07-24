//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

/**
 * @file src/ui/rmlui/rmlui_runtime.cpp
 * @brief TODO: Describe the purpose of this file.
 * @author Erik Coltey
 */

#include "ui/rmlui/rmlui_runtime.h"

#include "input/input_system.h"
#include "log.h"
#include "ui/rmlui/rmlui_file_interface.h"
#include "ui/rmlui/rmlui_render_interface.h"
#include "ui/rmlui/rmlui_system_interface.h"
#include "window/window_system.h"

#include <RmlUi/Core.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/ElementText.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/Input.h>
#include <RmlUi/Core/StringUtilities.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>

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

struct UiMetrics
{
    glm::ivec2 dimensions{1};
    glm::vec2 pointer_scale{1.0f};
    float dp_ratio = 1.0f;
};

UiMetrics WindowUiMetrics(const Window::WindowSystem &window)
{
    const glm::ivec2 logical = glm::max(window.Size(), glm::ivec2(1));
    const glm::ivec2 drawable = glm::max(window.DrawableSize(), glm::ivec2(1));
    UiMetrics result;
    result.dimensions = drawable;
    result.pointer_scale = glm::vec2(drawable) / glm::vec2(logical);
    result.dp_ratio = (result.pointer_scale.x + result.pointer_scale.y) * 0.5f;
    return result;
}

Rml::Vector2i RmlPointerPosition(glm::vec2 position, glm::vec2 scale)
{
    const glm::vec2 pixels = position * scale;
    return {static_cast<int>(std::lround(pixels.x)), static_cast<int>(std::lround(pixels.y))};
}

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
    struct ActionListener final : Rml::EventListener
    {
        ActionListener(Impl &owner, UiScreenHandle screen, std::string action, bool include_value)
            : owner(owner), screen(screen), action(std::move(action)), include_value(include_value)
        {
        }

        void ProcessEvent(Rml::Event &event) override
        {
            UiEvent result;
            result.screen = screen;
            result.action = action;
            if (include_value)
            {
                result.value = event.GetParameter<Rml::String>("value", "");
                result.checked = event.GetParameter<bool>("checked", false);
            }
            owner.events.push_back(std::move(result));
        }

        Impl &owner;
        UiScreenHandle screen;
        std::string action;
        bool include_value = false;
    };

    struct Binding
    {
        Rml::Element *element = nullptr;
        Rml::EventId event_id = Rml::EventId::Invalid;
        std::unique_ptr<ActionListener> listener;
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
                binding.element->RemoveEventListener(binding.event_id, binding.listener.get());
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
    bool needs_update_before_compose = false;
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
        Logging::Logger::Get().Error("ui", RuntimeActive ? "only one RmlUi runtime may be active"
                                                         : "UI content root does not exist");
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

    const UiMetrics metrics = WindowUiMetrics(window);
    impl_->context = Rml::CreateContext("cengine-game-ui", {metrics.dimensions.x, metrics.dimensions.y});
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
    impl_->context->SetDensityIndependentPixelRatio(metrics.dp_ratio);

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
    impl_->needs_update_before_compose = true;
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
    impl_->needs_update_before_compose = true;
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
    impl_->needs_update_before_compose = true;
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
    impl_->needs_update_before_compose = true;
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
    auto listener = std::make_unique<Impl::ActionListener>(*impl_, handle, std::move(action), false);
    element->AddEventListener(Rml::EventId::Click, listener.get());
    screen->bindings.push_back({element, Rml::EventId::Click, std::move(listener)});
    return true;
}

bool RmlUiRuntime::BindChange(UiScreenHandle handle, std::string_view element_id, std::string action)
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
    auto listener = std::make_unique<Impl::ActionListener>(*impl_, handle, std::move(action), true);
    element->AddEventListener(Rml::EventId::Change, listener.get());
    screen->bindings.push_back({element, Rml::EventId::Change, std::move(listener)});
    return true;
}

bool RmlUiRuntime::SetText(UiScreenHandle handle, std::string_view element_id, std::string_view text)
{
    Impl::Screen *screen = impl_->Resolve(handle);
    Rml::Element *element =
        screen != nullptr ? screen->document->GetElementById(Rml::String(element_id)) : nullptr;
    if (element == nullptr)
    {
        return false;
    }

    auto *text_element = rmlui_dynamic_cast<Rml::ElementText *>(element->GetFirstChild());
    if (text_element == nullptr || element->GetNumChildren() != 1)
    {
        element->SetInnerRML(Rml::StringUtilities::EncodeRml(Rml::String(text)));
        text_element = rmlui_dynamic_cast<Rml::ElementText *>(element->GetFirstChild());
        if (text_element == nullptr)
        {
            return false;
        }
    }

    // Preserve the text node so a changing readout never destroys and recreates
    // its DOM subtree. Compose performs a second, input-free update when needed,
    // making the new glyph geometry visible in this same rendered frame.
    text_element->SetText(Rml::String(text));
    impl_->needs_update_before_compose = true;
    return true;
}

bool RmlUiRuntime::SetValue(UiScreenHandle handle, std::string_view element_id, float value)
{
    Impl::Screen *screen = impl_->Resolve(handle);
    Rml::Element *element =
        screen != nullptr ? screen->document->GetElementById(Rml::String(element_id)) : nullptr;
    auto *input = rmlui_dynamic_cast<Rml::ElementFormControlInput *>(element);
    if (input == nullptr || !std::isfinite(value))
    {
        return false;
    }
    input->SetValue(std::to_string(value));
    impl_->needs_update_before_compose = true;
    return true;
}

bool RmlUiRuntime::SetChecked(UiScreenHandle handle, std::string_view element_id, bool checked)
{
    Impl::Screen *screen = impl_->Resolve(handle);
    Rml::Element *element =
        screen != nullptr ? screen->document->GetElementById(Rml::String(element_id)) : nullptr;
    if (element == nullptr)
    {
        return false;
    }
    if (checked)
    {
        element->SetAttribute("checked", "");
    }
    else
    {
        element->RemoveAttribute("checked");
    }
    impl_->needs_update_before_compose = true;
    return true;
}

void RmlUiRuntime::Update(const Input::InputSystem &input)
{
    if (!impl_->initialized)
    {
        return;
    }
    const UiMetrics metrics = WindowUiMetrics(*impl_->window);
    const Rml::Vector2i dimensions(metrics.dimensions.x, metrics.dimensions.y);
    if (impl_->context->GetDimensions() != dimensions)
    {
        impl_->context->SetDimensions(dimensions);
    }
    if (impl_->context->GetDensityIndependentPixelRatio() != metrics.dp_ratio)
    {
        impl_->context->SetDensityIndependentPixelRatio(metrics.dp_ratio);
    }

    const int modifiers = RmlModifiers(input);
    for (const Input::InputEvent &event : input.Events())
    {
        switch (event.type)
        {
        case Input::InputEventType::PointerMove:
            {
                const Rml::Vector2i position = RmlPointerPosition(event.position, metrics.pointer_scale);
                impl_->context->ProcessMouseMove(position.x, position.y, modifiers);
            }
            break;
        case Input::InputEventType::PointerButtonDown:
            {
                const Rml::Vector2i position = RmlPointerPosition(event.position, metrics.pointer_scale);
                impl_->context->ProcessMouseMove(position.x, position.y, modifiers);
            }
            impl_->context->ProcessMouseButtonDown(RmlPointerButton(event.button), modifiers);
            break;
        case Input::InputEventType::PointerButtonUp:
            {
                const Rml::Vector2i position = RmlPointerPosition(event.position, metrics.pointer_scale);
                impl_->context->ProcessMouseMove(position.x, position.y, modifiers);
            }
            impl_->context->ProcessMouseButtonUp(RmlPointerButton(event.button), modifiers);
            break;
        case Input::InputEventType::PointerWheel:
            {
                const Rml::Vector2i position = RmlPointerPosition(event.position, metrics.pointer_scale);
                impl_->context->ProcessMouseMove(position.x, position.y, modifiers);
            }
            impl_->context->ProcessMouseWheel({event.wheel.x, event.wheel.y}, modifiers);
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
    impl_->needs_update_before_compose = false;
}

Renderer::UiFrame RmlUiRuntime::Compose()
{
    if (!impl_->initialized)
    {
        return {};
    }
    if (impl_->needs_update_before_compose)
    {
        impl_->context->Update();
        impl_->needs_update_before_compose = false;
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
