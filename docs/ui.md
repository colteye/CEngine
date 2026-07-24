# UI system

CEngine has a narrow in-game UI facade backed privately by
[RmlUi](https://github.com/mikke89/RmlUi). The engine API owns screen lifetime,
semantic actions, normalized input, and renderer submission. RmlUi owns RML
document parsing, RCSS layout and styling, focus, and hit testing.

## Public boundary

Game code includes `ui/ui_system.h`; it does not include an RmlUi header or
retain an RmlUi object.

```cpp
CEngine::UI::UISystem ui;
ui.Initialize(window, content_root);
ui.LoadFont("ui/fonts/LatoLatin-Regular.ttf");

const auto menu = ui.LoadScreen("ui/start_menu.rml");
ui.BindClick(menu, "start-button", "start_game");
ui.Show(menu, true);

ui.Update(input);
for (const CEngine::UI::UiEvent &event : ui.DrainEvents())
{
    if (event.action == "start_game")
        ui.Hide(menu);
}

renderer.SetUiFrame(ui.Compose());
renderer.Render();
```

Screens use generation-checked handles. Click and change bindings publish
game-owned action strings rather than callbacks or DOM objects. Change events
also carry a form value and checked state. `SetText`, `SetValue`, and
`SetChecked` provide the narrow live-update path required by HUD counters and
runtime controls. Unloading a screen detaches its listeners and invalidates its
handle.

## Data flow

```text
SDL event
  -> SDL input adapter
  -> normalized InputEvent
  -> UISystem
  -> private RmlUi context
  -> UiFrame (vertices, indices, batches, generated RGBA textures)
  -> RenderSystem
  -> compiled graphics backend
```

The RmlUi render interface captures CPU geometry and generated font atlases
into `UiFrame`. That frame contains no RmlUi or graphics-API types. RmlUi
composes at the full drawable pixel dimensions and uses its density-independent
pixel ratio to preserve physical layout size. OpenGL uploads and draws the
frame as the final two-dimensional pass using premultiplied alpha.

Pointer input has one source: the ordered normalized event stream. Motion,
button, and wheel events each carry their own logical-window position, which
the private runtime converts once into drawable coordinates. RmlUi retains
pressed state between events, so a hold can span any number of event-free
frames without polling a second cursor position.

RmlUi and FreeType are statically built from immutable revisions. RmlUi's
samples, Lua bindings, SVG plugin, and Lottie plugin are disabled. The UI file
interface is read-only, rooted at the content directory, and rejects absolute
paths and parent traversal.

## Authoring

RML and RCSS intentionally resemble HTML and CSS, but only RmlUi's supported
elements and properties are available. They should be treated as game content,
not as browser pages or a JavaScript runtime. Fonts are loaded explicitly
through `UISystem::LoadFont`; the RCSS `font-family` uses the family name
reported by the font.

The viewer provides a modal start screen, persistent FPS HUD, and runtime
tuning panel under `samples/viewer/ui`. Its Start button emits `start_game`;
fixed-step simulation and captured-look input stay paused until that action
arrives. Shift opens the RmlUi tuning panel and releases the pointer. The
viewer does not link or initialize ImGui.

## Current limits

- The OpenGL backend draws `UiFrame`; Vulkan still compiles against the same
  neutral frame boundary but does not draw UI while its general render path is
  incomplete.
- External RML image URLs are rejected until they can resolve through the
  cooked `Store` texture path. Generated FreeType atlases are supported.
- The facade exposes semantic click/change actions, screen visibility, modal
  focus, fonts, narrow text/form updates, and composition. General DOM access,
  scripting, localization, animation helpers, and game-specific widgets should
  be added only for concrete game requirements.
