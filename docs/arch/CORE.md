# CEngine Target Architecture: Core

> **Status: consolidated reference.** Complete long-term ownership, entity, simulation, hosting, orchestration, and runtime-service contracts.
> `STATUS.md` and `IMPLEMENTATION.md` determine what is active; this file
> preserves detailed target information without expanding current scope.

## Contents

- [CEngine Architecture](CORE.md#cengine-architecture)
- [Applying the Architecture](CORE.md#applying-the-architecture)
- [Simulation and Entities](CORE.md#simulation-and-entities)
- [Client and Server Architecture](CORE.md#client-and-server-architecture)
- [Engine Orchestration](CORE.md#engine-orchestration)
- [Runtime Services](CORE.md#runtime-services)

## CEngine Architecture

> **Status: complete target reference.** For implementation work, start with
> [Implementation status](STATUS.md), then read the deliberately simplified
> [implementation architecture](IMPLEMENTATION.md). The detailed contracts in
> this document remain the intended destination; they do not make deferred
> mechanisms part of the active milestone.

### 1. Purpose

This document defines the target architecture of CEngine. It explains the
engine from its highest-level product model down to the boundaries between its
major runtime systems. Detailed contracts live in the documents linked from
this overview.

This is a normative design document. It describes what CEngine is intended to
be, not only what the repository implements today. When implementation and this
architecture disagree, the difference must be resolved deliberately by changing
the implementation or amending the architecture.

#### 1.1 How to use these documents

For any implementation task, begin with [Implementation status](STATUS.md), then
read [Implementation architecture](IMPLEMENTATION.md) and confirm the milestone
contract in [Release scope](DELIVERY.md#release-scope). For a gameplay feature, then use
[Applying the architecture](CORE.md#applying-the-architecture), choose the cheapest
representation, and trace one datum from authored input to authority to
presentation. Then read only the detailed owner documents on that path. For an
engine change, begin with the physical constraints and boundary proof below,
then fill the workload and evidence required by
[Performance envelope](DELIVERY.md#performance-envelope). If ownership, lifetime,
failure, bound, and overload behavior cannot be stated, the design is not ready.

### 2. Executive summary

CEngine is a native runtime for creating single-player and multiplayer 3D
games. It turns authored content and player intent into authoritative game
state, then turns an appropriate view of that state into player feedback. Every
other responsibility exists only to make that loop correct, fast, or usable.

The architecture has five central ideas:

1. **Entities are the primary game-programming model.** CEngine supplies a
   standard entity library. A game adds native entity classes that implement its
   authored behavior.
2. **A simulation contains one live game.** It owns entities, game state,
   entity relationships, timers, and scene instances.
3. **Client and server game code are separate modules.** The server is
   authoritative. The client predicts permitted local behavior and presents
   replicated state. A single-player or listen-server process can host both.
4. **Systems own specialized runtime work.** Rendering, physics, animation,
   audio, input, and networking use narrow APIs and own their backend resources.
5. **Scenes are assets.** `AssetService` loads immutable `SceneAsset` data. A
   server or client simulation instantiates the parts appropriate to its role.

These choices provide a direct Source-style game extension model without
coupling game code to graphics, physics, audio, operating-system, or network
backend details.

#### 2.1 The essential data path

CEngine exists to perform one loop well:

```text
authored immutable content + player intent
                    |
                    v
      authoritative fixed-tick simulation
                    |
              validated state
                    v
       predicted/interpolated client view
                    |
                    v
       graphics + audio + input feedback
```

At tick `t`, the irreducible state transition is:

```text
(authoritative state[t], validated intent[t], immutable content, fixed dt)
    -> (authoritative state[t + 1], durable events, replication facts)
```

A remote client cannot read that state directly, so the server transmits a
bounded, relevance-filtered description of it. A renderer, audio device, or
physics solver cannot consume game meaning directly, so an owning system
translates packed commands into backend work. These are consequences of
process and device boundaries, not patterns selected for their own sake.

Every top-level concept must do at least one of the following:

- own state with a distinct lifetime or thread;
- enforce an authority, safety, or backend boundary;
- remove work or allocation from a recurring hot path.

If it does none of these, it is not a top-level abstraction. In particular,
CEngine does not add a general component framework, message bus, service
locator, runtime prefab graph, or render graph merely to make the architecture
look extensible.

#### 2.2 Physical constraints and chosen policy

Physical or logical constraints cannot be designed away:

| Constraint | Consequence |
| --- | --- |
| a frame and tick have finite time | recurring work and queues are bounded and measured |
| remote clients are delayed, lossy, and untrusted | intent is validated; authority cannot be a shared object |
| CPU cores mutate memory concurrently | mutable state has one phase-qualified writer |
| GPUs, audio devices, and physics solvers own opaque state | narrow owners translate data at explicit boundaries |
| working sets exceed caches and sometimes memory | static content is packed, spatially chunked, and streamed |
| files, packets, modules, and devices can fail | publication is validated and transactional |
| identity outlives memory addresses | serialized IDs and generation handles replace cross-owner pointers |

The following are CEngine policies, not laws of nature: a server-authoritative
snapshot protocol, fixed-rate authoritative ticks, concrete native entity
classes, cooked packed mesh scenes, retained runtime systems, and single-writer
phases. They are defaults because together they satisfy the constraints with
one understandable path. A replacement is valid when measurements and a
complete correctness contract show a lower total cost; compatibility with an
industry fashion is not evidence.

#### 2.3 The primitive set

The runtime is reconstructed from a small set of mechanisms:

- plain values plus stable serialized IDs;
- immutable cooked payloads stored in contiguous blocks;
- generation-checked slots for mutable cross-owner objects;
- owner-controlled slabs, pages, rings, and resettable arenas;
- typed commands for requested mutation;
- typed events for completed facts;
- immutable snapshots for observation across time, thread, or process;
- fixed ticks and explicit frame/tick phase boundaries;
- `Result`/`Status` for expected failure.

Entities give these primitives game meaning. Systems own specialized state.
Services perform request-driven work. None of those labels introduces a second
object model.

#### 2.4 Five kinds of state

Every datum belongs to exactly one primary state category. Copies may cross a
boundary only through an explicit command, event, or snapshot.

| State | Primary owner | Examples |
| --- | --- | --- |
| Authored content | `AssetService` payload | scene declarations, meshes, materials, collision |
| Authoritative simulation | `ServerSimulation` and server systems | health, transforms, AI, physics outcomes |
| Client simulation | `ClientSimulation` | prediction history, replicated samples, local camera state |
| Presentation | client systems | poses, render records, voices, transient draws, UI |
| Backend/derived | owning specialized system | GPU images, physics shapes, decoded audio buffers |

This classification answers the first ownership question. For example, a mesh
asset is authored content, a door's open fraction is authoritative simulation,
its interpolated transform is client simulation, and its draw record is
presentation state. None is the same object.

#### 2.5 Opinionated defaults

The complete target architecture uses one `ServerSimulation` and one
`ClientSimulation` per local process, one fixed server tick rate, native entity
classes, a complete cooked mesh scene, server authority, snapshot replication,
and retained presentation systems. The initial implementation uses the smaller
composition in [Implementation architecture](IMPLEMENTATION.md): one
authoritative simulation, a lightweight `ClientView`, complete presentation
snapshots, and main-thread rendering. Target owners are promoted without
changing the stable authority and data boundaries.

[Release scope](DELIVERY.md#release-scope) is the normative build plan.
[Tradeoffs and promotion](DELIVERY.md#tradeoffs-and-promotion) owns every criterion for expanding it.

#### 2.6 Canonical vocabulary

These names are normative across code and documentation:

| Term | Meaning |
| --- | --- |
| simulation | root object that owns one live game's entities and rules |
| `ServerSimulation` | authoritative simulation |
| `ClientSimulation` | replicated, predicted, and presentation-facing simulation |
| `SimulationCommands` | tick-scoped writer for deferred structural changes |
| `SceneAsset` | immutable cooked scene content |
| `SceneInstance` | one simulation's live instantiation of scene content |
| scene chunk | optional system-owned batch derived from static scene entities |
| entity | named, addressable game object with concrete behavior |
| system | owner of specialized scheduled state such as physics or rendering |
| service | request-driven facility such as assets or saving |
| handle | generation-checked identity for an object owned by another subsystem |

“World” is reserved for spatial language such as world-space coordinates. It
does not name the simulation root.

### 3. Product-level model

CEngine separates reusable engine code from game-specific code.

```text
CEngine
  foundation, content, runtime systems, and module hosting
                         |
            +------------+------------+
            |                         |
      GameServer module          GameClient module
      authoritative rules        presentation and prediction
      server entities            client entities
            |                         |
            +------------+------------+
                         |
                    GameShared
        protocol, schemas, commands, and selected
              deterministic shared simulation
```

The same game can run in three configurations:

| Configuration | Loaded game modules | Primary use |
| --- | --- | --- |
| Dedicated server | `GameServer` and `GameShared` | authoritative headless hosting |
| Remote client | `GameClient` and `GameShared` | player presentation and input |
| Local/listen server | all three modules | single-player or player-hosted multiplayer |

Single-player uses an in-process client/server transport. It exercises the same
authority and replication boundaries as multiplayer without requiring network
packets to leave the process.

This is one semantic path, not a requirement to simulate an OS network stack.
The in-process transport may transfer validated packed records directly and
share immutable asset payloads. It preserves ordering, capacity, command
validation, and snapshot application because bypassing those rules would create
a second game with different failure and authority behavior.

### 4. Top-level architecture

```text
Game host application
  |
  +-- Game modules
  |     +-- GameShared
  |     +-- GameServer (when authoritative simulation is hosted)
  |     `-- GameClient (when presentation is hosted)
  |
  +-- Simulations
  |     +-- ServerSimulation (when authority is hosted)
  |     `-- ClientSimulation (when presentation is hosted)
  |
  +-- Runtime systems
  |     +-- InputSystem (client)
  |     +-- PhysicsSystem (server; optional client prediction scene)
  |     +-- AnimationSystem (client)
  |     +-- AudioSystem (client)
  |     +-- RenderSystem (client)
  |     `-- NetworkSystem
  |
  `-- Shared services
        +-- AssetService
        `-- Platform services
```

#### 4.1 Application

The host application is the composition root. It:

- starts the platform and shared services;
- loads or links the required game modules;
- creates the necessary systems and simulations;
- connects client and server endpoints;
- controls the outer frame and shutdown schedule.

The application does not implement entity behavior, replication policy,
rendering, or physics. It constructs the objects that do.

#### 4.2 Systems, services, and subsystems

A **system** owns specialized runtime state and advances it on a frame, fixed
tick, or device schedule. Physics and rendering are systems.

A **service** performs work in response to requests without defining the game
simulation schedule. Asset acquisition is a service.

A **subsystem** is an internal responsibility inside a system or service. A
feature becomes top-level only when it requires an independent ownership,
lifetime, or scheduling boundary.

#### 4.3 Boundary proof

Each top-level owner exists for a concrete reason. The cheaper alternative is
the default whenever that reason is absent.

| Owner | Irreducible responsibility | Why it is a boundary |
| --- | --- | --- |
| host application | composition and outer schedule | one place must create, order, and destroy configured owners |
| `ServerSimulation` | authoritative game transition | authority needs one mutation order and cannot trust a client |
| `ClientSimulation` | prediction and presentation-facing state | client time and correction differ from authoritative time |
| `AssetService` | immutable content identity and publication | storage I/O and staged residency outlive any one consumer |
| `InputSystem` | device state to actions | focus and platform devices have a distinct event lifetime |
| `PhysicsSystem` | solver state and collision queries | the backend owns coupled opaque state stepped as a unit |
| `AnimationSystem` | batched pose state and evaluation | poses are bulk derived data with frame-buffer lifetimes |
| `AudioSystem` | real-time mixing and device state | its callback has a separate thread and hard latency rules |
| `RenderSystem` | visibility, GPU state, and image production | GPU state and frame execution have a separate device lifetime |
| `NetworkSystem` | connection I/O, delivery, and sequencing | transport crosses an unreliable process boundary |
Game replication is deliberately not a runtime system. Network classes,
relevance, prioritization, and prediction are game meaning owned by the game and
simulation; `NetworkSystem` only transports prepared data.

### 5. Core abstractions

#### 5.1 Game modules

`GameServer` and `GameClient` are statically linked game-code modules hosted by
CEngine. “Module” describes source, dependency, and ownership boundaries.

- `GameServer` registers authoritative entity classes, game rules, and
  replication producers.
- `GameClient` registers client entity classes, prediction, interpolation,
  cameras, user interface, and presentation effects.
- `GameShared` contains versioned protocol definitions, common value types,
  property schemas, asset identifiers, user commands, and selected deterministic
  simulation used by both sides.

Shared code must not contain mutable process-global game state or hide
client/server behavior behind extensive conditional compilation.

See [Client and server architecture](CORE.md#client-and-server-architecture).

#### 5.2 Simulations

A simulation is the root of one live game. `ServerSimulation` and
`ClientSimulation` are concrete, separate roots even when they run in one process.
They may share private entity-storage and lifecycle code; the architecture does
not require a public simulation base class.

The server simulation owns authority. The client simulation contains replicated
proxies, local predicted entities, and client-only presentation entities. They communicate
through network messages or the equivalent in-process transport, never through
shared entity pointers.

See [Simulation and entities](CORE.md#simulation-and-entities).

#### 5.3 Entities

An entity is a named, addressable object with a concrete game meaning. Entities
are the primary way authored scenes introduce executable behavior.

CEngine provides reusable classes such as props with a static/dynamic flag, lights,
triggers, cameras, player starts, and logic relays. A game supplies classes such
as enemies, weapons, capture zones, objectives, and round managers.

Server and client implementations of one networked concept can differ:

```text
ServerPlayer             ClientPlayer
authoritative health     replicated health
inventory and damage     prediction and correction
server physics           rendering and animation
          \                 /
           +-- NetEntityId +
```

#### 5.4 Runtime systems

Systems perform specialized bulk work. They own the records behind opaque
generation handles. Entities may retain the handles required for their behavior,
but they do not own backend pointers.

For example, a server dynamic prop can retain a `PhysicsBodyHandle`; its client
counterpart can retain a `RenderObjectHandle` and `AnimationInstanceHandle`.
There is no universal entity-runtime-links record.

#### 5.5 Assets and scenes

Assets are identified, storage-backed runtime data. A scene is an asset whose
payload declares entities, properties, relationships, output connections, and
dependencies.

`AssetService` loads a `SceneAsset`. A simulation instantiates it. Loading does
not create live entities, and scene instantiation does not implement file I/O.

CEngine does not require a general runtime prefab system. Reuse comes from
entity classes and referenced assets. Authoring tools may expand reusable
assemblies into ordinary scene entities during cooking.

See [Assets and scenes](SYSTEMS.md#assets-and-scenes).

#### 5.6 The game-authoring path

The minimum native gameplay feature is one entity class, not a collection of
framework registrations. A game author writes fixed fields and lifecycle or
event methods, then registers one descriptor:

```cpp
namespace CEngine::Game {

class Door final : public ServerEntity {
  public:
    [[nodiscard]] static Status Register(EntityRegistry& registry);

    [[nodiscard]] Status OnSpawn(SpawnContext& context) override;
    void Open(const InputContext& input);
    void Think(const TickContext& tick);

  private:
    AssetRef<CollisionAsset> collision_{};
    EntityRef activator_{};
    PhysicsBodyHandle body_{};
    DoorState state_ = DoorState::Closed;
    float speed_ = 1.0f;
};

Status Door::Register(EntityRegistry& registry) {
    return registry.Define<Door>("door")
        .Property("collision", &Door::collision_, PropertyFlags::Required)
        .Property("speed", &Door::speed_, FloatRange{0.01f, 100.0f})
        .Input("Open", &Door::Open)
        .Commit();
}

} // namespace CEngine::Game
```

Registration happens once, scene properties are converted into fixed fields
once, and the entity pays no per-frame property lookup or mandatory update. Adding client
presentation or replication is a separate explicit descriptor for the same
game concept, not inheritance from a networked mega-base class.

Game callbacks receive narrow phase-specific contexts such as `SpawnContext`,
`TickContext`, and `InputContext`. A context exposes only operations valid in
that phase. Game code cannot discover process globals or retain backend objects.

### 6. Server and client data flow

The server processes authoritative simulation in fixed ticks. The client samples
input, sends user commands, predicts permitted local behavior, and renders an
interpolated view of server state.

```text
Client InputSystem
  -> UserCommand
  -> NetworkSystem or in-process transport
  -> ServerSimulation
  -> shared movement and server entity behavior
  -> server PhysicsSystem
  -> authoritative simulation state
  -> relevance and delta snapshot
  -> ClientSimulation
  -> reconciliation and interpolation
  -> AnimationSystem / AudioSystem / RenderSystem
```

Static scene content is loaded from the same cooked `SceneAsset` on both sides
and is not replicated object-by-object unless it has dynamic authoritative
state.

The server never trusts client position, damage, inventory, or spawn decisions.
The client sends intent. The server validates and simulates the result.

See [Networking and replication](NETWORK.md#networking-and-replication).

### 7. Scene and player lifetime

A scene instance is a group of entities owned by a simulation. It is not the
simulation itself.

```text
ServerSimulation
  simulation-owned match and player state
  scene instance: current map
    doors, triggers, NPCs, map logic, player pawns

ClientSimulation
  connection-local presentation state
  scene instance: current map
    static presentation, replicated proxies, client effects
```

For multiplayer, a connected participant, logical player state, and physical
pawn have different lifetimes:

```text
PlayerConnection -> PlayerState -> possessed PlayerPawn entity
```

- `PlayerConnection` lasts while the network participant is connected.
- `PlayerState` stores logical session data and survives pawn respawn.
- `PlayerPawn` is a physical scene entity and can be destroyed and reconstructed.

### 8. Public API rules

All engine and game-module boundaries follow these rules:

1. Give each top-level system or service one public facade.
2. Keep backend-specific types behind the facade.
3. Pass typed descriptions, commands, snapshots, and events.
4. Use opaque generation handles for runtime objects.
5. Provide symmetrical create and destroy operations.
6. Document calling threads and input lifetimes.
7. Do not allow one system to modify another system's private state.
8. Keep serialized records separate from live runtime objects.
9. Do not expose OpenGL, Vulkan, Jolt, GLFW, socket-library, or audio-library
   types in engine and game APIs.
10. Make the common call synchronous and unsurprising when it is simulation-local;
    use staging only across a real thread, I/O, or transaction boundary.
11. Accept caller-owned spans or typed descriptions for batches; do not require
    one heap allocation per entity, command, event, draw, contact, or packet.
12. Return explicit `Result` values for expected failure. Reserve assertions for
    violated programmer invariants.
13. Do not expose configurability without a supported use case and a safe
    default.
14. Prefer concrete facades. Add a virtual/function-table boundary only for a
    backend, platform adapter, or test seam that is actually
    substituted. Do not mirror every concrete class with an `IThing` interface.
15. A handle lookup is justified when it protects lifetime or crosses owners;
    simulation-local fixed fields and direct helper calls do not need another layer.

Game code is statically linked.

#### 8.1 Results and diagnostics

Expected failure returns `Result<T>` or `Status`; it does not return a sentinel
handle and require callers to inspect a global error. A status contains a stable
domain/code plus an optional diagnostic record ID. Human-readable context lives
in the structured diagnostics system rather than an allocating error-string
chain on every call.

The layer that can act on an error handles or propagates it. The boundary that
finally chooses fallback, retry, disconnect, or abort emits one diagnostic, so a
single failure is not logged at every stack frame. Queued owner operations
report failure through their documented result or event path.

Unless an API explicitly returns a partial-result status, failure publishes no
new object and leaves prior valid state unchanged. Stale handles, capacity
exhaustion, incompatible revisions, and invalid untrusted data have
distinct stable codes; they are not collapsed into `false`.

### 9. Scheduling model

Simulation mutation occurs only in named phases. Calls made during a phase either
take effect immediately by contract or enter that simulation's bounded command
buffer for the next structural safe point. No public API leaves this ambiguous.

The engine drains each event class at most at its documented point. Newly
generated events never recursively re-enter arbitrary entity code; they append
to the current bounded queue or the next phase according to event type. This
makes order reproducible and prevents callback-depth failures.

#### 9.1 Server tick

1. **Ingress:** drain transport messages and validate tick-addressed commands.
2. **Pre-simulation safe point:** apply queued spawns, destroys, ownership
   changes, and scene operations from the prior tick.
3. **Gameplay:** advance timers, scheduled entity thoughts, rules, AI, and
   shared movement intent.
4. **Physics:** apply body commands, step exactly once, and publish normalized
   results.
5. **Post-physics gameplay:** apply transforms and dispatch contacts, triggers,
   and resulting entity I/O without recursion.
6. **Commit safe point:** apply deferred structural changes and finalize the
   tick. Entities created here begin ordinary simulation on the next tick.
7. **Replication capture:** copy declared state into immutable, packed snapshot
   storage.
8. **Egress:** build per-connection deltas and enqueue transport output.

A server tick never waits on storage, shader compilation, network I/O, or
client readiness. Activation-critical assets are ready before a scene enters
the ticking simulation.

#### 9.2 Client frame

1. Pump platform and device events.
2. Build the local action snapshot and user command.
3. Send the command and retain it for possible replay.
4. Apply received entity creation, destruction, and snapshots.
5. Reconcile the local predicted state and replay unacknowledged commands.
6. Interpolate remote entities for presentation time.
7. Evaluate final animation poses and presentation events.
8. Submit audio spatial state and commands.
9. Extract render views and render the frame.

Snapshot application and prediction replay are client-simulation mutation phases.
Animation, audio, and rendering consume immutable presentation snapshots and
cannot write results back into authority. Multiple render frames may occur
between simulation ticks; rendering never advances authoritative time.

#### 9.3 Overload policy

Real time can outrun simulation. The server executes only a configured maximum
number of catch-up ticks per outer loop, records the overload, and then runs
late; it does not silently enlarge the fixed time step. A competitive server
normally never drops authoritative ticks. A local noncompetitive product may
choose a documented clamp policy.

Every bounded queue has an owner-specific response: disconnect abusive network
input, drop prioritized cosmetic work, request a fresh baseline, or fail scene
activation. Overflow is never allowed to corrupt ordering or trigger unbounded
allocation.

Single-player follows both schedules through an in-process connection.

### 10. Dependency architecture

```text
Foundation
  IDs, math, containers, diagnostics, platform abstractions
       |
       +----------------------+----------------------+
       v                      v                      v
Assets                  Runtime system APIs      Network transport
       \                      |                      /
        +---------------------+---------------------+
                              v
                      Simulation runtime
                  entities, scenes, lifecycle
                              |
               +--------------+--------------+
               v                             v
         Engine entities                 GameShared
                                               |
                                 +-------------+-------------+
                                 v                           v
                            GameServer                   GameClient
                                 +-------------+-------------+
                                               v
                                         Host application
```

Backend implementations depend on their public system APIs. Game modules depend
on CEngine. CEngine never depends on one specific game.

### 11. Detailed specifications

| Area | Document |
| --- | --- |
| Active implementation, milestones, stable seams, and agent routing | [Implementation architecture](IMPLEMENTATION.md) |
| Release contents and vertical build order | [Release scope](DELIVERY.md#release-scope) |
| Accepted costs, rejected alternatives, and measurable promotion criteria | [Tradeoffs and promotion](DELIVERY.md#tradeoffs-and-promotion) |
| First-principles capacity equations, product profiles, and acceptance gates | [Performance envelope](DELIVERY.md#performance-envelope) |
| Active M0 reference workload and machine targets | [M0 performance profile](DELIVERY.md#m0-performance-profile-macbook-air-m4) |
| Top-level composition, server ticks, client frames, and scene activation | [Engine orchestration](CORE.md#engine-orchestration) |
| Choosing representations and implementing common 3D game features | [Applying the architecture](CORE.md#applying-the-architecture) |
| Simulation root, entity registration, lifecycle, I/O, and game rules | [Simulation and entities](CORE.md#simulation-and-entities) |
| Client/server ownership, hosting, and single-player | [Client and server architecture](CORE.md#client-and-server-architecture) |
| Network transport, replication, prediction, and interpolation | [Networking and replication](NETWORK.md#networking-and-replication) |
| Asset identity, synchronous loading, packed mesh scenes, and activation | [Assets and scenes](SYSTEMS.md#assets-and-scenes) |
| Rendering front end, GPU ownership, extraction, and frame execution | [Rendering](SYSTEMS.md#rendering) |
| Physics ownership, queries, contacts, character movement, and authority | [Physics](SYSTEMS.md#physics) |
| Client clip playback, cross-fade, cosmetic events, and skinning | [Animation](SYSTEMS.md#animation) |
| Audio commands, mixing, spatial state, and device callback | [Audio](SYSTEMS.md#audio) |
| Input devices, bindings, actions, and user commands | [Input](SYSTEMS.md#input) |
| Platform, clocks, memory, threading, shutdown, and diagnostics | [Runtime services](CORE.md#runtime-services) |

### 12. Hard-to-change contracts

The following decisions shape persisted content, network compatibility, game
code, or concurrency. They receive substantially more scrutiny than an internal
algorithm:

| Contract | Why change is expensive | Required stability mechanism |
| --- | --- | --- |
| server/client authority split | changes game semantics and security | separate mutable simulations and intent-only client input |
| tick, command, and snapshot semantics | affects prediction, replay, and every connection | versioned schemas, explicit tick IDs, compatibility tests |
| entity and asset identity | appears in scenes, references, and packets | stable serialized IDs; generations only for live slots |
| cooked scene/chunk layout | controls toolchain and platform data | versioned formats, bounds validation, migration or rebuild |
| mutation phases and thread ownership | violations create nondeterministic corruption | named safe points, immutable worker inputs, one writer |
| coordinate system and units | touches physics, rendering, scenes, and protocols | one documented convention from the first format |

Backend algorithms, container internals, culling methods, compression, and work
partitioning stay behind these contracts and are expected to evolve. CEngine
does not freeze an internal representation merely because it was implemented
first.

### 13. Architecture constraints

These rules are normative:

- Entities are the primary scene-authored extension mechanism.
- Engine and game entities use the same registration model on each runtime side.
- Server and client simulations are separate instances with no shared live entities.
- The server is authoritative for game outcomes.
- The client predicts only explicitly permitted behavior.
- Network transport carries data but does not define game meaning.
- A scene is an asset; a scene instance is live simulation state.
- `AssetService` loads scene data; the owning simulation instantiates it.
- CEngine does not require a general runtime prefab system.
- Runtime systems own specialized objects and backend resources.
- Serialized data contains no runtime pointers or backend objects.
- Scene instantiation and network snapshot application fail without exposing
  partially constructed public state.
- Backend worker callbacks never execute arbitrary entity behavior.
- Large payloads live in dedicated assets and are referenced by scenes.
- Each piece of mutable state has one writer during its mutation phase.
- Real-time callbacks and declared cross-thread paths use retained bounded
  storage and make no general-purpose heap allocation calls. Other recurring
  paths instrument allocation, forbid unbounded fallback, and adopt pools,
  pages, or arenas when their checked-in performance profile requires them.
- Queues and histories are bounded, instrumented, and have defined overload
  behavior.
- The common entity path requires no per-frame callback, property lookup,
  service lookup, or cross-system pointer chasing.
- Static mesh-scene data is stored and processed in bulk; it is not expanded
  into one general-purpose entity per triangle or render record.
- Every product configuration declares a versioned performance envelope and
  passes the applicable workload gates in
  [Performance envelope](DELIVERY.md#performance-envelope).

### 14. Change control

Architecture changes must update the high-level document and every affected
detailed contract in the same change. New code-generation work should identify
the owning subsystem and its applicable architecture document before creating a
new top-level abstraction.

A change to a hard-to-change contract includes: the violated physical or
product constraint, measured evidence, the cheapest rejected alternative,
format/protocol migration, failure and overload behavior, and updated tests.
“More flexible,” “industry standard,” and possible future use are not evidence.

When a design question is unresolved, keep the decision local and reversible.
Do not publish it as a cross-system API or serialized format until the evidence
exists.

---

## Applying the Architecture

> **Status: target design examples.** Use these examples to preserve ownership
> seams, not to add features beyond the active milestone in `IMPLEMENTATION.md`.

### 1. Purpose

This document is the shortest path from a game idea to the correct CEngine
owner and data flow. It is both a usability guide and a test of the architecture:
common 3D game features must fit without a new top-level abstraction.

### 2. Choose the representation

Start with the cheapest representation that has the required lifetime and
behavior.

Use this decision order:

1. If it is immutable authored data, make it an asset or packed scene record.
2. If it has named game behavior or independent mutable lifetime, make it an
   entity with fixed fields.
3. If it is simulation-wide policy, keep it in game rules rather than inventing
   an entity or manager.
4. If many objects require the same backend or bulk operation, retain narrow
   handles in entities and put the packed records in the owning system.
5. Introduce another top-level owner only if the boundary proof in
   [CEngine architecture](CORE.md#43-boundary-proof) applies.

| Need | Use | Do not start with |
| --- | --- | --- |
| static visible/collidable environment | packed `SceneAsset` chunk records | one entity per mesh or surface |
| named, configured, connected behavior | entity | global manager switch statement |
| movable or independently mutable object | entity with narrow system handles | backend object exposed to game code |
| match-wide policy | simulation-owned game rules | invisible placed singleton entity |
| persistent player identity/progression | `PlayerState` | pawn fields that disappear on respawn |
| fixed authored data shared by instances | asset | mutable runtime singleton |
| hot bulk specialized work | owning system or a proven entity capability index | per-entity messages or components by default |
| short-lived presentation | packed render/audio/effect command | authoritative entity unless gameplay needs it |

An object can cross representations over its life. A static packed wall can be
replaced at a safe point by a breakable-wall entity when gameplay activates it;
after destruction, debris can be client presentation unless authoritative
collision is required. The transition is explicit and server-directed.

### 3. A complete networked door

#### 3.1 Authored content

The scene contains one `door` declaration with fixed properties: model,
collision, movement endpoints, speed, lock state, and input/output connections.
Because it is a networked domain, only the server instantiates the declaration.

#### 3.2 Server representation

`ServerDoor` owns door state and a `PhysicsBodyHandle`. `OnSpawn` validates
assets and creates its kinematic body. `Open` validates lock/game rules and sets
the authoritative state. Scheduled `Think` calls advance only while the door is
moving. Physics results update the authoritative transform.

The replication descriptor captures state, movement start tick, and endpoints.
It does not replicate the physics handle, raw property map, or per-frame bone
data.

#### 3.3 Client representation

The server create record constructs `ClientDoor`, which owns a
`RenderObjectHandle`, optional animation handle, and presentation history. The
client derives smooth motion from replicated semantic state and timestamps. A
confirmed transition can emit a tokenized sound; reconnecting late still shows
the correct door because durable state is replicated.

```text
scene declaration
  -> ServerDoor + physics body
  -> authoritative door state
  -> snapshot/create event
  -> ClientDoor + render/audio bindings
```

This example uses every fundamental boundary and no door-specific engine
manager.

### 4. Player, camera, and movement

The server owns `PlayerConnection`, `PlayerState`, and the possessed
`ServerPlayerPawn`. The client owns local input contexts, a `ClientPlayer`
representation, camera behavior, HUD, and prediction history.

One tick follows this path:

1. `InputSystem` publishes actions.
2. `GameClient` creates a bounded `UserCommand`.
3. shared movement advances one packed `PredictionState` locally.
4. the server validates the command and runs the same controller contract.
5. the server snapshot acknowledges the command and returns authoritative
   movement state.
6. the client restores/replays if needed and smooths only the presentation.
7. the camera derives from final predicted/interpolated presentation state.

The camera is not replicated. The client physics scene cannot award a jump,
pickup, hit, or vehicle entry.

### 5. Weapons and combat

#### 5.1 Hitscan weapon

The command carries attack intent and aim inputs. The server checks cadence and
inventory, optionally performs bounded lag compensation, queries authoritative
hit state, applies damage, and changes durable replicated state. A tokenized
event drives muzzle flash, tracer, impact, sound, and camera feedback.

The local client may predict the firing animation and sound. It never predicts
the durable hit result as authority.

#### 5.2 Projectile

Use an authoritative entity when a projectile has gameplay lifetime, collision,
ownership or durable replicated state. Use a packed client effect when it is only
visual.

#### 5.3 Melee

Server timers own the attack window and server physics owns the hit query.
Client animation events present anticipation but cannot apply damage.

### 6. AI and navigation

AI controllers are server entities or simulation-owned game-rule helpers. A
game-local immutable waypoint/grid asset and bounded search are sufficient.
Perception reads explicit gameplay/physics queries, not renderer visibility.

An NPC schedules thinking at the cadence its behavior needs. It does not receive
a mandatory frame callback. Path queries run on the Game Thread and return into
caller-owned bounded storage.

### 7. Vehicles, constraints, and moving platforms

A vehicle is an entity or a small cooperating set of entities with physics
body/constraint handles. The server owns entry permission, occupants, controls,
and physical outcomes. The client can predict a locally controlled vehicle only
if its complete replay state and collision dependencies are declared; otherwise
it interpolates and accepts correction.

Moving platforms are ordinary kinematic entities. Character prediction includes
support identity and relative motion, so correction does not depend on an
untracked backend contact.

### 8. Destruction and procedural worlds

Destruction has two explicit classes:

- **gameplay destruction:** the server replaces or modifies authoritative
  collision/entities and replicates durable state;
- **cosmetic destruction:** clients generate bounded debris/effects from a
  confirmed token and local quality budget.

Procedural generation produces the same normalized staging records as cooked
content: packed scene-chunk records plus entity declarations. The server chooses
the seed and authoritative results. Generated data is validated and published
transactionally; procedural origin does not create a second runtime object
model.

### 9. UI, inventory, quests, and dialogue

Authoritative inventory, quest, and dialogue progression belongs to server
entities, player state, or game rules according to lifetime. Replication exposes
only the client-facing view model needed by the owning player/audience.

`GameClient` owns UI model and interaction policy. UI actions become validated
`ClientRequest` messages or `UserCommand` fields. Tick-sensitive predicted
actions use commands; reliable menu/dialogue/inventory transactions use
idempotent requests. The renderer receives final 2D draw data; it does not
inspect inventory or quests.

### 10. Single-player, cooperative, and competitive games

The gameplay model does not change by mode:

| Mode | What changes | What does not |
| --- | --- | --- |
| single-player | in-process transport, one connection, latency near zero | server authority and separate simulations |
| cooperative | relevance, audience policy, and shared objectives | command and snapshot contracts |
| competitive | stricter validation and anti-cheat policy | ownership and presentation boundary |

### 11. When a new abstraction is justified

Use a concrete entity, game-rules helper, private subsystem, or asset. Promotion
to a new engine abstraction follows
[Tradeoffs and promotion](DELIVERY.md#2-promotion-rule).

### 12. Cost checklist

Before accepting a gameplay or engine design, answer:

- What player-visible or authoritative outcome is minimally required?
- Which physical constraint prevents the direct local implementation?
- Which of the five state categories owns each field?
- Which thread writes it, and in which phase?
- What stable ID crosses each boundary?
- What happens if its target is destroyed, unloaded, late, duplicated, or stale?
- What allocates at load, spawn, tick, frame, packet, and audio-callback time?
- What is bounded, and what happens at the bound?
- Which product-profile number pays for the design, and what is its p99 cost?
- Does work scale with relevant active data or accidentally with all resident
  data?
- Can a dedicated server run it without presentation systems?
- Can a client reconnect or join late and reconstruct durable state?
- Can the feature be replayed from declared inputs?
- Why is the chosen representation cheaper than an ordinary entity or packed
  scene record?
- What simpler alternative was measured or logically ruled out?
- Does this alter an ID, format, protocol, authority, unit, or thread contract
  that will be expensive to change later?

If these answers are not explicit, the feature is not architecturally complete.
The required measurements and workload manifests are defined in
[Performance envelope](DELIVERY.md#performance-envelope).

---

## Simulation and Entities

> **Status: detailed target contract.** M0 begins with generation-checked pointer
> slots and only the lifecycle/indexing used by the vertical slice. Optimized
> storage below is promoted without changing entity identity or semantics.

### 1. Purpose

This document defines CEngine's game-domain model. It covers simulations, entity
classes, registration, identity, lifecycle, relationships, entity input/output,
game rules, and runtime-system bindings.

The model is intentionally Source-style: concrete entities are the primary
native extension mechanism for scene-authored behavior.

### 2. Responsibility

A simulation is the root of one live game. It owns:

- entity storage and identity;
- entity construction, activation, and deferred destruction;
- scene instances and their entity membership;
- names, tags, lookup, and explicit relationships;
- scheduled entity thinking and timers;
- entity inputs, outputs, and typed events;
- game rules and simulation-wide game state;
- routing between entity behavior and system-facing interfaces.

A simulation does not own:

- asset file I/O or asset caches;
- graphics or physics backend objects;
- audio devices or network sockets;
- platform windows and input devices;
- another simulation's live entities.

### 3. Server and client simulations

The server and client follow the same entity and lifecycle contracts and may
share private helpers, but each owns a separate concrete simulation root.

#### 3.1 Server simulation

`ServerSimulation` contains authoritative game state:

- server entities and game rules;
- damage, inventory, objectives, AI, and triggers;
- authoritative transforms and physics results;
- authoritative scene changes;
- replicated-property sources;
- network relevance inputs.

#### 3.2 Client simulation

`ClientSimulation` contains presentation state:

- client representations of replicated entities;
- the local predicted entity state;
- interpolation histories for remote entities;
- client-only presentation entities;
- client scene instances;
- camera, HUD, animation, audio, and rendering-facing behavior.

The two simulations never share `Entity*`, `EntityHandle`, or mutable entity storage. A
`NetEntityId` relates representations across the network.

### 4. Entity classes

An entity class gives authored data a concrete runtime meaning. Examples include
physical objects, logic controllers, environment settings, and markers.

```text
Engine server entities            Engine client entities
----------------------            ----------------------
server_prop                       client_prop
server_trigger                    client_light
server_logic_relay                client_camera
server_player_start               client_environment

Game server entities              Game client entities
--------------------              --------------------
server_capture_zone               client_capture_zone
server_enemy                      client_enemy
server_round_manager              client_round_presentation
server_player                     client_player
```

Names above illustrate runtime implementations. A scene can use one canonical
authored classname such as `capture_zone`; the server and client registries map
that classname to their appropriate implementations.

#### 4.1 Engine and game entities

CEngine supplies a standard entity library. Game modules add entities without
modifying CEngine.

Built-in and game-defined classes use the same:

- base entity contract;
- class registry;
- property metadata;
- scene instantiation path;
- identity and reference model;
- lifecycle and safe-destruction rules;
- input/output mechanism;
- network metadata rules.

The engine must not use hard-coded loader branches for built-in classnames.

#### 4.2 Class descriptors

Each runtime side owns an `EntityFactory` populated with `EntityClassDescriptor`
records.

```cpp
namespace CEngine {

struct EntityClassDescriptor {
    EntityClassId id{};
    std::string_view classname;
    EntityDomain domain{};
    std::uint32_t schema_version = 1;
    EntityConstructFn construct = nullptr;
    EntityDestroyFn destroy = nullptr;
    Span<const PropertyDescriptor> properties{};
    Span<const InputDescriptor> inputs{};
    Span<const OutputDescriptor> outputs{};
};

} // namespace CEngine
```

The in-memory layout is private, but the descriptor contract must support:

- construction without a classname switch;
- validation before construction;
- conversion from serialized properties to fixed typed fields;
- tooling and editor inspection;
- class and property versioning;
- replication metadata.

Class IDs used in cooked content or networking are assigned deterministically
and validated against the game/content revision. Runtime hash-table order is not
an identity scheme.

### 5. Entity data model

The base entity contains only state common to all entities:

- transient `EntityHandle`;
- optional stable serialized identity;
- target name;
- flags and tags;
- transform when the class is spatial;
- ownership information for its scene instance;
- input/output connection state where applicable.

Concrete classes use typed C++ fields for runtime data. Serialized property
lookup is not the normal per-frame access path.

For example, a door stores typed asset references, state, tuning values, safe
entity references, and its physics handle directly. See the complete door
definition in [CEngine Architecture](CORE.md#56-the-game-authoring-path).

Inheritance remains shallow. CEngine avoids a deep hierarchy that encodes every
combination of animation, damage, physics, and rendering. Reusable implementation
belongs in ordinary helper objects or game-local functions.

CEngine does not require a universal component framework. It can use internal
data-oriented stores where bulk work benefits, without changing the public
entity programming model.

#### 5.1 Storage and cost model

Entity semantics are object-oriented; storage is not one heap allocation per
object. Each registered class declares size, alignment, constructor, and
destructor operations. The simulation stores entity objects in class-specific slab
pages and keeps slots, generations, class IDs, and common flags in packed arrays.

Consequences of the default policy are:

- spawning normally consumes a free slot and space from an existing page;
- page growth occurs only at a structural safe point in a budgeted bulk
  allocation; latency-critical profiles reserve their declared peak before play;
- destroying returns storage to the class pool at a structural safe point;
- iterating one concrete class or a purpose-built protocol index walks packed
  slots, not all entities plus dynamic casts;
- target names, tags, scheduled work, and spatial membership are optional
  indexes populated only for entities that use them;
- variable-size serialized data lives in the scene payload or staging arena and
  is converted once into fixed fields;
- a scene transaction uses an arena for temporary maps, errors, and construction
  records, then releases it in bulk.

Raw entity addresses are callback-scoped and cannot be stored by game code.
`EntityHandle`/`EntityRef` are the only durable references. Slab pages do
not compact live objects; this is an implementation rule, not a public pointer
guarantee.

#### 5.2 No general capability registry

A component or capability framework does not exist beside entities. Scheduling,
replication, names, and spatial lookup may each keep one private
packed index because a current engine protocol queries them in bulk. They are
not registered through a universal callback table.

Game-specific cross-class behavior starts as a direct function, explicit entity
reference, concrete-class query, or game-local narrow interface. `DamageReceiver`
and similar labels are not engine abstractions.

#### 5.3 Spatial transforms and attachment

A spatial entity has a `WorldTransform`: `Vec3` position, rotation, and scale in
the project coordinate convention. Systems receive the same world-space values
through packed owner-specific commands.

An entity may have one explicit transform parent and a parent-local transform.
This supports carried items, vehicle occupants, moving-platform attachments, and
hierarchical props without turning the entire simulation into a scene graph.

- parenting changes occur at structural safe points and reject cycles;
- parent references use `EntityRef`, never raw pointers;
- destroying or unloading a parent follows the child's declared detach, destroy,
  or reparent policy;
- the simulation flattens dirty transform subtrees once and submits packed world
  transforms to systems;
- networked attachment replicates parent `NetEntityId`, local transform, and a
  discontinuity sequence when required;
- interpolation evaluates the parent before the attached child for the same
  presentation time.

Physics bodies declare which scale modes they support. A dynamic non-uniform
scale is rejected unless the backend contract explicitly supports it; changing
collision scale normally stages a shape/body rebuild rather than mutating a
backend pointer in place.

### 6. Identity and references

#### 6.1 Runtime identity

`EntityHandle` is a generation handle containing a slot index and generation. It is
valid only inside one simulation instance.

Destroying an entity increments its slot generation. Stale handles then fail
cleanly rather than addressing a newly allocated entity.

#### 6.2 Serialized identity

Scene data uses stable identity only where references must survive cooking or
scene reconstruction. During instantiation, a staging
transaction maps serialized identities to runtime `EntityHandle` values.

#### 6.3 Network identity

`NetEntityId` identifies one replicated game concept across server and client.
It is not interchangeable with either side's `EntityHandle`.

```text
server EntityHandle -> NetEntityId -> client EntityHandle
```

#### 6.4 Entity references

An entity reference stores a safe identifier, not an `Entity*`. References define
their semantics, including whether the target is:

- required or optional;
- resolved during scene activation or later;
- allowed to cross scene instances;
- cleared, notified, or treated as an error on target destruction;
- allowed to resolve later for a runtime-spawned target.

Target names are convenient authored lookup keys, not permanent runtime object
addresses.

### 7. Lifecycle

#### 7.1 Scene-created entities

The staged lifecycle is:

```text
validate class and properties
construct entity
assign identity and scene ownership
deserialize fixed typed fields
resolve required assets and entity references
OnSpawn
validate required staged bindings and activation
commit staged system bindings
publish scene instance
OnActivate
```

No staged entity is visible through normal simulation lookup before publication.
`OnSpawn` may fail and creates any class-specific physics, render, animation,
or audio bindings in prepared-but-invisible state. It can inspect
its typed properties, resolved references, retained assets, spawn context, and
its own staged bindings; it cannot query the public simulation or execute gameplay
against other staged entities.

Activation validation is the last fallible step. Committing prepared bindings
does not allocate or fail and occurs at the owning systems' safe point. Physics
bindings become query-visible before the simulation publishes the entities; render
and audio commands may become externally visible on their next owned frame.
`OnActivate` is a non-failing notification after publication and may only
enqueue new structural work for the next safe point. This prevents an activation
callback from exposing a half-published scene.

#### 7.2 Runtime-spawned entities

Runtime spawning uses the same construction and validation path with an explicit
spawn description. Initialization does not overwrite caller-supplied values.

Spawns requested during event delivery are deferred until a structural safe
point unless the API explicitly creates an isolated staged entity.

Ordinary game callbacks receive a tick-scoped `SimulationCommands` writer:

```text
Spawn(SpawnDesc) -> SpawnToken
Destroy(EntityHandle)
SetParent(EntityHandle or SpawnToken, EntityHandle or SpawnToken, policy)
Transfer(EntityHandle, destination ownership)
```

Commands append into the tick arena and allocate no individual command objects.
A `SpawnToken` can connect structural commands in the same buffer but cannot be
stored as a durable entity reference. At commit, the simulation validates the entire
dependency group, maps successful tokens to new `EntityHandle` values, and publishes
spawn results at the next gameplay phase. If a required spawn fails, dependent
commands fail with it rather than observing a half-created object.

Direct construction APIs exist only on the simulation implementation during scene
transactions and named structural safe points. Entity callbacks cannot force an
immediate spawn or destruction by calling a different facade.

#### 7.3 Destruction

Destruction is normally deferred:

1. mark the entity pending destruction;
2. stop accepting new scheduled work;
3. notify or clear defined relationships;
4. call `OnDeactivate` if active;
5. destroy owned runtime-system bindings;
6. call `OnDestroy`;
7. remove lookup and scene membership;
8. increment the slot generation;
9. release retained asset references.

Callbacks must tolerate a simulation already being in shutdown. Destructors do not
perform fallible game behavior.

### 8. Execution model

Entities are event-driven by default. A static prop should not incur a virtual
update call every frame.

Entity behavior can use:

- lifecycle callbacks;
- typed inputs;
- physics and animation events;
- explicit game events;
- scheduled `Think` calls;
- simulation timers.

The simulation owns the scheduler. A scheduled entry stores a safe entity identity
and token. Destroyed entities and cancelled tokens are rejected before dispatch.

Arbitrary game code runs on the simulation thread. A specialized backend may
calculate private results concurrently, but results return as commands or events
at a safe point.

#### 8.1 Phase and event rules

The authoritative phase order is defined in [CEngine Architecture](CORE.md#cengine-architecture).
Within a phase:

- entity iteration order is stable where game behavior can observe it;
- entity inputs and events append records to a simulation-owned bounded queue;
- dispatch reads a fixed prefix, so newly emitted records cannot recursively
  re-enter the dispatcher;
- spawns and destroys are recorded in a structural command buffer;
- conflicting commands resolve by documented type-specific order, with destroy
  winning over later ordinary work for the same entity;
- a callback receives a `TickContext` or `InputContext`, not a global clock or
  system locator.

Stable order means a documented key such as phase, target `EntityHandle`, source
sequence, and insertion ordinal. It does not mean relying on hash-table or
worker completion order.

#### 8.2 Query rules

The ordinary APIs are intentionally small:

```text
Resolve(EntityHandle) -> EntityView or null
FindByName(name) -> bounded view of EntityHandle
ForEachClass<T>(callback)
QuerySpatial(bounds, mask, output span) -> count/status
```

Queries return IDs, short-lived views, or write into caller-provided storage.
They do not allocate a container per call. A view expires at the next structural
safe point unless its API states a shorter lifetime. Systems use their own
spatial indexes for physics and rendering; the simulation maintains only the
gameplay spatial index justified by gameplay queries and partitioning.

### 9. Entity input and output

Entity I/O lets authored entities communicate without hard-coded pointers.

An output connection contains:

- source entity;
- output name;
- target entity reference or target-name query;
- input name;
- typed or validated parameter data;
- optional delay;
- optional fire-count policy.

```text
trigger_exit.OnTriggered
  -> alarm_relay.Enable
  -> door_lock.Lock after 0.25 seconds
```

Connections are validated against registered class descriptors during cooking
where possible and again during scene instantiation. Delayed delivery uses safe
identities and is cancelled or ignored when a target no longer exists.

A target-name connection declares its cardinality and resolution time:

- `exactly_one` must resolve to one target or validation/delivery fails;
- `all_at_fire_time` resolves the current matching set when the output fires;
- `first_stable` selects the lowest stable serialized identity, then `EntityHandle`
  for runtime-only targets.

Multi-target delivery is sorted by stable identity/`EntityHandle`. Hash-table order
is never observable. A delayed connection either captures resolved `EntityRef`
values or re-runs its declared query at delivery; the format states which.

Network policy is explicit. Server entity I/O does not automatically run on the
client. Observable results are replicated or emitted as defined game events.

Entity I/O is for authored, relatively low-frequency orchestration. It is not a
general per-frame message bus. Hot paths such as damage batches, contacts,
render extraction, AI perception, and replication use typed packed records and
dedicated APIs.

### 10. Game rules and player model

Game-wide policy can live in simulation-owned game rules rather than being
forced into a placed entity. Examples include match phase, team policy, scoring,
and allowed player classes.

Use an entity when designers need to place, name, configure, connect, or
reference the behavior in a scene. Use game rules when the behavior is an
invariant of the selected game mode.

For multiplayer, separate:

```text
PlayerConnection
  network participant and command stream

PlayerState
  logical identity, team, score, session inventory

PlayerPawn entity
  physical avatar, transform, health policy, collision, active presentation
```

`PlayerState` survives pawn destruction and respawn for the session. Health,
ammunition, and inventory placement is game policy: data required to survive a
new pawn belongs in `PlayerState`; physical-life state normally belongs in the
pawn.

### 11. Runtime-system bindings

Entities call backend-neutral system interfaces and can retain opaque handles.
Systems own the actual objects.

```text
ServerProp
  PhysicsBodyHandle -> PhysicsSystem-owned body

ClientProp
  RenderObjectHandle -> RenderSystem-owned object
  AnimationInstanceHandle -> AnimationSystem-owned instance
  AudioEmitterHandle -> AudioSystem-owned emitter state

```

There is no fixed global record containing one handle for every possible system.
That design fails for entities with multiple shapes, render objects, emitters,
or effects and forces every new system into one central type.

When a system must report an event, its front-end record stores a safe owner
token such as `EntityHandle`. Backends never call entity code.

Bindings are normally created once and updated through buffered typed commands.
Per-tick updates for many objects are submitted as spans from simulation-owned scratch
storage. The simulation does not issue one allocating virtual message per transform.

Destroying a binding invalidates its public handle at the owning system's next
safe point. The system, not the entity, retains any asset payload or backend
resource required until outstanding physics steps, audio callbacks, or GPU
frames retire. Entity destruction can therefore release its own `AssetRef`
values without guessing a backend fence.

### 12. Scene ownership

Every scene-created entity records its owning `SceneInstanceId`. Removing the
instance destroys those entities. Session-level game rules, connections, and
`PlayerState` are simulation-owned rather than hidden in the scene.

### 13. Failure behavior

- An unknown classname fails scene instantiation unless the content explicitly
  permits a non-gameplay placeholder.
- Invalid required properties or references fail before publication.
- Failure to create a required system binding rolls back the staged entity set.
- Failure during binding preparation leaves every binding invisible and rolls
  back the transaction; binding commit itself is non-failing.
- Optional presentation dependencies use declared fallbacks.
- A failing entity callback is reported with entity identity, classname, scene,
  and lifecycle phase.

### 14. Invariants

- One live entity belongs to exactly one simulation.
- One `EntityHandle` is meaningful only in its owning simulation.
- Server and client entities never share live memory.
- A concrete entity class is registered before its scene data is instantiated.
- Built-in and game entity classes use the same construction path.
- Entity behavior never owns backend objects or backend pointers.
- Structural changes occur at defined safe points.
- Scene rollback exposes no partially activated entities.
- Static entities incur no mandatory per-frame callback.
- One entity does not require one general-purpose heap allocation.
- Observable behavior never depends on hash iteration or worker completion
  order.
- Entity I/O is not used as a bulk simulation or rendering transport.
- `OnActivate` cannot fail after a scene has been published.
- Structural commands can refer to same-buffer spawns only through transient
  `SpawnToken` values.
- Target-name resolution has declared cardinality, timing, and stable order.

---

## Client and Server Architecture

> **Status: detailed target contract.** The active implementation begins with an
> authoritative simulation and lightweight `ClientView`; full paired runtimes are
> promoted by `IMPLEMENTATION.md`.

### 1. Purpose

This document defines the boundary between CEngine, shared game code, the
authoritative game server, and the presentation client. It covers module
responsibilities, hosting configurations, simulation ownership, and
single-player behavior.

### 2. Design decision

CEngine uses this split:

```text
GameShared
GameServer module
GameClient module
```

The server and client are separate game runtimes. They can execute in different
processes or in one process. They communicate through a transport abstraction
and never share live entity objects.

Multiplayer supplies the proof for the split: an untrusted remote process sends
intent across a delayed channel, while one process must decide the outcome.
Prediction also needs a rewindable client timeline, and headless hosting must
not acquire presentation state. Separate mutable runtimes are the smallest
model that represents those facts directly.

Single-player retains the logical split to avoid a second set of game rules.
This is a deliberate implementation cost: paired representations and snapshot
application remain. The in-process path repays that cost by sharing immutable
payloads and moving validated records without packet encoding or an OS hop.

`GameShared` is a statically linked library used by both modules, not a third
mutable runtime. `GameServer` and `GameClient` are also linked into their host
executables.

### 3. Module responsibilities

#### 3.1 GameShared

`GameShared` contains definitions required identically by both sides:

- network protocol and schema versions;
- `UserCommand`, `ClientRequest`, and common input values;
- `NetEntityId` and stable network class IDs;
- replicated-property layouts;
- asset and scene identifiers;
- damage, team, weapon, and movement value types;
- deterministic movement code used for server simulation and client prediction;
- common constants derived from cooked game data.

It must not contain:

- live authoritative or client simulation state;
- graphics, audio, or user-interface code;
- server-only decisions disguised by client/server preprocessor branches;
- process-global mutable game state;
- backend types.

#### 3.2 GameServer

`GameServer` owns authoritative game meaning:

- server entity registration;
- game rules and match state;
- player connections' association with logical player state;
- authoritative spawning, damage, inventory, AI, and entity I/O;
- server scene instantiation;
- replication descriptors, relevance, and snapshot production;
- validation of user commands and client requests.

The server module does not render, mix audio, own a local HUD, or trust a
client-reported game outcome.

#### 3.3 GameClient

`GameClient` owns player-facing behavior:

- client entity registration;
- local input interpretation and user-command production;
- discrete client-request production and result presentation;
- prediction and reconciliation;
- remote-entity interpolation;
- cameras and view behavior;
- render, animation, audio, particle, and UI integration;
- client scene instantiation;
- presentation of replicated game events;
- client-only entities and effects.

The client does not authoritatively grant items, apply damage, determine hits,
or choose replicated spawn outcomes.

### 4. Hosting configurations

#### 4.1 Dedicated server

```text
DedicatedServer executable
  Foundation
  AssetService
  NetworkSystem
  server PhysicsSystem
  GameShared
  GameServer
  ServerSimulation
```

The dedicated server excludes rendering, audio, UI, and client animation. Its
asset configuration can exclude presentation-only payloads.

#### 4.2 Remote client

```text
Game executable
  Foundation and Platform
  AssetService
  NetworkSystem
  InputSystem
  optional prediction PhysicsSystem
  AnimationSystem
  AudioSystem
  RenderSystem
  GameShared
  GameClient
  ClientSimulation
```

#### 4.3 Listen server

A listen server hosts both modules and both simulations. It still communicates
through an in-process transport endpoint.

```text
one process
  ServerSimulation <-> in-process transport <-> ClientSimulation
```

No direct entity pointers cross between the simulations. This preserves parity
with remote multiplayer and prevents accidental client access to authority.

#### 4.4 Single-player

The standard single-player configuration is a local server plus local client.
It uses the same command, snapshot, and scene protocols with an optimized
in-process transport.

This provides:

- consistent authority semantics;
- multiplayer-ready game entities;
- dedicated-server parity;
- deterministic capture and replay opportunities;
- one scene/network behavior path to test.

Single-player may tune latency to zero, share immutable asset payloads, and
transfer packed commands/snapshots without byte serialization. It may not share
mutable simulations, skip command validation, construct networked
client entities from server pointers, or apply authoritative changes directly
from client code.

### 5. Module interface

Game modules link statically and each exposes one registration/composition
function:

```cpp
namespace CEngine::Game {

[[nodiscard]] Status RegisterServer(ServerModuleContext& context);
[[nodiscard]] Status RegisterClient(ClientModuleContext& context);

} // namespace CEngine::Game
```

Each context exposes only the registries and facades valid for that side.
Simulation objects and system facades are supplied during startup; modules do
not fetch them from globals.

### 6. Startup

#### 6.1 Server startup

1. Start foundation, diagnostics, assets, transport, and server physics.
2. Load `GameShared` protocol/schema data.
3. Register `GameServer`.
4. Register engine and game server entity classes.
5. Create `ServerSimulation` and install game rules.
6. Load and instantiate the initial server scene.
7. Open connections and begin fixed ticks.

#### 6.2 Client startup

1. Start platform, input, assets, transport, prediction physics, animation,
   audio, and rendering as configured.
2. Load `GameShared` protocol/schema data.
3. Register `GameClient`.
4. Register engine and game client entity classes.
5. Create `ClientSimulation`.
6. Connect to a remote or in-process server.
7. Validate protocol and content compatibility.
8. Load the scene selected by the server and apply the initial snapshot.

Startup contexts are temporary and cannot be stored. Registered descriptors and
callbacks are destroyed before their statically linked game-owned state.

### 7. Networked entity pairing

One network class describes the protocol relationship between server and client
representations.

```text
Authored classname: player
NetworkClassId: Player

Server factory -> ServerPlayer
Client factory -> ClientPlayer
```

The representations need not share fields or inheritance. They share only the
versioned data contract required for replication and prediction.

Some classes exist only on one side:

| Kind | Examples |
| --- | --- |
| Server-only | triggers, AI hints, spawn directors, hidden logic relays |
| Client-only | viewmodels, impact effects, HUD controllers, transient visual effects |
| Scene-derived on both sides | static props, selected environment state |
| Networked pair | players, NPCs, doors, pickups, moving platforms |

### 8. Player ownership

The server owns the association:

```text
ConnectionId -> PlayerStateId -> possessed server EntityHandle
```

The client owns its presentation association:

```text
local connection -> local player presentation -> client EntityHandle
```

The server decides which pawn a player possesses. A client requests actions for
its assigned player; it cannot select arbitrary authoritative entities.

Connection, logical player state, and pawn lifetime remain separate. Disconnect
policy determines whether player state is destroyed immediately, retained for a
reconnect window, or converted to AI control.

### 9. Shutdown

Shutdown order is:

1. stop accepting new connections or commands;
2. stop game ticks and presentation callbacks;
3. deactivate scene instances;
4. destroy simulations and entity factories;
5. destroy game-owned registration and state;
6. stop systems and shared services.

### 10. Failure behavior

- Game registration rejects incompatible protocol, schema, or content versions.
- Game startup is transactional; failure destroys all objects created by that
  attempt.
- No game callback is invoked after game shutdown begins.
- Dedicated servers continue to produce useful diagnostics without client UI.
- A client module crash or disconnect cannot mutate server authority.

### 11. Invariants

- Server and client modules have separate simulations and entity registries.
- The server is authoritative for game outcomes.
- Single-player does not bypass the authority boundary.
- `GameShared` contains no live simulation state.
- Game code is statically linked.

---

## Engine Orchestration

> **Status: complete target orchestration.** The active loop is the smaller loop
> in `IMPLEMENTATION.md`. Do not instantiate deferred owners merely because they
> appear in the examples below.

### 1. Purpose

This document shows the top-level C++ shape of CEngine. The examples are
normative about ownership, phase order, data movement, and lifetimes. Names may
gain implementation detail, but systems must continue to interact through the
same commands, results, and immutable snapshots.

The host is the only composition root. A system never discovers another system
through a global registry, and a worker never calls entity code.

All C++ examples target C++17 and follow the repository's `.clang-format`.
Engine-owned types live in `CEngine`; game protocol and gameplay types live in
`CEngine::Game`.

### 2. Composition

This listen-server example contains both runtime sides. A dedicated server omits
the client block; a remote client omits the server block.

```cpp
namespace CEngine {

class EngineHost final {
  public:
    [[nodiscard]] Status Start(const RuntimeConfig& config);
    void RunFrame() noexcept;
    void Stop() noexcept;

  private:
    [[nodiscard]] Result<SceneInstanceId> ActivateServerScene(const AssetReference& reference);
    void TickServer(SimulationTick tick) noexcept;
    void TickClient(const FrameTime& frame) noexcept;

    Diagnostics diagnostics_;
    PlatformServices platform_;
    AssetService assets_;
    NetworkSystem network_;

    PhysicsSystem server_physics_;
    ServerSimulation server_;

    InputSystem input_;
    PhysicsSystem client_physics_;
    AnimationSystem client_animation_;
    AudioSystem audio_;
    RenderSystem renderer_;
    ClientSimulation client_;

    TickArena server_tick_arena_;
    SnapshotArena snapshot_arena_;
    FrameArena client_frame_arena_;
};

} // namespace CEngine
```

Members appear in dependency order and shut down in reverse order. The
simulations receive fixed typed references to the facilities valid for their
side during construction. They do not retain an `EngineHost*`.

The server and client members above are configuration-owned blocks, not a rule
that every executable constructs every system. Product composition removes
unused systems entirely.

### 3. Outer frame

The outer frame advances zero or more fixed server ticks and exactly one client
presentation frame. It never changes the fixed tick duration to catch up.

```cpp
namespace CEngine {

void EngineHost::RunFrame() noexcept {
    const FrameTime frame = platform_.BeginFrame();

    platform_.PumpEvents();
    const ActionSnapshot actions = input_.Sample(frame);
    const Game::UserCommand command = client_.BuildUserCommand(actions);

    client_.Predict(command);
    network_.Send(command);
    network_.Poll();

    server_.Clock().Accumulate(frame.real_delta);

    while (const std::optional<SimulationTick> tick = server_.Clock().TakeTick()) {
        TickServer(*tick);
    }

    network_.Poll();
    TickClient(frame);
    platform_.EndFrame();
}

} // namespace CEngine
```

`TakeTick` is bounded by the configured catch-up limit and records overload when
the simulation remains late.

### 4. Server tick

The host moves packed data between owners. `ServerSimulation` is the only owner
that invokes game rules or entities.

```cpp
namespace CEngine {

void EngineHost::TickServer(const SimulationTick tick) noexcept {
    server_tick_arena_.Reset();
    snapshot_arena_.Reset();

    const ServerInbox inbox = network_.DrainServerInbox(server_tick_arena_);

    server_.BeginTick(ServerTick{tick, server_.Clock().FixedDelta()}, inbox);
    server_.CommitPendingChanges();

    const ServerGameplayOutput gameplay = server_.RunGameplay(server_tick_arena_);

    server_physics_.Apply(gameplay.physics_commands);
    server_physics_.Apply(server_.DrainPhysicsCommands(server_tick_arena_));
    server_physics_.Step(server_.Clock().FixedDelta());

    const PhysicsStepOutput physics = server_physics_.DrainStepOutput(server_tick_arena_);
    server_.ApplyPhysics(physics.body_states, physics.events);
    server_.RunPostPhysics(server_tick_arena_);
    server_.CommitPendingChanges();

    const ReplicationCapture capture = server_.CaptureReplication(snapshot_arena_);
    const OutboundSnapshots snapshots = server_.Replication().BuildSnapshots(
        capture, network_.ServerConnections(), server_tick_arena_);
    network_.Send(snapshots);
    network_.FlushServerOutput();
}

} // namespace CEngine
```

All returned views are arena-backed and expire at the documented reset. Physics,
replication, and transport never retain them without copying into their own
bounded storage.

### 5. Client frame

The client applies network state, replays prediction, evaluates presentation,
then submits immutable work to animation, audio, and rendering.

```cpp
namespace CEngine {

void EngineHost::TickClient(const FrameTime& frame) noexcept {
    client_frame_arena_.Reset();

    const ClientInbox inbox = network_.DrainClientInbox(client_frame_arena_);
    client_.ApplySnapshots(inbox.snapshots);
    client_.ApplyRequestResults(inbox.request_results);
    client_.ReconcilePrediction(inbox.command_acknowledgements);
    client_.Interpolate(frame.presentation_time);

    const AnimationCommands animation_commands =
        client_.BuildAnimationCommands(client_frame_arena_);
    client_animation_.Apply(animation_commands);
    const AnimationFrame animation =
        client_animation_.Evaluate(frame.presentation_delta, client_frame_arena_);

    client_.ApplyAnimationEvents(animation.events);
    const ClientPresentationOutput presentation =
        client_.BuildPresentation(frame, animation.attachments, client_frame_arena_);

    audio_.Submit(presentation.audio_commands);
    audio_.SubmitSpatialFrame(presentation.audio_spatial_frame);

    const RenderSubmission submission{presentation.render_updates, presentation.light_updates,
                                      animation.skinning_palettes, presentation.render_frame};
    renderer_.Submit(submission);
}

} // namespace CEngine
```

`Submit` bulk-copies the spans into the next preallocated render-owned packet and
signals the Render Thread. The render frame contains cameras, environment state,
UI, and transient draws. Neither the submission nor retained updates contain an
entity pointer.

### 6. Scene activation

Scene activation prepares all fallible work before changing public state.

```cpp
namespace CEngine {

Result<SceneInstanceId> EngineHost::ActivateServerScene(const AssetReference& reference) {
    Result<AssetRef<SceneAsset>> scene = assets_.Acquire<SceneAsset>(reference);
    if (!scene) {
        return scene.Error();
    }

    ServerSceneTransaction transaction{server_, server_physics_, scene.TakeValue()};

    const Status prepared = transaction.Prepare();
    if (!prepared) {
        return prepared;
    }

    return transaction.Commit();
}

} // namespace CEngine
```

`Prepare` retains assets, constructs staged entities, calls `OnSpawn`, and
prepares invisible physics bindings. `Commit` performs no allocation,
cannot report recoverable failure, publishes bindings at a safe point, and then
publishes the `SceneInstance`. Client activation follows the same transaction
with render and presentation bindings added.

### 7. Interaction rules

- The host schedules; simulations own game meaning; systems own specialized
  state.
- Cross-owner data is a typed value, handle, span, command buffer, result, event,
  or immutable snapshot.
- A returned span documents its owner and expiration point.
- Systems never retain simulation/entity pointers.
- Simulation callbacks never call backend APIs.
- One owner logs the terminal failure; intermediate layers return `Status`.
- Per-tick and per-frame methods do not allocate from the general heap.

---

## Runtime Services

> **Status: detailed target contract.** The active milestone uses one main owner
> thread; a backend may add internal workers only when the active contract permits
> them. The complete Game/Render/Audio ownership model below is a promoted target.

### 1. Purpose

This document defines the small set of cross-cutting runtime facilities:
platform access, clocks, diagnostics, memory ownership, startup, and shutdown.

These facilities remain narrow. They do not become a global service locator for
arbitrary game behavior.

### 2. Platform services

Platform services isolate operating-system and window-library behavior. They
provide:

- process and application lifecycle;
- windows, displays, and presentation surfaces;
- monotonic clocks and sleep/wake primitives;
- filesystem and cooked-storage access;
- thread primitives and CPU topology;
- device and platform event pumping;
- crash-reporting hooks;
- platform networking and audio-device adapters.

GLFW and OS-specific types remain behind these interfaces. A dedicated server
can use a headless platform configuration with no window or graphics surface.

### 3. Concurrency policy

CEngine uses a Game/Render/Audio thread-ownership model:

| Execution context | Sole mutable ownership |
| --- | --- |
| Game Thread | platform pump, input, server/client simulations, networking, animation, scene staging |
| Render Thread | retained render scene, graphics API/device, frame execution, presentation |
| Audio callback | mixer, active voices, and audio device buffers |
| Jolt backend workers | physics-owned step data only, joined before `Step` returns |

A dedicated server normally has only the Game Thread plus Jolt's private workers.
A listen server advances server ticks and the client frame sequentially on the
same Game Thread. Nonblocking socket I/O and asset staging also run there.

The Game Thread publishes one immutable, preallocated render submission. The
Render Thread may run at most one frame behind. If the next submission slot is
still in use, the Game Thread waits at the explicit frame boundary rather than
allocating a third packet or racing render state. The Render Thread never reads
entities or client-simulation memory. Retired frame and resource sequence
numbers tell the Game Thread when storage can be reused or destroyed.

The Render Thread calls the compiled graphics backend directly.

The Audio callback consumes a bounded single-producer/single-consumer command
ring and immutable spatial snapshot. It never waits on the Game or Render
Thread. Complete audio assets are decoded before play.

Parallel execution belongs to the subsystem that owns the data. Simulation,
asset staging, animation, render extraction, and transport processing remain on
the Game Thread. Jolt may use workers inside its backend because those workers
touch only physics-owned state.

Criteria for adding subsystem-local parallel work are defined in
[Tradeoffs and promotion](DELIVERY.md#tradeoffs-and-promotion).

Normal gameplay has only two cross-thread paths: Game-to-Render frame
submission and Game-to-Audio commands. Waiting is allowed only for physics-step
completion, bounded render backpressure at the frame boundary, startup,
shutdown, and explicit loading screens. Thread assertions guard every owning
facade in debug builds.

### 4. Time and clocks

CEngine distinguishes:

- monotonic real time;
- client presentation/frame time;
- fixed server simulation ticks;
- client predicted ticks;
- audio device time;
- network timestamp/round-trip estimates.

Game and protocol code use explicit clock/tick types. Raw floating-point seconds
from unrelated clocks are not silently mixed.

The server controls authoritative simulation tick progression. The client maps
presentation time to received server state for interpolation.

### 5. Diagnostics

Diagnostics are an engine-wide facility with structured records:

- severity and category;
- message and stable diagnostic ID;
- subsystem and phase;
- asset, entity, scene, connection, frame, or tick context;
- thread and source context when appropriate.

Logging must work in headless servers and early startup. No diagnostic path can
depend on the renderer or arbitrary game callbacks.

Profiling supports:

- CPU scopes, threads, and phase relationships;
- frame and fixed-tick markers;
- GPU timing through RenderSystem;
- memory/residency counters;
- physics and animation workload counters;
- network bandwidth, packet, snapshot, and correction counters;
- audio starvation and command-queue counters.

Assertions report invariant violations. Recoverable content and runtime failures
use explicit results and diagnostics rather than assertion-only handling.

### 6. Memory ownership

Each system owns its long-lived allocations. Cross-system APIs use values,
explicit views, immutable references, or opaque handles.

Memory classes include:

- persistent system/simulation storage;
- asset payload storage;
- frame/tick transient arenas;
- GPU memory;
- audio real-time buffers;
- network packet/snapshot buffers.

Every view documents its lifetime. A frame arena cannot back a command consumed
after that frame.

#### 6.1 Allocation policy

The engine does not solve allocation by passing an allocator object through
every API. Ownership determines the storage strategy:

| Lifetime/workload | Default storage |
| --- | --- |
| entities and handle records | class/system slab pages plus generation slots |
| one tick or frame | double-buffered linear arena |
| events and cross-thread commands | retained bounded ring or paged queue |
| snapshots and packets | size-classed pages with per-connection budgets |
| immutable asset payload | one/few contiguous payload allocations |
| variable long-lived game data | owner container with explicit budget |

After warm-up, recurring gameplay, render extraction, physics contacts,
animation evaluation, audio callbacks, and packet processing make no
general-purpose heap allocation calls. Capacity is reserved in owner-controlled
pages, rings, pools, and arenas. APIs accept spans and output buffers for bulk
results. An occasional large allocation during scene loading is preferable to a
permanent maze of tiny objects, provided it occurs in staging, is budget-checked
before publication, and never leaks into the recurring path.

Capacity misses are observable. A caller can reserve at load time; at runtime a
queue either rejects, truncates only where semantically safe, drops a prioritized
cosmetic record, or fails/disconnects. It never silently falls back to unlimited
heap growth.

### 7. Startup

The host starts dependencies before consumers:

1. bootstrap diagnostics and crash handling;
2. platform and clocks;
3. memory facilities and synchronous `AssetService`;
4. network transport;
5. configured physics, input, animation, audio, and rendering
   systems;
6. statically linked shared and game registration;
7. entity factories and simulations;
8. initial scene and network session.

Every stage either completes or rolls back the objects it created. Startup does
not publish a partially initialized top-level system.

### 8. Shutdown

Shutdown reverses dependency order:

1. stop accepting new connections, asset work, and external requests;
2. stop game ticks, client frames, and new callbacks;
3. deactivate scene instances and destroy simulations;
4. destroy entity factories and module-created objects;
5. stop rendering, audio, animation, physics, and input;
6. close network sessions and transport;
7. release asset references;
8. destroy windows, devices, and platform state;
9. flush final diagnostics.

No callback can target an object or module after its shutdown phase.

### 9. Thread ownership summary

| Domain | Primary owner |
| --- | --- |
| `ServerSimulation` | Game Thread |
| `ClientSimulation` | Game Thread |
| Platform events and input | Game Thread/platform main thread |
| Asset read/decode | Game Thread during staging |
| Physics facade | Game Thread; Jolt workers exist only inside `Step` |
| Animation evaluation | Game Thread |
| Graphics device | Render Thread |
| Audio device callback | real-time audio callback |
| Network I/O | Game Thread nonblocking poll |

Threading rules belong to public facade contracts. A function is not implicitly
thread-safe merely because it uses a handle.

### 10. Configuration and feature composition

Runtime configuration selects systems without changing game-domain ownership:

- dedicated server excludes presentation systems;
- remote client excludes authoritative server simulation;
- listen/single-player hosts both sides;
- tools can host assets and rendering without a networked game;
- test configurations can use null platform, render, audio, or transport
  backends where their behavior is explicitly defined.

Feature absence is visible during composition. Game code does not repeatedly
probe an unstructured global service locator.

Configuration is frozen into a typed `RuntimeConfig` before systems start.
Frequently changed game tuning belongs in versioned game data, not in a generic
string-key configuration lookup on hot paths.

### 11. Verification

The architecture must be testable without graphics or a live network. The host
can run a server simulation with null transport and deterministic test clocks, feed
recorded commands, and capture declared state hashes after each tick.

The deterministic command-capture test records only declared simulation inputs:

- initial scene/content revision and game configuration;
- accepted `UserCommand` and `ClientRequest` streams plus authoritative external
  events;
- initial seeds for named random streams;
- tick rate and protocol/schema version.

Randomness affecting authority comes from simulation-owned named streams and is
part of prediction state where relevant. Code never uses wall-clock time,
thread-local randomness, pointer values, or worker completion order as game
input. Whole-simulation bitwise determinism across all platforms is not promised;
the test reports the first divergent declared state field and tick.

Required architecture-level tests include:

- scene transaction rollback at every fallible stage;
- stale generation handles and `NetEntityId` reuse;
- snapshot loss, reorder, duplicate, baseline expiry, and sequence wrap;
- prediction restore/replay and presentation-event deduplication;
- queue, history, memory-budget, and system-device failure paths;
- single-player and remote-client parity for the same command capture.

### 12. Failure behavior

- Platform startup failure stops before dependent systems are created.
- A backend-worker failure is captured into its owning system result.
- Logging remains available during subsystem teardown.
- Public-facade shutdown is idempotent.
- Timeouts and forced shutdown paths report which work failed to retire.

### 13. Invariants

- The application is the composition root, not a game-logic container.
- Each system owns its long-lived state and documents thread access.
- Parallel work remains private to the subsystem that owns its data.
- Simulation mutation begins on one owning thread.
- Startup and load operations publish only complete state.
- Shutdown removes callbacks before destroying their dependencies.
- Dedicated server operation does not require graphical or audio facilities.
- Recurring hot paths make no general-purpose heap allocation calls after warm-up.
- Authoritative randomness and time enter through explicit simulation-owned sources.
- Core simulation and replication can be verified headlessly from recorded
  inputs.
