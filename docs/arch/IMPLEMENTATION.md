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
AssetStore
InputSystem
PhysicsSystem
RenderSystem
Scene
```

`EngineContext` is a non-owning composition value passed during lifecycle
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
| reusable entity, mesh-instance, light, body, constraint, character slot | tagged `SlotHandle<Tag>{index, generation}` | stale references must fail after slot reuse and unrelated handle domains must not mix |
| append-only input action | `ActionId{index}` | actions are never removed or reused |
| serialized entity or asset table position | `uint32_t` | it is a file-local index, not a live runtime identity |
| cooked asset identity | validated `AssetReference{path, guid, type}` | immutable assets do not need mutable runtime handles |
| immutable decoded asset ownership | `shared_ptr<const T>` | scenes and render records may safely share one decoded value |
| backend lookup key | raw `const T*` | call-scoped or backend-private observation of an asset retained by a public `shared_ptr` owner |
| subsystem composition | raw pointer in `EngineContext` | explicitly non-owning and bounded by application lifetime |
| private implementation | `unique_ptr<Impl>` | hides Jolt and other heavy implementation details |

Do not introduce an asset handle in parallel with `AssetReference` and
`shared_ptr`. Do not use a bare integer for a reusable slot. Do not place
owning raw pointers in public records.

## Assets and scenes

`AssetStore` has two jobs that justify its existence:

- resolve project-relative references and enforce their declared type and
  per-path identity;
- cache immutable decoded material, mesh, texture, and collision values.

It is not an asset database, dependency graph, mutable registry, or second
lifetime system. Each loader accepts one concrete target file and produces one
engine value. Cache maps are typed; no variant-like record with unrelated
nullable fields exists. Engine container files validate their embedded GUID and
type; standard external payloads such as DDS validate their own format while
the store still rejects conflicting GUIDs for the same resolved path. Failed
decodes are not published.

`.cscene` stores fixed tables of asset references, entities, generated class
records, auxiliary asset indices, settings, and connections. The schema
generator owns entity field layouts and C++ readers. The supported field
vocabulary includes semantic `asset`, `asset_list`, and `entity` references;
tooling validates those references before writing.

`Scene` owns entity objects in generation-checked slots. Native entity classes
inherit their generated property record directly, so there is no parallel
handwritten property type. Activation initializes in scene order and rolls
back initialized entities in reverse order on failure. Shutdown is symmetric.

## Rendering

`Mesh` means immutable indexed geometry:

```text
Mesh
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

## Physics

`PhysicsSystem` is the only engine-facing physics object. Jolt classes, body
IDs, layers, listeners, allocators, and constraints remain in
`jolt_physics_system.cpp`.

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
height changes. The viewer player uses this path when physics is available and
falls back to transform movement in tests or tools without physics.

## Input

`InputSystem` owns one platform backend and maps device state to append-only
actions. `Key` covers every standard GLFW keyboard key. Game code consumes
`ActionId` values; device-specific GLFW values do not cross the backend.

## Extension rules

- Add a collider kind only with Blender cooking, `.cphys` validation, Jolt
  construction, malformed-input coverage, and a focused runtime test.
- Add a schema field only when runtime and authoring both consume it.
- Add a handle only for a reusable mutable slot with independent lifetime.
- Add a renderer presentation kind beside `MeshInstance`, not inside a generic
  renderable union.
- Add an asset cache only to `AssetStore` and keep it typed.
- Keep backend resources private and derived from engine-owned immutable values.

## Document routing

- [`STATUS.md`](STATUS.md): current initiative, inventory, remaining work, and
  verified commands.
- [`../physics_requirements.md`](../physics_requirements.md): physics product
  contract and support matrix.
- [`DELIVERY.md`](DELIVERY.md): historical milestone/performance references.
- `CORE.md`, `SYSTEMS.md`, `NETWORK.md`: complete target design references.
