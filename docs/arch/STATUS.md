# Architecture Implementation Status

> **Status: current implementation inventory, verified 2026-07-23.**
> Historical M0 step plans are obsolete and are not an implementation
> checklist.

## Current initiative result

Standard 3D physics is implemented across Jolt, cooked assets, scenes, and the
Blender add-on. The same change set completed the rendering, asset, input, and
identity cleanup described below.

## Current implementation

### Rendering

- `Mesh` is immutable indexed geometry with AoS `MeshVertex` data and local
  bounds.
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
- Particles will be a separate presentation path, not a generic mesh/renderable
  variant.
- Vulkan still compiles as an incomplete backend; it does not yet implement the
  full mesh rendering path.

### Assets and schemas

- `AssetStore` replaced `AssetDatabase` and `AssetHandle`.
- Cooked assets use validated `AssetReference` values and
  `shared_ptr<const T>` ownership.
- Typed caches directly hold materials, meshes, texture orientations, and
  collision shapes; the old variant-like cached record is gone.
- Scene entity properties are generated from game schemas. Handwritten duplicate
  prop/light/skybox property records and flag types are gone.
- Semantic schema fields include assets, asset lists, and scene entity
  references.
- Blender mesh cooking deduplicates complete packed vertices and emits a real
  index buffer.

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
  moving-ground state, penetration recovery, and crouch height.
- Handles reject stale slots; body destruction removes attached constraints.

### Physics assets, scenes, and Blender

- `.cphys` is a deterministic engine-neutral format with strict envelope,
  range, finite-value, topology, hierarchy, and shape-specific validation.
- `prop` schema version 2 carries an optional collision asset, explicit motion,
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

- `Key` and the GLFW mapping include punctuation, digits, A-Z, navigation,
  locks, F1-F25, keypad keys, modifiers, super keys, world keys, and menu.

## Known limits

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
- `ctest --test-dir build/mac-debug --output-on-failure`: 11/11 passed;
- `python3 -m unittest discover tools/ceasset/tests`: 121 tests passed with one
  intentional skip;
- `git diff --check`: passed;
- both game-schema JSON files parse successfully;
- all 120 concrete `Key` values have a GLFW mapping;
- source searches contain no live old renderable, asset-database, public
  physics-backend, legacy box-physics, or parallel property APIs.
