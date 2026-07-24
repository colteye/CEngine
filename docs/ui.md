# UI system

CEngine has a narrow in-game UI facade and an engine-owned HTML/CSS runtime.
The parser, layout, interaction, and frame-generation sources live under
`src/ui`; the derived core is under `src/ui/html` and compiles directly into
`CEngineCore`. RmlUi is not fetched, linked, or exposed as an engine backend.

The runtime is derived from RmlUi 6.2 Core and SVG sources, whose MIT license
and exact source revision are recorded beside the implementation. Its
inherited internal names are provenance, not CEngine content formats or public
API. LunaSVG is a pinned low-level rasterization dependency, not an engine
backend or authoring format.

## Public boundary

Game code includes `ui/ui_system.h`; it does not include parser or layout
headers or retain document objects.

```cpp
CEngine::UI::UISystem ui;
ui.Initialize(window, content_root);
ui.LoadFont("ui/fonts/LatoLatin-Regular.ttf");

const auto menu = ui.LoadScreen("ui/start_menu.html");
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
runtime controls. Unloading a screen detaches its bindings and invalidates its
handle. `LoadScreen` accepts `.html` paths only.

## Data flow

```text
SDL event
  -> SDL input adapter
  -> normalized InputEvent
  -> UISystem
  -> engine HTML/CSS runtime
  -> UiFrame (vertices, indices, batches, generated RGBA textures)
  -> RenderSystem
  -> compiled graphics backend
```

The runtime emits CPU geometry, generated FreeType atlases, and rasterized SVG
textures into `UiFrame`. That frame contains no parser, layout, SVG, or
graphics-API types. Composition uses the full drawable dimensions. CSS `px` is
interpreted as a logical pixel and scaled once at layout time, preserving the
previous physical size on high-DPI windows. OpenGL uploads and draws the frame
as the final two-dimensional pass using premultiplied alpha.

Pointer input has one source: the ordered normalized event stream. Motion,
button, and wheel events each carry their own logical-window position, which
the runtime converts once into drawable coordinates. Pressed state persists
between events, so a hold can span any number of event-free frames without
polling a second cursor position.

The read-only file interface is rooted at the content directory and rejects
absolute paths and parent traversal. FreeType and LunaSVG are pinned
rasterization dependencies; the HTML/CSS/SVG integration implementation itself
is in the engine tree.

## Approved HTML subset

Documents use normal `.html` files with an `<html>` root, `<head>`, and
`<body>`. External styles use the standard form:

```html
<html>
<head>
    <title>Start menu</title>
    <link rel="stylesheet" href="start_menu.css"/>
</head>
<body>
    <button id="start-button">Start</button>
</body>
</html>
```

The approved elements are `html`, `head`, `title`, `link`, `body`, `div`,
`span`, `section`, `p`, `h1`, `h2`, `h3`, `label`, `button`, `input`, and
static inline `svg`. Inputs support `type="range"` and `type="checkbox"` with
`min`, `max`, `step`, `value`, and `checked`. The author-facing attributes are
`id`, `class`, and those form attributes.

Inline SVG is the approved icon path and uses ordinary markup:

```html
<svg width="16" height="16" viewBox="0 0 16 16">
    <path fill="#15130f" d="M3 2 L14 8 L3 14 Z"/>
</svg>
```

The approved static SVG subset includes `svg`, `g`, `path`, `rect`, `circle`,
`ellipse`, `line`, `polyline`, and `polygon`, with dimensions, `viewBox`,
paths, fills, strokes, and transforms. SVG scripts, animation, navigation, and
network resources are outside the contract.

This is intentionally a strict, script-free subset. Tags must be nested
correctly and void elements should be self-closed. There is no JavaScript,
browser DOM, navigation, network loading, templates, or compatibility path for
`.rml`, `.rcss`, `<rml>`, `text/rcss`, or the `dp` unit.

## Approved CSS subset

Styles use ordinary `.css` files. The supported selector forms are element,
`.class`, `#id`, descendants, comma-separated selectors, and the interaction
states used by the sample (`:hover`, `:active`, `:checked`, and
`:first-child`).

The supported layout and paint surface is deliberately small:

- block, inline, flex, absolute, and fixed layout;
- `width`, `height`, `top`, `right`, `bottom`, `left`, `margin`, and `padding`;
- `box-sizing`, `align-items`, `justify-content`, and the basic flex
  direction/wrapping properties;
- `color`, `background-color`, and solid border widths/colors;
- `font-family`, `font-size`, `font-weight`, `line-height`,
  `letter-spacing`, and `text-align`;
- `overflow-x`, `overflow-y`, `pointer-events`, and `cursor`;
- logical `px`, percentages, unitless numbers, and hexadecimal colors.

Only solid borders are rendered, so the subset makes a nonzero border width
implicitly solid. Prefer the standard longhand declarations `border-width` and
`border-color`; side-specific width and color declarations are supported.
Unsupported browser features are parse errors rather than promises of future
support.

Range inputs and scroll containers expose the existing generated part element
names (`slidertrack`, `sliderprogress`, `sliderbar`, `sliderarrowdec`,
`sliderarrowinc`, and `scrollbarvertical`) to CSS. These names are the one
engine extension in the approved stylesheet subset; authored document markup
still uses standard `input` elements.

Fonts are loaded explicitly through `UISystem::LoadFont`; `font-family` uses
the family name reported by the font.

## Viewer and current limits

The viewer provides a modal start screen, persistent FPS HUD, and runtime
tuning panel under `samples/viewer/ui`. Its Start button emits `start_game`;
fixed-step simulation and captured-look input stay paused until that action
arrives. The HUD adds a centered crosshair and ball-launcher status. Tab
releases the pointer, while F1 opens the tuning panel and releases the pointer.
The viewer does not link or initialize ImGui.

- The OpenGL backend draws `UiFrame`; Vulkan still compiles against the same
  neutral frame boundary but does not draw UI while its general render path is
  incomplete.
- Ordinary HTML image URLs are rejected until they can resolve through the
  cooked `Store` texture path. Icons use approved inline SVG; generated
  FreeType atlases are also supported.
- The facade exposes semantic click/change actions, screen visibility, modal
  focus, fonts, narrow text/form updates, and composition. General DOM access,
  scripting, localization, animation helpers, and game-specific widgets remain
  out of scope.
