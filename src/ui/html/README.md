# CEngine HTML runtime

This directory is the engine-owned HTML/CSS parser, layout, interaction,
geometry, and SVG integration used directly by `UISystem`. It is derived from
the RmlUi 6.2 Core and SVG sources at commit
`2230d1a6e8e0848ed87a5761e2a5160b2a175ba4`; the original MIT license is in
`LICENSE.txt`.

CEngine intentionally exposes a smaller authoring contract than the inherited
implementation:

- screens are `.html` documents rooted at `<html>`;
- external styles use `<link rel="stylesheet" href="file.css" />`;
- static inline SVG is supported for icons through ordinary `<svg>` markup;
- CSS `px` values are logical pixels and scale to drawable pixels;
- the former `.rml`, `.rcss`, `<rml>`, `text/rcss`, and `dp` authoring forms
  are not supported by CEngine.

The RmlUi namespace and some historical RML/RCSS names remain inside the
derived implementation so its provenance and future source-level comparisons
stay clear. They are not part of CEngine's public API or content format.
LunaSVG 3.0.0 is pinned as the SVG module's low-level rasterization dependency.
The only compatibility change in the copied SVG source is its direct
`Bitmap::isNull()` check for that API.
