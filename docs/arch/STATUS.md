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
- The viewer's ordinary window/event loop uses the engine facade. Only its
  explicit ImGui adapter sees opaque SDL event/window values.
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

## Known limits

- Skeleton and animation assets cook and load, and meshes carry skin weights,
  but there is no runtime pose evaluation, cross-fade, cosmetic-event, or GPU
  skinning path yet. The proposed design is
  [`../animation_requirements.md`](../animation_requirements.md).
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

These are explicit limits, not placeholder interfaces.

## Verification

The complete gate passed on 2026-07-23:

```sh
cmake --build --preset mac-debug -j 6
ctest --test-dir build/mac-debug --output-on-failure
python3 -m unittest discover tools/ceasset/tests
git diff --check
```

- `cmake --build --preset mac-debug -j 6`: passed;
- `ctest --test-dir build/mac-debug --output-on-failure`: 12/12 passed;
- an audio-disabled OpenGL viewer build passed with SDL3 audio off and no
  miniaudio sources;
- a Vulkan-selected viewer compile/link check passed through SDL3's Vulkan
  surface path;
- `python3 -m unittest discover tools/ceasset/tests`: 128 tests passed with one
  intentional skip;
- Blender 5.2 background registration created every engine and viewer entity,
  then authored a source-to-target connection through the add-on operators;
- `git diff --check`: passed;
- both game-schema JSON files parse successfully;
- all standard concrete `Key` values have an SDL3 scancode mapping; F25 is the
  sole intentionally unmapped legacy value;
- source searches contain no live old renderable, asset-database, public
  physics-backend, legacy box-physics, or parallel property APIs.
