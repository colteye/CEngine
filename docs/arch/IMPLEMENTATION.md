# CEngine Implementation Architecture

> **Status: current architecture.** This document describes the small engine
> that exists in the repository now and the seams that new work must preserve.
> [`STATUS.md`](STATUS.md) owns the active initiative and verified gates.
> `CORE.md`, `SYSTEMS.md`, and `NETWORK.md` are target references, not permission
> to pre-build unused abstractions.

## Principles

1. Add a type only when it enforces ownership, identity, wire semantics, or a
   repeated domain invariant.
2. Prefer one direct data path over paired descriptions, update messages,
   registries, or backend mirrors.
3. Cook source data in tools. Runtime code reads validated target assets only.
4. Keep Jolt and graphics API objects behind their owning subsystem boundary.
5. Publish no handle until creation succeeds. Destroying a live reusable slot
   increments its generation.
6. Use ordinary values for append-only indices and serialized table offsets.
7. Do not add deferred threads, queues, allocators, component frameworks,
   render graphs, or configuration switches.

## Runtime composition and frame flow

The application owns the concrete long-lived systems:

```text
Store
WindowSystem
InputSystem
PhysicsSystem
RenderSystem
AudioSystem
UISystem
Scene
```

`Context` is a non-owning composition value passed during lifecycle
calls. Its raw pointers are valid because the application owns every referenced
system for longer than the active scene. An active `Scene` keeps a copy of that
small value for symmetric teardown; it never keeps a pointer to the caller's
context object.

The viewer owns the wall-clock accumulator. It caps a frame at 250 ms, advances
at 60 Hz, performs at most four catch-up ticks, and drops older excess time.
One scene tick is:

```text
Entity::Update
PhysicsSystem::Step(fixed_delta)
Entity::LateUpdate
RenderSystem::Render
```

Kinematic targets and character commands are issued in `Update`. Dynamic body
state is copied to entity transforms in `LateUpdate`. `PhysicsSystem::Step`
advances exactly once and never owns another clock.

## Identity, handles, references, and pointers

The representation follows lifetime:

| Value | Representation | Reason |
| --- | --- | --- |
| reusable entity, mesh-instance, light, body, constraint, character, or audio-voice slot | tagged 64-bit `Handle<Tag>` packing a 32-bit index and generation | stale references must fail after slot reuse and unrelated handle domains must not mix |
| append-only input action | tagged `ActionHandle` with a fixed generation | actions share the typed handle representation but are never removed or reused |
| serialized entity or asset table position | `uint32_t` | it is a file-local index, not a live runtime identity |
| cooked asset identity | validated `Reference{path, guid, type}` | immutable assets do not need mutable runtime handles |
| immutable decoded asset ownership | `shared_ptr<const T>` | scenes and render records may safely share one decoded value |
| backend lookup key | raw `const T*` | call-scoped or backend-private observation of an asset retained by a public `shared_ptr` owner |
| subsystem composition | raw pointer in `Context` | explicitly non-owning and bounded by application lifetime |
| compile-selected subsystem backend | `unique_ptr<I*Backend>` | keeps window, graphics, input, physics, and audio implementations replaceable without runtime registries |

Do not introduce an asset handle in parallel with `Reference` and
`shared_ptr`. Do not use a bare integer for a reusable slot. Do not place
owning raw pointers in public records.

## Assets and scenes

`Store` has two jobs that justify its existence:

- resolve project-relative references and enforce their declared type and
  per-path identity;
- cache immutable decoded material, mesh, texture, collision, skeleton,
  animation, composition, particle, and standard-audio values.

It is not an asset database, dependency graph, mutable registry, or second
lifetime system. Each loader accepts one concrete target file and produces one
engine value. Cache maps are typed; no variant-like record with unrelated
nullable fields exists. Engine container files validate their embedded GUID and
type; standard external payloads such as DDS validate their own format while
the store still rejects conflicting GUIDs for the same resolved path. Failed
decodes are not published.

Every engine container uses one version-one envelope and a generated owned
payload from `schemas/engine.game.json`. `Reader` owns bounded little-endian
primitives; generated `Wire::Read` functions own layout and structural
validation. Python uses the same flattened schema through one generic
packer/unpacker. Handwritten `*_asset.cpp` files contain only semantic
validation and construction. DDS, WAV, FLAC, MP3, and Ogg/Vorbis remain
standard external files.

`.cscene` contains generated settings, references, entity rows, generated class
records, auxiliary asset indices, and connections. The same schema generator
owns entity property layouts. The supported field vocabulary includes semantic
`asset`, `asset_list`, and `entity` references; tooling validates those
references before writing.

The engine-owned library is the 15 concrete classes documented in
[`../entity_library.md`](../entity_library.md). Entity schemas declare their
supported inputs and outputs. Connections carry caller/activator identity,
parameter, simulation-time delay, and optional fire count; the scene dispatch
queue is bounded and deterministic. Physics bodies and characters register
their entity owner so normalized contacts reach trigger/collider behavior on
the simulation thread.

`Scene` owns entity objects in generation-checked slots. Native entity classes
inherit their generated property record directly, so there is no parallel
handwritten property type. Activation initializes in scene order and rolls
back initialized entities in reverse order on failure. Shutdown is symmetric.
Games create live entities through `Scene::CreateEntity`; constructor
arguments establish runtime identity and the initial transform before an
active scene calls `Initialize`. Player spawn points and their selection
policy remain game-owned because team, player-type, checkpoint, and authority
semantics are not engine concepts.

## Rendering

`Mesh` means immutable indexed geometry:

```text
Mesh
  lods: MeshLod[]
    screen_size
    vertices: MeshVertex[]
    indices: uint32[]
  local_bounds
  has_lightmap_uv
```

`MeshInstance` means one retained placement of a mesh with a material, optional
lightmap binding, transform, and presentation flags. The name is intentionally
specific. Particles, decals, terrain, text, and other future presentation forms
are not “mesh instances” and should receive their own narrow paths when they
exist.

There is no `Renderable`, `RenderableDesc`, or `RenderableUpdate`. A
`MeshInstance` is the single creation record and retained state. Transform and
flag changes use direct arguments. `RenderSystem` computes and owns world
bounds, revisions, and reusable slots.

Mesh instances retain immutable mesh/material/texture assets with
`shared_ptr<const T>`. Backend draw items may use raw pointers only because the
corresponding retained mesh instance outlives them. OpenGL uploads one AoS
vertex buffer and one index buffer per shared mesh and reference-counts its
private GPU resources. Draws use `glDrawElements`.

The compiled `IRenderBackend` boundary exists because OpenGL and Vulkan have
different resource implementations. It is not a runtime plugin registry.
Vulkan remains an incomplete compiled alternative.

OpenGL implementation types live in `CEngine::Renderer::OpenGL`; the backend
directory therefore uses local names such as `RenderBackend`, `ShadowSystem`,
and `ShadowBuffer` without repeating `OpenGL` in every filename and type.
Directional cascade fitting is part of `shadow_system.h/.cpp`, not a parallel
subsystem.

The OpenGL image pipeline is linear Rec. 709 from material sampling through
lighting, fog, bloom, transparency, and HDR scene composition. Base-color
textures decode from sRGB; normal, material-data, lightmap, and environment
textures remain linear. Exposure is applied once immediately before Khronos
PBR Neutral tone mapping at presentation, the sRGB framebuffer performs the
output transfer, and premultiplied sRGBA UI inputs convert to linear before
blending.

## Physics

`PhysicsSystem` is the engine-facing physics facade and owns one
compile-selected `IPhysicsBackend`. `JoltPhysicsBackend` is the current concrete
implementation. Jolt classes, body IDs, layers, listeners, allocators, and
constraints remain in `jolt_physics_backend.cpp`. This is an ordinary compiled
backend boundary like rendering and input, not a runtime plugin registry.

Public collision data is `PhysicsShape`, an engine-neutral tree supporting:

- box, sphere, capsule, cylinder, and static plane;
- convex hull;
- static triangle mesh and height field;
- compound shapes with local child transforms.

Movable triangle meshes, height fields, and planes are rejected, including when
nested inside compounds. Blender cooks a distinct `.cphys`; runtime physics
never interprets `.cmesh`.

Bodies support static, dynamic, kinematic, and sensor behavior; mass, friction,
restitution, damping, gravity factor, initial velocities, sleeping, CCD,
six-axis locks, collision layers, teleport, kinematic movement, velocity,
forces, torque, impulses, activation, and state readback.

Queries are layer-filtered and bounded: closest/all ray casts, convex shape
casts, and overlaps. Contact and trigger events are collected in a bounded
buffer and normalized to engine handles. The layer matrix is symmetric.

Constraints support fixed, point/ball, hinge, slider, distance/spring, and cone
forms. Hinge and slider motors support velocity and position targets. Constraint
and body destruction are symmetric; destroying a body also destroys attached
constraints.

The virtual character path supports Z-up capsules, slopes, stairs, floor
sticking, gravity, grounding, moving-ground velocity, penetration recovery, and
height changes. Its inner body gives ordinary queries and sensor contacts a
generation-checked character identity. The viewer player registers that
character with its scene entity and falls back to transform movement in tests
or tools without physics.

## Input

`InputSystem` owns one platform backend and maps device state to append-only
actions. `Key` covers the standard keyboard set and the SDL3 backend maps it to
scancodes. It also publishes client-frame pointer, key, wheel, and text events
in platform-neutral values. Game and UI code consume engine types; SDL values
do not cross the backend.

## UI

`UISystem` is the engine-facing in-game UI facade and is available through
`Context` like the other application-owned systems. Screen lifetime uses
generation-checked handles. Element clicks and form changes become semantic
`UiEvent` actions; narrow text, numeric-value, and checked-state setters support
live HUDs and game-owned tuning. Game code does not receive DOM pointers or
RmlUi callbacks.

RmlUi is a private implementation under `src/ui/rmlui`, not a renderer backend
and not a public engine dependency. It consumes one ordered normalized
`InputSystem` event stream; pointer positions are converted once from logical
window coordinates to full drawable pixels. RmlUi retains hover, focus, and
pressed state across event-free frames, owns RML/RCSS layout and FreeType atlas
generation, then emits one renderer-neutral `UiFrame`. `RenderSystem` owns the
submitted frame. The compiled graphics backend draws it as the final 2D pass;
OpenGL implements that pass now. See [`../ui.md`](../ui.md) for the exact API
and current limits.

## Window and platform

`WindowSystem` owns one `IWindowBackend`. The SDL3 backend owns video subsystem
lifetime, event pumping, a high-pixel-density resizable window, OpenGL context
or Vulkan surface prerequisites, drawable sizing, presentation, and monotonic
time. Renderer initialization consumes `WindowSystem`, not an SDL window.

Opaque native access exists only for compiled integrations such as Vulkan
surface creation. Game/entity code and the sample's ordinary control flow use
CEngine window values.

SDL3 is the sole platform/device dependency. Its fetched static build enables
video, events, the selected graphics surface, and optional audio while disabling
unused GPU-renderer, camera, joystick, haptic, HID, power, sensor, dialog, and
tray subsystems.

## Audio

`AudioSystem` owns one `IAudioBackend` and generation-checked voice slots.
CEngine owns validation, voice capacity, priority/age stealing, buses, and
diagnostics. The miniaudio backend decodes and mixes without its device layer;
an SDL3 playback stream pulls the final stereo floating-point mix.

The current path supports cached effects, streamed music/ambience, 2D and 3D
voices, listener/source velocity, attenuation models, rolloff, Doppler, cones,
gain/pitch/fades, looping, pause/resume/seek, music/effects/dialog buses,
sound-effects environment reverb, and obstruction/occlusion filtering. See
[`../audio_requirements.md`](../audio_requirements.md) for the exact support
matrix and exclusions.

## Extension rules

- Add a collider kind only with Blender cooking, `.cphys` validation, Jolt
  construction, malformed-input coverage, and a focused runtime test.
- Add a schema field only when runtime and authoring both consume it.
- Add a handle only for a reusable mutable slot with independent lifetime.
- Add a renderer presentation kind beside `MeshInstance`, not inside a generic
  renderable union.
- Add an asset cache only to `Store` and keep it typed.
- Keep backend resources private and derived from engine-owned immutable values.

## Document routing

- [`STATUS.md`](STATUS.md): current initiative, inventory, remaining work, and
  verified commands.
- [`../animation_requirements.md`](../animation_requirements.md): implemented
  backend-neutral animation assets, Ozz backend boundary, playback ownership,
  renderer skinning, and verification contract.
- [`../physics_requirements.md`](../physics_requirements.md): physics product
  contract and support matrix.
- [`../audio_requirements.md`](../audio_requirements.md): audio product contract
  and support matrix.
- [`../entity_library.md`](../entity_library.md): implemented base entity set,
  authoring mapping, and deliberate exclusions.
- [`DELIVERY.md`](DELIVERY.md): historical milestone/performance references.
- `CORE.md`, `SYSTEMS.md`, `NETWORK.md`: complete target design references.
