# CEngine Implementation Architecture

> **Status: primary implementation contract.** Read this after `STATUS.md`. This
> document defines what an implementation agent should build now, the order in
> which capabilities are promoted, and the stable seams that allow the detailed
> target architecture to be introduced later. `CORE.md`, `SYSTEMS.md`, and
> `NETWORK.md` preserve the full target design; they do not expand the active
> milestone by themselves.

[Architecture implementation status](STATUS.md) identifies the active milestone,
active vertical step, current code inventory, and next automated gate. Read it
before selecting work from this document.

## 1. Objective

CEngine first proves one small, playable, measurable 3D game path. It grows by
replacing simple implementations behind stable data boundaries, not by creating
placeholder frameworks for possible future features.

The initial path is:

```text
authored room + player input
  -> synchronous cooked-asset load
  -> authoritative fixed-tick simulation
  -> Jolt physics
  -> complete presentation snapshot
  -> retained main-thread renderer
  -> displayed frame
```

This path deliberately preserves the authority/presentation boundary needed by
future multiplayer without initially implementing remote transport, delta
replication, prediction, paired client entity classes, or interest management.

## 2. Rules for implementation agents

1. Implement only the active milestone and its exit tests.
2. Prefer a direct concrete type over a public framework or interface hierarchy.
3. Preserve the stable contracts in section 5; internal representations are free
   to change.
4. Do not implement a deferred feature because a detailed document describes its
   eventual contract.
5. Do not create empty abstractions, configuration flags, factories, queues, or
   serialized fields for deferred features.
6. Keep one writer for mutable state during a named phase.
7. Move backend-specific data through opaque handles or packed values, never
   cross-owner pointers.
8. Add complexity only through the promotion evidence in
   `DELIVERY.md#tradeoffs-and-promotion`.
9. Keep the current milestone running and measurable after every vertical change.
10. When code and this document disagree, either correct the code or update this
    document deliberately in the same change.

## 3. Initial composition

```text
EngineApp
  AssetStore
    complete immutable cooked assets

  ServerSimulation
    entity slots
    fixed-tick clock
    scheduled behavior and deferred structural changes
    authoritative game state

  PhysicsWorld
    Jolt bodies, shapes, queries, and contacts

  ClientView
    presentation records keyed by stable presentation identity
    no authoritative behavior

  Renderer
    retained render objects
    fixed passes
    one compiled graphics backend
```

Execution initially uses the platform/main thread. Jolt may use backend-private
workers that join before `Step` returns. No engine Render Thread, job system,
network thread, or asset worker exists in the initial milestone.

## 4. Initial frame and tick flow

The outer frame performs the following work in order:

1. Pump platform events.
2. Normalize device input into `PendingInput`.
3. Accumulate real time in the fixed-tick clock.
4. Advance zero or more authoritative ticks, with a default limit of four ticks
   per outer frame.
5. For each tick:
   1. build exactly one bounded `UserCommand` addressed to that tick;
   2. apply the command;
   3. commit pending spawn/destruction;
   4. run scheduled entity behavior;
   5. apply physics commands;
   6. step Jolt once with the fixed delta;
   7. apply body states and contact/trigger events;
   8. run post-physics behavior;
   9. commit pending structural changes.
6. Capture one complete presentation snapshot from stable post-tick state.
7. Apply the snapshot to `ClientView`.
8. Update retained render records and render one frame.

`PendingInput` has three field classes:

- **continuous:** latest absolute value, such as a movement axis;
- **held:** latest pressed/released state, such as crouch or fire-held;
- **impulse:** accumulated edge or relative value, such as jump-pressed,
  interact-pressed, or mouse delta.

If an outer frame runs no tick, impulse input remains pending. The first tick in
an outer frame consumes accumulated impulses and copies continuous/held values.
Additional catch-up ticks copy continuous/held values but receive zero impulse
fields. An impulse is therefore neither lost nor repeated. Recorded replay stores
the resulting `UserCommand` sequence, not `PendingInput` or platform events.

If four catch-up ticks do not make the M0 local simulation current, the clock
records overload, discards all but less than one tick of remaining wall-clock
debt, and continues with the fixed delta. It never increases the tick delta. M2
defines a dedicated-server lateness policy rather than silently inheriting this
local-overload behavior.

The authoritative simulation never reads renderer state. The renderer never
reads entity pointers. The snapshot can initially be an in-memory packed value;
it does not require a transport abstraction or byte serialization.

## 5. Stable contracts

These semantics are expensive to change and are preserved from the first
milestone:

- right-handed coordinates, `+X` forward, `+Y` left, `+Z` up;
- meters, radians, and seconds;
- fixed authoritative simulation ticks;
- the server/authoritative side decides game outcomes;
- commands request mutation; events describe completed facts;
- authored data, authoritative state, client/presentation state, and backend
  state are distinct;
- `EntityId` is a simulation-local slot plus generation;
- `AssetId` is stable authored identity inside a content build;
- `ContentBuildId` identifies compatible cooked content for one executable or
  session;
- backend resources are owned by their systems and exposed through generation
  handles;
- serialized files contain no pointers or backend objects;
- structural entity changes occur only at named safe points;
- platform, Jolt, graphics, audio, and transport types do not enter game-facing
  contracts;
- game behavior does not execute inside backend worker callbacks;
- every authored prop is an entity with one explicit static/dynamic flag;
  systems may derive packed bulk records for static props;
- failure before initial startup completes tears down the attempted startup;
- queues and retained histories are bounded once they exist.

Initial canonical vocabulary:

| Term | M0 meaning |
| --- | --- |
| `PendingInput` | frame-to-tick accumulator owned by the local input path |
| `UserCommand` | one immutable, tick-addressed set of simulation intent |
| `ServerSimulation` | the sole authoritative M0 simulation root |
| `PresentationId` | M0 alias/wrapper for source `EntityId`; never serialized |
| `PresentationSnapshot` | complete immutable post-tick presentation state |
| `ClientView` | retained presentation records and renderer bindings; no authority |
| `ContentBuildId` | compatibility identity for one complete cooked content build |

The following are implementation details and must not be frozen prematurely:

- entity allocation and page layout;
- number of engine threads;
- render packet count;
- snapshot delta encoding;
- reliable transport lanes;
- per-asset live revision coexistence;
- render graph or task graph structure;
- scene replacement transaction protocol;
- allocator framework;
- backend implementation details behind the one-backend-per-build boundary.

## 6. Initial subsystem contracts

### 6.1 Entities and simulation

Use a generation-checked slot array containing `std::unique_ptr<Entity>`.
Concrete entity classes have fixed typed fields and shallow inheritance.

Initial behavior mechanisms are:

- construction and activation;
- scheduled `Think` calls;
- typed direct inputs/events required by the vertical slice;
- one deferred structural command buffer for spawn and destruction;
- safe `EntityId` lookup.

The built-in `prop` entity owns mesh/material references, visibility, collision
settings, and one `dynamic` flag. An unset flag means static. Visibility and
collision-enabled state are independent fields. There are no separate static
and dynamic prop classes and no third generic prop mode.

Do not initially implement class-specific slabs, a universal capability index,
runtime reflection, a general message bus, or one component framework. The class
descriptor contains only the data required to construct and load current entity
classes. Add metadata when the cooker, tooling, or replication path consumes it.

### 6.2 Assets and scenes

The runtime reads cooked target assets only. `AssetStore` loads complete immutable
payloads synchronously before play and retains them while referenced.

Initial references use `AssetId` plus the build-wide `ContentBuildId`; they do not
support multiple simultaneously live revisions. Asset files validate magic,
version, type, sizes, ranges, and semantic constraints before publication.

One scene is loaded during startup. Scene loading constructs entities, creates
required physics/render bindings, and either completes or tears down the startup
attempt. There is no live scene replacement transaction until map travel, content
reload, or multiple scene instances are promoted.

Every authored prop is a scene entity. Cooked class records are stored in packed
arrays, and rendering or physics may batch static prop bindings internally.
Those representations are system-owned derivatives; they do not replace prop
identity or remove props from entity storage.

### 6.3 Physics

Use Jolt directly behind `PhysicsWorld`; do not create a general multi-backend
hierarchy. The public boundary uses engine values and generation handles.

M0 configures Jolt's authoritative step with a single-threaded job system. This
keeps command-capture replay and failure diagnosis direct. A Jolt worker pool is
a measured M4 promotion, not an M0 capacity assumption.

The initial physics surface contains:

- static prop collision bindings, batched internally where useful;
- rigid bodies required by the room and door;
- one character movement path;
- ray/shape queries required by gameplay;
- triggers and normalized contact events;
- fixed-step execution.

Jolt callbacks write minimal normalized records into one physics-owned bounded
buffer and never invoke game code. The owning thread sorts records by the
documented body/entity/contact key after the step and then delivers them.

The M0 declared-state hash walks live entities in `EntityId` order and hashes
only explicit authoritative fields. Physics-derived position, rotation, and
velocity are copied into engine values first; NaN is invalid, negative zero is
normalized, and quaternion sign is canonicalized. Repeated runs of the same
content, commands, executable, and reference platform must match exactly. M0
does not promise cross-platform or multi-worker bitwise physics determinism.

### 6.4 Presentation and rendering

`ClientView` owns only presentation-facing records. A presentation identity maps
authoritative snapshot records to retained render objects. The first version does
not require a full second entity runtime.

The minimum M0 data contract is conceptually:

```cpp
using PresentationId = EntityId; // M0-only, in-process, never serialized

struct PresentationObjectState {
    PresentationId id;
    AssetId model;
    WorldTransform transform;
    PresentationFlags flags;
};

struct PresentationCameraState {
    WorldTransform anchor;
    float vertical_fov_radians;
};

struct PresentationSnapshot {
    SimulationTick tick;
    ContentBuildId content;
    PresentationCameraState camera;
    Span<const PresentationObjectState> objects;
};
```

`SimulationTick` is the last completed authoritative tick. Reapplying the same
tick is valid and idempotent; applying an older tick is rejected. M0
`PresentationFlags` is a fixed bit mask containing only `Visible` and
`CastShadow`. New flags require a demonstrated presentation consumer rather than
a generic property channel.

The complete `objects` array contains every active M0 object that should have a
retained presentation representation. It is sorted by `PresentationId`, contains
no duplicate IDs, and contains no entity pointers, backend handles, strings, or
variable property maps. Models include their ordinary material bindings; an M0
snapshot has no general material-override schema.

`ClientView::Apply` first validates the tick, content build, capacity, finite
values, sorted unique identities, and required assets. It reserves or stages all
fallible work before publishing any new state, then reconciles deterministically:

1. create a retained record and render binding for an unseen ID;
2. update transform and flags for an existing ID;
3. after processing the complete array, destroy retained IDs not present;
4. publish the new camera state only after reconciliation succeeds.

Any apply failure destroys staged work and leaves the prior `ClientView`
unchanged. In M0 the producer is trusted in-process code, but old-tick,
non-finite, duplicate, capacity, stale-content, and missing-asset cases remain
testable invariants. M1 replaces the M0-only identity in serialized snapshots
with reuse-protected `NetEntityId`; it does not expose local `EntityId` outside
the authoritative process.

Rendering runs on the main thread using one compiled backend and fixed passes.
The initial visual target is:

- static room props, with system-owned batching where useful;
- retained dynamic objects;
- one camera;
- opaque and alpha-tested materials already supported by the repository;
- the minimum existing lighting needed to make the room readable;
- debug drawing and diagnostic text.

Existing renderer features may remain, but they do not gate the milestone and
must not force a render graph, Render Thread, or runtime backend selection path.

The approved environment presentation path uses one concrete `skybox` entity
and one concrete `exponential_height_fog` entity per scene. The skybox owns one
authored HDR panorama reference plus intensity and rotation; renderer-private
derived cubemaps provide its background, diffuse irradiance, and prefiltered
specular IBL. When IBL is active it replaces the fallback ambient approximation,
but does not replace authored direct lights. Fog owns fixed UE-style height-fog
parameters and is evaluated consistently in deferred and forward shading. GPU
textures, convolution passes, and backend handles remain renderer-owned.

### 6.5 Input

Platform input is normalized into engine-owned actions. Game code builds a
`UserCommand` from an immutable action snapshot. The authoritative simulation
consumes commands, never raw device state.

### 6.6 Diagnostics and memory

Every active phase reports time, counts, bytes, drops, and high-water marks.
The audio callback, when added, and cross-thread queues must never allocate or
block. Initial simulation and rendering use ordinary owner-controlled containers
until measurements justify specialized arenas or slabs.

Long-running tests must show stable memory and no unbounded queue or history.

## 7. Milestones

[Release scope](DELIVERY.md#release-scope) is the only owner of milestone
deliverables, exit gates, and vertical build order.
[Architecture implementation status](STATUS.md) is the only owner of the
currently active milestone and step.

| Milestone | Architectural transition |
| --- | --- |
| M0 | direct playable room with authoritative simulation and complete in-process presentation snapshots |
| M1 | bounded serialized local command/snapshot boundary |
| M2 | remote transport, interpolation, and local-character prediction |
| M3 | production presentation and iteration features required by the selected game |
| M4 | individually measured scale promotions |

The table is orientation, not duplicate scope. When milestone wording differs,
the release-scope section in `DELIVERY.md` governs.

## 8. Extension map

| Initial implementation | Advanced replacement | Preserved seam |
| --- | --- | --- |
| `unique_ptr<Entity>` slots | class slabs or capability indexes | `EntityId` and lifecycle |
| synchronous complete assets | async loading and streaming | `AssetId`, immutable publication |
| one startup scene | map travel and transactional replacement | scene declaration and instance identity |
| main-thread fixed renderer | Render Thread, render graph, GPU-driven work | render handles and submissions |
| complete presentation snapshot | deltas, relevance, prioritization | stable schema and identity |
| in-process bounded queues | remote transport | `UserCommand` and snapshot records |
| direct authoritative result | client interpolation and prediction | tick-addressed state |
| static native game code | dynamic modules or scripting | class registration and typed fields |
| serial simulation | subsystem-local parallel work | phases and single-writer ownership |
| game-selected clips | animation graph | semantic animation state |
| semantic sound commands | advanced mixer/spatialization | audio command boundary |
| one complete content build | live revisions and patching | stable asset identity |

## 9. Performance profile required before optimization

M0 uses the checked-in
[MacBook Air M4 profile](DELIVERY.md#m0-performance-profile-macbook-air-m4),
which contains concrete values for:

- reference CPU, GPU, RAM, operating system, compiler, and build configuration;
- tick and frame targets;
- resident and scheduled entities;
- rigid bodies, contacts, and queries;
- static instances, dynamic objects, lights, and visible draws;
- asset, CPU-memory, and GPU-memory bytes;
- startup/cook/build iteration budgets;
- median, p95, p99, maximum, warm-up policy, and run duration.

Until the profile contains measured results, provisional targets may shape the
test harness but may not justify speculative optimization. Optimize
representation only where correctness requires it or a captured trace
demonstrates the cost.

## 10. Build, workflow, and platform policy

- CMake is the only project build generator.
- Dependencies are pinned to immutable revisions or deliberately vendored.
- One development/shipping platform is authoritative for the active milestone;
  other presets are compile/test support until CI promotes them.
- Backend dependencies stay private to their owner wherever the current
  implementation permits.
- Blender plus the CEngine add-on is the M0 scene editor.
- `ceasset` is the deterministic command-line cooker and validator.
- The cooker rebuilds only the changed asset and its affected dependency closure.
- Development runs use source/cooked assets without copying the entire asset tree
  after each C++ link.
- Every content error names the source asset and, when available, the Blender
  object/property responsible.

## 11. Document routing

Read `STATUS.md` and this document for ordinary implementation work. Open one
reference only when the task crosses its boundary:

| Reference | Read when |
| --- | --- |
| [Implementation status](STATUS.md) | choosing the next task or recording completed evidence |
| [Delivery, promotion, and performance](DELIVERY.md) | deciding scope, proposing complexity, or running acceptance gates |
| [Core target architecture](CORE.md) | changing entity, simulation, hosting, orchestration, memory, or threading semantics |
| [Target system architecture](SYSTEMS.md) | changing assets, scenes, input, physics, rendering, animation, or audio |
| [Target network architecture](NETWORK.md) | working on M1/M2 serialization, transport, replication, prediction, or interpolation |

The detailed documents preserve the intended advanced behavior. Their mechanisms
become normative for implementation only when this document or an approved
promotion makes the corresponding milestone active.
