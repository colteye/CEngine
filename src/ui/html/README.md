# CEngine HTML runtime

This directory is the engine-owned HTML/CSS parser, layout, interaction, and
geometry implementation used directly by `UISystem`. It is derived from the
RmlUi 6.2 Core sources at commit
`2230d1a6e8e0848ed87a5761e2a5160b2a175ba4`; the original MIT license is in
`LICENSE.txt`.

CEngine intentionally exposes a smaller authoring contract than the inherited
implementation:

- screens are `.html` documents rooted at `<html>`;
- external styles use `<link rel="stylesheet" href="file.css" />`;
- CSS `px` values are logical pixels and scale to drawable pixels;
- the former `.rml`, `.rcss`, `<rml>`, `text/rcss`, and `dp` authoring forms
  are not supported by CEngine.

The RmlUi namespace and some historical RML/RCSS names remain inside the
derived implementation so its provenance and future source-level comparisons
stay clear. They are not part of CEngine's public API or content format.
