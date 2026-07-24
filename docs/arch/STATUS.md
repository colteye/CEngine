# Architecture Implementation Status

> **Status: current implementation inventory, verified 2026-07-23.**
> Historical M0 step plans are obsolete and are not an implementation
> checklist.

## Current initiative result

Standard 3D physics is implemented across Jolt, cooked assets, scenes, and the
Blender add-on. Windowing and input have moved from GLFW to engine-owned SDL3
backends, and the engine now has a complete SDL3/miniaudio playback path.
Cooked asset payloads use one schema-owned version-one format.

### Base entities and scene logic

- The engine library contains 15 concrete classes: prop, collider, trigger
  volume, physics constraint, light, camera, audio source/environment, skybox,
  fog, post process, relay, timer, auto, and marker.
- Entity schemas declare validated inputs and outputs. Scene connections carry
  an optional parameter, simulation-time delay, and fire count; dispatch is
  deterministic and bounded.
- Scene physics ownership routes body and virtual-character contacts to
  collider and trigger behavior after the physics step.
- The active view is an enabled camera/player entity and drives both renderer
  camera state and the audio listener.
- Blender uses native Mesh, Light, Camera, and Speaker objects, cooks
  collider/trigger meshes only as physics, authors output connections, exports
  passthrough audio, and serializes its active camera.
- The viewer owns `player`, `player_spawn`, and a concrete game coordinator.
  The same join/respawn path supports one local player or multiple live player
  records; spawn filtering uses game-owned team/group/priority and clearance
  metadata.
- The complete rationale and deliberate exclusions are in
  [`../entity_library.md`](../entity_library.md).

### Window and platform

- `WindowSystem` owns one replaceable `IWindowBackend`.
- The SDL3 backend owns window/video lifetime, event pumping, drawable sizing,
  OpenGL context setup, Vulkan surface prerequisites, presentation, and time.
- Render backends initialize from `WindowSystem`; no window-library type is in
  the renderer facade.
- The viewer's window, input, game UI, FPS HUD, and runtime tuning flow use
  engine facades; it no longer has an ImGui platform or renderer adapter.
- The pinned SDL3 static build enables video, events, the selected graphics
  surface, and optional audio; unused subsystems are disabled.

## Current implementation

### Rendering

- `Mesh` is immutable indexed geometry with ordered LODs, AoS `MeshVertex`
  data, skin weights, local bounds, and screen-size selection.
- `MeshInstance` is the only retained mesh placement record. It contains shared
  immutable mesh/material/texture ownership, transform, lightmap binding, and
  flags.
- `Renderable`, `RenderableDesc`, `RenderableUpdate`, `MeshData`, parallel
  vertex arrays, and the old mesh source file are gone.
- `RenderSystem` owns instance/light generational slots, derived world bounds,
  and revisions.
- OpenGL shares/refcounts GPU mesh, material, and lightmap resources and draws
  indexed geometry.
- OpenGL owns backend-private skinning palette texture buffers and applies the
  same four-weight matrix skinning in deferred, forward, depth, and point
  shadow passes. Vulkan explicitly rejects skinning while its mesh path remains
  incomplete.
- OpenGL treats base-color textures as sRGB and material-data, lightmap, and HDR
  textures as linear. Lighting and scene composition remain linear in
  floating-point targets; Khronos PBR Neutral tone mapping runs once before
  hardware sRGB output, and UI blends in linear display space.
- OpenGL implementation types live under `Renderer::OpenGL`; cascade fitting
  and shadow resource management are consolidated in `ShadowSystem`.
- `.cparticle` is a separate generated asset type; a renderer simulation path
  remains separate from meshes.
- Vulkan still compiles as an incomplete backend; it does not yet implement the
  full mesh rendering path.

### Assets and schemas

- `Store` replaced `AssetDatabase`, `AssetHandle`, and the longer
  `AssetStore`/`AssetReference` names.
- Cooked assets use validated `Reference` values and
  `shared_ptr<const T>` ownership.
- Typed caches directly hold every decoded runtime asset: materials, meshes,
  texture orientations, collision, skeletons, animations, compositions, and
  particles. Standard WAV, FLAC, MP3, and Ogg/Vorbis audio is loaded directly.
- `schemas/engine.game.json` owns every cooked payload record. The generator
  emits owned `Wire::*` values and complete C++ readers; Python exports all
  payloads through one generic schema packer.
- Handwritten disk records, `asset_io`, `binary.h`, payload-specific readers,
  and compatibility versions are gone.
- Semantic schema fields include assets, asset lists, and scene entity
  references.
- Blender mesh cooking deduplicates complete packed vertices and emits a real
  index buffer.
- Blender exports skeletons, animations, physics, compositions, scenes, and
  simplified particle settings through the same schema path.
- Skeleton and animation assets contain only engine records: canonical
  hierarchy/rest/inverse-bind data and evaluated local TRS tracks/events. No
  Ozz type or archive is part of `CEngineAssets` or the exported format.
- Sponza scene, material, and mesh files are recooked to the new format; its
  existing `Sponza_0.dds` lightmap is preserved.

### Identity and pointers

- Reusable entity, renderer, physics-body, constraint, and character slots use
  tagged 64-bit handles packing a 32-bit index and 32-bit generation.
- Append-only input actions use the shared tagged handle representation with a
  fixed generation.
- Serialized indices remain plain `uint32_t`.
- Immutable assets do not have runtime handles.
- Public mesh instances retain resources with shared ownership. Backend raw
  pointers are private observations whose owner is the retained record.
- `Context` pointers are non-owning lifecycle-scoped composition only.

### Physics runtime

- `PhysicsSystem` owns one compile-selected `IPhysicsBackend`. Jolt is the
  current concrete backend; its classes and opaque IDs stay inside
  `JoltPhysicsBackend`.
- The caller owns the fixed-step accumulator; `Step` advances exactly once.
- Shapes: box, sphere, capsule, cylinder, plane, convex hull, triangle mesh,
  native height field, and nested compound.
- Bodies: static, dynamic, kinematic, sensors, mass/material coefficients,
  damping, gravity factor, velocities, sleep, CCD, locks, and 32 collision
  layers.
- Commands: teleport, kinematic target, velocity, force-at-point, torque,
  impulse-at-point, gravity factor, sensor toggle, and activation.
- Queries: closest/all ray, convex shape cast, and bounded unique overlap with
  masks and ignored body.
- Events: bounded begin/persist/end contact and trigger records with dropped
  counts.
- Constraints: fixed, point/ball, hinge, slider, distance/spring, and cone;
  hinge/slider velocity and position motors.
- Character: virtual Z-up capsule with slopes, steps, grounding, floor sticking,
  moving-ground state, penetration recovery, crouch height, body/character
  query identity, and trigger contact routing.
- Handles reject stale slots; body destruction removes attached constraints.

### Physics assets, scenes, and Blender

- `.cphys` is a deterministic engine-neutral format with strict envelope,
  range, finite-value, topology, hierarchy, and shape-specific validation.
- `prop` schema version 1 carries an optional collision asset, explicit motion,
  body parameters, collision layer, sensor/CCD/sleep flags, and axis locks.
- Blender cooks primitives, planes, convex hulls, triangle meshes, height
  fields, and compounds. Movable concave/infinite geometry is rejected.
- An explicit collision-only mesh may be parented to a prop; it is cooked with
  a local transform and excluded from visible mesh export.
- The add-on exposes schema-driven body settings, collider selection, collision
  object selection, collider wire/bounds preview, and validation.
- `physics_constraint` entities resolve Blender object names to semantic scene
  references and create/destroy Jolt constraints at runtime.
- The viewer player uses the Jolt character controller and Space jump action.

### Input

- `Key` and the SDL3 scancode mapping include punctuation, digits, A-Z,
  navigation, locks, F1-F24, keypad keys, modifiers, super keys, world keys,
  and menu. The legacy F25 value safely remains unmapped because SDL3 has no
  corresponding scancode.
- The SDL3 adapter also normalizes pointer motion/buttons/wheel, key
  transitions, window leave, and text input at client-frame cadence without
  exposing SDL types.

### UI

- `UISystem` owns generation-checked screen handles, font loading, modal
  visibility, semantic click/change bindings, narrow text/form updates,
  display-frame updates, and event draining.
- RmlUi 6.2 and FreeType 2.14.3 are pinned to immutable revisions and remain
  private implementation dependencies. Lua, SVG, Lottie, and dependency
  samples are disabled.
- A read-only content-rooted file adapter rejects absolute paths and parent
  traversal. RmlUi focus, hit testing, layout, and generated font atlases feed
  a renderer-neutral `UiFrame`.
- RmlUi composes at full drawable resolution with a density-independent layout
  ratio. OpenGL draws `UiFrame` last with generated texture caching and
  premultiplied alpha; RmlUi types do not cross the renderer boundary.
- UI pointer input has one authority: ordered normalized SDL events. Motion,
  button, and wheel events carry logical-window coordinates which are converted
  once at the RmlUi boundary; pressed state remains inside RmlUi across frames.
- The viewer owns a modal start menu, a persistent FPS HUD, and an RmlUi
  runtime tuning panel. ImGui is no longer linked into the viewer.

### Audio

- `AudioSystem` owns a replaceable `IAudioBackend` and generation-checked voice
  slots with deterministic capacity, priority, and age policy.
- SDL3 owns the platform device and output stream. Miniaudio is compiled without
  device I/O and owns decoding, mixing, sample-rate conversion, effects, and
  3D spatialization.
- Playback supports cached effects and streamed music/ambience, 2D/3D voices,
  attenuation, rolloff, Doppler, cones, velocity, gain, pitch, loops, fades,
  pause/resume/seek, and cursor queries.
- Master/music/effects/dialog buses support gain, mute, and fades.
- Sound effects have environment reverb and per-voice obstruction/occlusion
  gain plus low-pass filtering.
- Optional device failure degrades to a silent facade; required startup fails
  explicitly. Diagnostics expose device and voice lifecycle outcomes.
- The vendored audio footprint is limited to miniaudio, stb_vorbis, and the
  miniaudio reverb node. SDL_mixer is not included.

### Animation

- `AnimationSystem` owns generation-checked instances, playback cursors,
  pause/loop/rate policy, interrupted cross-fades, cosmetic events, budgets,
  and diagnostics.
- `IAnimationBackend` is the engine boundary for runtime representation and
  pose evaluation. The current Ozz backend alone includes Ozz headers and
  converts backend-neutral skeleton/track assets into private runtime objects.
- The application owns animation beside assets, physics, audio, and rendering;
  scene evaluation runs after physics and before entity `LateUpdate`.
- A skinned `prop` binds one mesh, skeleton, and default animation by GUID,
  creates an animation instance, and uploads its evaluated palette through the
  renderer facade.
- Blender evaluates armature actions and exports complete canonical local TRS
  tracks. Scene export binds a mesh armature to its `.cskel` and deterministic
  default `.canim`; no separate animation processor or interchange JSON exists.
- Ozz 0.16.0 is pinned to an immutable commit and is private to
  `src/animation/ozz`.

## Known limits

- The first animation slice selects one default clip per authored prop. Game
  animation graphs, root motion, retargeting, additive/partial layers, IK,
  worker evaluation, and animation LOD remain deferred.
- Constraint break thresholds are not implemented.
- Constraint authoring uses world-space anchors; a richer local-frame editing
  widget is not implemented.
- Physics diagnostics expose counts and event overflow, but not timing,
  high-water, active-body, or shape-memory telemetry yet.
- Collision asset deduplication is path/object based, not content-hash based.
- Static/dynamic friction and restitution are per body rather than shared
  physics-material assets.
- Character crouch exists in the runtime API; the viewer has no crouch action.
- There is no debug-draw bridge from Jolt into the renderer.
- SDL3 gamepad/touch bindings and mobile lifecycle interruption policy are not
  implemented yet; the input/platform backend seams do not expose desktop
  types to game code.
- Mobile and console target packages and graphics backends are not currently
  built in CI. Console SDL packages remain platform-SDK inputs.
- Audio has one listener and one global sound-effects environment.
- Vulkan accepts the neutral UI frame at the `RenderSystem` boundary but does
  not draw it while that backend's general mesh path remains incomplete.
- RML external image URLs are rejected until they can resolve through cooked
  `Store` textures. The UI facade exposes semantic click/change actions and
  narrow text/form updates, not scripting or a browser DOM.

These are explicit limits, not placeholder interfaces.

## Verification

The animation slice was verified on 2026-07-23 with:

```sh
cmake --build --preset mac-debug -j 6
ctest --test-dir build/mac-debug --output-on-failure
python3 -m unittest discover tools/ceasset/tests
git diff --check
```

- the neutral asset, `AnimationSystem`, Ozz backend, scene prop, and renderer
  integration translation units compile;
- the focused animation executable converts generated raw engine assets,
  samples the expected midpoint palette, emits a marker, pauses, cross-fades,
  destroys its slot, and rejects its stale handle;
- `python3 -m unittest discover -s tools/ceasset/tests`: 131 tests passed with
  one intentional skip;
- `git diff --check`: passed;
- source searches confirm that Ozz appears only in its backend and build/docs,
  with no Ozz data or type in assets, schemas, Blender export, scenes, or
  rendering.

The UI/input slice and repository-wide gate were verified on 2026-07-23 with:

```sh
cmake --preset mac-debug
cmake --build --preset mac-debug -j 8
ctest --test-dir build/mac-debug --output-on-failure
python3 -m unittest discover -s tools/ceasset/tests
./build/mac-debug/CEngineUiRendererTests
git diff --check
```

- all 15 CTest cases passed; the OpenGL UI renderer test is skipped in the
  sandbox when a graphics display is unavailable;
- the same OpenGL test passed separately with a hidden native context and
  exact textured-pixel readback;
- the UI lifecycle test composes at 2x drawable density; verifies click holds
  across event-free frames, inside/outside release rules, range and checkbox
  payloads, unloading, and stale-handle rejection;
- all 131 Python asset tests passed with one intentional skip.
