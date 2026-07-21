# CEngine Architecture

## 1. Executive summary

CEngine is a modular runtime for building 3D games. It provides the common
facilities that a game needs, including world simulation, asset management,
rendering, physics, animation, audio, input, and platform access. A game extends
the engine with its own entities and game rules instead of modifying engine
internals.

The architecture has four central ideas:

1. **`GameWorld` owns the live game simulation.** It owns entities, game state,
   entity relationships, and game behavior.
2. **Entities are the main game-programming model.** CEngine provides standard
   entity classes. Each game registers additional entity classes for its own
   authored behavior.
3. **Systems perform specialized runtime work.** Rendering, physics, animation,
   audio, and input use narrow APIs and own their specialized state.
4. **Assets are immutable authored data.** The `AssetManager` loads scenes,
   meshes, materials, textures, sounds, collision data, and other content. A
   scene is an asset whose payload describes entities and their connections.

These boundaries keep game code expressive while preventing backend details
from spreading through the engine. They also permit individual systems to grow
without forcing the engine to adopt unnecessary global complexity.

## 2. Design goals

CEngine must support the complete runtime needs of a modern 3D game while
remaining understandable to a small engineering team.

The architecture optimizes for:

- a clear extension model for game developers;
- explicit ownership and lifetime;
- deterministic construction and safe failure;
- efficient bulk work in performance-critical systems;
- cooked, validated runtime content;
- replacement of graphics, physics, audio, and platform backends;
- incremental growth based on measured requirements;
- tools that use the same entity and asset definitions as the runtime.

The architecture does not attempt to make every feature generic. CEngine uses a
deliberately direct, Source-style entity model. It favors concrete game concepts,
shallow abstractions, and explicit APIs over a universal component framework.

## 3. Architecture at a glance

The application is the composition root. It constructs the engine, installs the
game module, selects the active game rules, and controls the outer frame loop.

```text
Game application
  |
  +-- Game module
  |     +-- game entity classes
  |     +-- game rules
  |     `-- game-specific services when required
  |
  +-- GameWorld
  |     +-- live entities
  |     +-- entity lifecycle and lookup
  |     +-- entity input/output and events
  |     +-- timers and deferred changes
  |     `-- active scene instances
  |
  +-- Runtime systems
  |     +-- InputSystem
  |     +-- PhysicsSystem
  |     +-- AnimationSystem
  |     +-- AudioSystem
  |     +-- RenderSystem
  |     `-- NetworkSystem (optional)
  |
  `-- Shared services
        +-- AssetManager
        +-- SaveService
        +-- JobSystem
        `-- Platform services
```

### 3.1 Systems, services, and subsystems

A **system** owns specialized runtime state and advances it on a frame, fixed
tick, or device schedule. Examples are physics simulation and rendering.

A **service** performs work in response to requests and does not define the game
simulation schedule. Examples are asset acquisition and save-data processing.

A **subsystem** is an implementation area inside a larger system or service. It
does not need a top-level facade unless it gains an independent lifetime,
schedule, or ownership boundary.

This vocabulary describes execution and ownership. It does not require every
feature to become a separate top-level object.

### 3.2 Top-level responsibilities

| Owner | Primary responsibility | Owns |
| --- | --- | --- |
| Application | composition and outer schedule | engine instances, game module, window, active world |
| `GameWorld` | authoritative game simulation | entities, game state, relationships, timers, scene instances |
| `AssetManager` | asset identity and CPU residency | asset records, dependencies, immutable published payloads |
| `InputSystem` | device-to-action translation | devices, bindings, focus, action state |
| `PhysicsSystem` | physical simulation | bodies, shapes, constraints, queries, collision events |
| `AnimationSystem` | pose evaluation | animation instances, state machines, poses, animation events |
| `AudioSystem` | sound reproduction | mixer, voices, streams, device state |
| `RenderSystem` | image production | render scene, GPU resources, visibility, frame execution |
| `NetworkSystem` | optional network simulation support | transport, replication, prediction, reconciliation |

## 4. The game programming model

### 4.1 `GameWorld` is the game-domain boundary

One `GameWorld` represents one live game simulation. It is authoritative for
game identity, entity lifetime, normal game properties, and game rules.

`GameWorld` provides:

- entity creation, lookup, and deferred destruction;
- stable names and safe runtime entity identifiers;
- scene instantiation and removal;
- entity-to-entity references;
- entity inputs, outputs, and typed events;
- scheduled entity thinking and world timers;
- safe points for structural changes;
- game-rule integration;
- routing between entity behavior and runtime systems;
- save/restore traversal of game-owned state.

`GameWorld` is the top-level container for game business logic. It is not a
container for graphics APIs, physics backend objects, file I/O, or audio-device
code. Those implementation domains remain in specialized systems and services.

### 4.2 Entities are the primary extension mechanism

An entity is a named, addressable game object with a concrete meaning. Entities
are the main vehicle through which authored scenes add state and behavior to a
game.

Physical entities include props, doors, characters, lights, and triggers.
Logical entities include relays, timers, counters, environment controllers, and
game-mode coordinators. Marker entities include player starts, camera points,
and navigation hints.

CEngine supplies a standard entity library. A game module registers additional
classes that implement its own vocabulary.

```text
Engine entity classes              Game entity classes
---------------------              -------------------
prop_static                        npc_security_guard
prop_dynamic                       item_health_station
light                              trigger_capture_zone
camera                             logic_round_manager
trigger                            info_team_spawn
logic_relay                        boss_controller
info_player_start                  weapon_gravity_device
```

Built-in and game-defined entities follow identical runtime rules. The scene
instantiation path does not contain special branches for each engine or game
classname.

### 4.3 Entity registration

The application creates an `EntityFactory`. CEngine registers its standard
classes, and the game module registers its classes before a world loads content.

```cpp
EntityFactory factory;
RegisterEngineEntities(factory);
RegisterGameEntities(factory);
```

An entity-class descriptor contains:

- a canonical classname;
- a constructor;
- a versioned property schema;
- property serialization and validation metadata;
- optional editor-facing metadata.

The descriptor lets runtime loading and authoring tools understand a class
without hard-coding it into the scene format. The initial implementation can
start with constructor and property-binding metadata, then add richer editor
metadata without changing the runtime model.

### 4.4 Entity shape and inheritance

`Entity` contains fields shared by every entity:

- transient `EntityId`;
- stable serialized identity when required;
- target name;
- flags and tags;
- transform;
- input/output connections.

Concrete entity classes use fixed, typed C++ fields for their runtime state.
Class inheritance remains shallow. Reusable implementation details should use
helper objects or narrow capability interfaces instead of deep behavioral
inheritance.

CEngine does not require a universal entity-component framework. A new entity
class is appropriate when it represents a distinct concept that designers can
place, configure, reference, or connect in a scene.

### 4.5 Entity execution

Entities are event-driven by default. Static entities do not receive an
unnecessary update call each frame.

Common lifecycle and behavior operations include:

```text
OnSpawn
OnActivate
OnInput
OnPhysicsEvent
OnAnimationEvent
Think at a scheduled time
OnDeactivate
OnDestroy
```

`GameWorld` schedules thinking only for entities that request it. It defers
spawning and destruction during unsafe iteration or event delivery. Safe
generation identifiers reject events and timers for destroyed entities.

### 4.6 Game rules and non-entity game code

Entities are the primary scene-facing extension model, but not all game code
must be an entity. Match rules, player-session management, team state, scoring,
and other world-wide policies can be owned by a game-rules object installed in
`GameWorld`.

Use this distinction:

- If a designer must place, name, configure, connect, or reference it in a
  scene, it is usually an entity.
- If it is an invariant policy of the selected game mode, it is usually game
  rules or another world-owned game object.
- If it performs specialized bulk work or owns backend resources, it belongs in
  a runtime system.

## 5. Assets and content

### 5.1 Asset model

An asset is identified, storage-backed, authored data. Source content such as
Blender files and source images is converted into validated target formats
before runtime use.

The `AssetManager` provides one typed interface for every runtime asset type:

```text
Acquire<T>(AssetId) -> AssetRef<T>
Request<T>(AssetId) -> AssetFuture<T>
Reload(AssetId) -> result
```

It owns:

- asset identity and type;
- canonical project paths and stable asset IDs;
- loading and validation;
- dependency tracking;
- asynchronous request state;
- immutable payload publication;
- CPU-memory residency and budgets;
- asset revisions used by hot reload.

An `AssetRef<T>` keeps a published asset payload alive. Type mismatches fail
before a caller receives a reference.

The `AssetManager` does not own GPU objects, physics bodies, or audio-device
voices. The corresponding systems own those runtime resources.

### 5.2 Runtime asset types

CEngine can support these target asset categories:

| Asset | Content |
| --- | --- |
| `SceneAsset` | entity declarations, settings, connections, and dependencies |
| `MeshAsset` | geometry, bounds, submeshes, and LODs |
| `MaterialAsset` | material parameters and texture references |
| `TextureAsset` | texture metadata, pixels, and mip levels |
| `CollisionAsset` | triangle meshes, convex shapes, or compounds |
| `SkeletonAsset` | hierarchy and bind-pose data |
| `AnimationAsset` | clips, curves, root motion, and events |
| `AudioAsset` | encoded samples, stream metadata, and loop information |
| `NavigationAsset` | baked navigation data |
| `ShaderAsset` | cooked shader programs and reflection data |

A cohesive model definition may reference mesh, material, collision, skeleton,
and animation assets. This is reusable content, not a nested collection of live
entities.

### 5.3 Dependency readiness

Not every dependency must block the same phase. Each dependency declares a
readiness policy:

- required before entity construction;
- required before scene activation;
- optional with a fallback;
- streamable after activation;
- editor-only or excluded from a server build.

Collision required by gameplay can block activation. High-resolution texture
mips do not. This distinction allows correct startup behavior without giving up
progressive loading and large-world streaming.

### 5.4 Streaming and residency

Loading makes an asset usable. Streaming changes its resident detail while it
remains usable.

`AssetManager` owns storage access and CPU residency. Runtime systems own their
specialized residency. For example, `RenderSystem` owns GPU resources and asks
for desired texture mips or mesh LODs. `AudioSystem` owns decode buffers and
requests compressed blocks with playback deadlines.

Streamable assets provide minimum-resident data:

- a texture provides small mip levels;
- a mesh provides bounds, material slots, and a fallback LOD;
- a scene provides settings and region metadata;
- a sound provides metadata and enough data to begin playback.

Residency policies use budgets and hysteresis. Advanced methods such as virtual
texturing or mesh-cluster streaming are introduced only when measurements and
content requirements justify them.

## 6. Scenes and world construction

### 6.1 A scene is an asset

A `SceneAsset` is an immutable composition asset. It describes entities and the
relationships between them. It is loaded through `AssetManager` like every
other asset type.

A scene is not the live world. Loading a `SceneAsset` reads and validates data.
Instantiating it creates live entities in `GameWorld`.

```text
SceneAsset                              GameWorld
----------                              ---------
serialized entity declarations   ->    live concrete entities
serialized identities            ->    runtime EntityId values
asset references                  ->    retained AssetRef values
entity references                 ->    resolved live references
output connections                ->    active event routes
```

This distinction allows one scene asset to be inspected, cached, instantiated,
or unloaded without confusing serialized data with live game state.

### 6.2 Scene contents

A scene contains:

- scene settings;
- a flat list of entity declarations;
- classnames and versioned property data;
- stable serialized entity identities where needed;
- explicit entity references;
- input/output connections;
- references to other assets;
- optional spatial-region metadata.

Large payloads remain in their own assets. Scenes reference meshes, materials,
textures, collision data, animation, and audio instead of embedding them.

CEngine does not require a general runtime prefab system. Reuse comes from
entity classes and referenced assets. Authoring tools may duplicate or expand
reusable assemblies into ordinary scene entities during export.

### 6.3 Transform relationships

Authoring hierarchy is not automatically runtime hierarchy. Organizational
parenting in a content tool can be baked during export. A relationship that has
game meaning is serialized explicitly, such as an owner, movement parent,
attachment, follow target, or door target.

Runtime transform relationships are typed, cycle-validated, and define their
behavior when either entity is destroyed. This preserves necessary attachments
without importing arbitrary authoring hierarchy into the live world.

### 6.4 Transactional instantiation

`GameWorld` instantiates a scene through a staging transaction:

```text
AssetManager loads and validates SceneAsset
                     |
                     v
GameWorld begins staged scene instance
  1. construct entities through EntityFactory
  2. deserialize and validate typed properties
  3. remap serialized entity identities to EntityId values
  4. resolve required asset and entity references
  5. create required runtime-system bindings
  6. run activation validation
  7. publish the complete scene instance
```

Before publication, staged entities are not observable from the active world.
If any required operation fails, the transaction removes created system objects,
destroys staged entities, and releases acquired assets. Existing world state is
unchanged.

The result is a `SceneInstanceId`. A world can own multiple additive scene
instances. Removing an instance destroys its non-persistent entities through
the normal deferred-destruction path.

### 6.5 Large worlds

Large-world policy belongs to a world-partition subsystem, not to the asset
loader. It determines which spatial regions should be resident, requests their
scene-region assets, and asks `GameWorld` to instantiate or remove them.

Persistent entities can transfer out of a region instance before it unloads.
Cross-region references use explicit policies for unresolved, temporarily
unloaded, and persistent targets.

## 7. Runtime-system integration

### 7.1 Boundary rules

Runtime systems own specialized objects. Entities own game meaning and may keep
opaque handles used to address those objects.

For example, a dynamic-prop entity can retain a `RenderObjectHandle` and a
`PhysicsBodyHandle`. The rendering and physics systems own the records behind
those handles. No entity owns a backend pointer.

There is no universal `EntityRuntimeLinks` record. Each entity class retains the
handles required by its behavior, and each system can store the owning
`EntityId` in its front-end record when it must return events to the world.

All public system APIs follow these rules:

1. Use one public facade for each top-level system.
2. Keep backend-specific types behind that facade.
3. Pass typed descriptions, commands, snapshots, and events.
4. Use opaque generation handles for runtime objects.
5. Provide symmetrical create and destroy operations.
6. Document calling threads and input lifetimes.
7. Do not let one system modify another system's private state.
8. Keep serialized records separate from runtime objects.
9. Do not expose OpenGL, Vulkan, Jolt, GLFW, or audio-library types in engine
   and game APIs.

### 7.2 State authority

`GameWorld` owns game-semantic transforms. A runtime source can temporarily
drive the produced transform according to an explicit policy:

- world or entity logic;
- physics simulation;
- animation root motion;
- network authority;
- editor manipulation.

The binding defines synchronization direction, update phase, and interpolation.
This avoids conflicting writes and supports transitions such as an animated
character entering ragdoll simulation.

Rendering and audio are never authoritative for game state.

## 8. Runtime systems

### 8.1 InputSystem

`InputSystem` converts platform events and device state into named game actions.
It owns keyboard, mouse, gamepad, bindings, action maps, and focus routing.

It publishes an immutable action snapshot. Game code reads actions such as
`move`, `look`, `jump`, and `interact` instead of GLFW key numbers. User-interface
focus can consume actions before gameplay receives them.

### 8.2 PhysicsSystem

`PhysicsSystem` owns shapes, rigid bodies, constraints, triggers, character
controllers, spatial queries, and collision-event production. A backend adapter
contains all Jolt-specific types.

```text
CreateBody(desc) -> PhysicsBodyHandle
UpdateBody(handle, command)
DestroyBody(handle)
RayCast(query) -> result
ShapeCast(query) -> result
DrainEvents() -> collision and trigger events
Step(fixed_delta_time)
```

Static map surfaces use triangle-mesh or authored compound collision assets.
Movable bodies use convex shapes. Character movement supports slopes, steps,
ground detection, and moving platforms.

Physics worker callbacks write to a system event queue. They never execute game
code. After a fixed step, `GameWorld` receives events containing safe entity
identities and dispatches them to entity behavior.

### 8.3 AnimationSystem

`AnimationSystem` owns animation instances and performs clip sampling, state
transitions, blending, inverse kinematics, pose calculation, root-motion
production, and animation-event production.

`GameWorld` or entity behavior controls semantic animation state. The animation
system calculates poses. The renderer consumes skinning palettes but does not
select clips or own animation state.

Animation can begin as a runtime subsystem with a simple main-thread schedule.
Its public ownership boundary remains stable if pose evaluation later moves to
jobs or a dedicated update phase.

### 8.4 AudioSystem

`AudioSystem` owns the device, mixer, buses, voices, listener state, decode
buffers, and streams. Entities issue typed play, stop, and parameter commands.
The world supplies listener and emitter transforms.

The game thread never blocks the audio callback. Commands cross through a queue.
Short effects use decoded buffers; music and long ambience stream. Common buses
include master, music, effects, dialog, and user interface.

### 8.5 RenderSystem

`RenderSystem` receives prepared world data and produces pixels. It owns render
objects, GPU resources, cameras, visibility, lighting, shadows, particles,
decals, fog, sky, post-processing, debug drawing, and final 2D composition.

Its graphics-neutral front end can expose:

```text
CreateRenderObject(desc) -> RenderObjectHandle
UpdateRenderObject(handle, update)
DestroyRenderObject(handle)
SubmitView(view)
RenderFrame()
```

Persistent handles represent retained render-scene objects. Per-frame view data
describes cameras, visibility inputs, interpolated transforms, and transient
draws. The renderer does not search `GameWorld` or call entity hooks.

OpenGL and Vulkan backends implement GPU execution without changing engine or
game-facing APIs. `RenderSystem` owns GPU residency and tracks the asset revision
used to create each GPU resource.

### 8.6 NetworkSystem

Networking is optional. When present, `NetworkSystem` owns transport,
serialization, replication, prediction, and reconciliation support. Game code
defines authority and replication policy.

The engine does not assume that physics simulation is deterministic across
machines. Transform authority and correction behavior must be explicit for each
replicated entity type.

## 9. Frame and fixed-tick flow

The application owns the outer schedule. `GameWorld` coordinates game-domain
work with the runtime systems.

An initial frame flow is:

1. Platform services pump window and device events.
2. `InputSystem` publishes the current action snapshot.
3. `GameWorld` processes inputs, due entity thoughts, timers, and network events.
4. Entity behavior emits physics, animation, audio, and rendering commands.
5. `AnimationSystem` evaluates state required for root motion.
6. `PhysicsSystem` runs zero or more fixed ticks.
7. `GameWorld` receives physics results and dispatches entity events.
8. `GameWorld` applies deferred spawning, destruction, and relationship changes.
9. `AnimationSystem` produces final poses and animation events.
10. The runtime extracts audio and render data from the completed world state.
11. `AudioSystem` accepts commands and spatial state.
12. `RenderSystem` renders the frame.

Networking phases depend on the selected authority and prediction model. They
must be specified with the game networking design rather than inserted at an
arbitrary point.

## 10. Events and cross-system effects

Unrelated systems communicate through typed commands, snapshots, and events.
They do not call each other's backends or mutate each other's storage.

For example:

```text
AnimationSystem produces FootstepEvent
                |
                v
GameWorld dispatches event to the character entity
                |
                +-- entity issues audio command
                `-- entity issues visual-effect command
```

This keeps the game decision in game code. Animation does not decide which
sound or particle effect defines a specific game character.

## 11. Identity, references, and save data

Runtime objects use generation handles. A handle contains a slot index and a
generation. Destroying an object advances the generation so stale access fails
safely.

Serialized entities use stable identity when they must survive scene reload,
save/restore, or external references. A scene-load transaction remaps serialized
identity to runtime `EntityId` values.

Save data contains game properties, stable asset identity, and stable entity
identity. It never contains raw pointers, runtime handles, GPU resources, or
backend identifiers. Save formats are versioned and validated into temporary
state before modifying an active world.

## 12. Threading and lifetime

`GameWorld` begins as main-thread-owned state. Jobs are introduced only where
data ownership and synchronization are explicit.

- Asset workers read, decode, and validate unpublished payloads. Publication is
  atomic after successful validation.
- Physics workers operate inside `PhysicsSystem::Step`. Game code does not run
  on those workers.
- The render thread owns GPU creation, use, and deletion.
- The audio thread owns the device callback and consumes lock-free or bounded
  command data.
- Structural world changes occur at defined safe points.

Shutdown reverses dependency order:

1. stop game behavior and new requests;
2. deactivate scene instances;
3. remove audio, animation, physics, and render bindings;
4. destroy live entities and the world;
5. release asset references;
6. stop systems and worker services;
7. destroy platform windows and devices.

## 13. Build and dependency architecture

The build should make the extension direction visible:

```text
Foundation
  IDs, containers, math, jobs, diagnostics, platform abstractions
       |
       +-------------------+
       v                   v
Assets                 Runtime system APIs
asset identity         render, physics, animation,
loading, formats       audio, input, network
       |                   |
       +---------+---------+
                 v
              GameWorld
      entities, factory, scenes,
       lifecycle, game-facing API
                 |
        +--------+--------+
        v                 v
Engine entity library   Game module
standard entities       custom entities and rules
        +--------+--------+
                 v
          Game application
```

Backend implementations depend on their public system APIs, not on game code.
The game module depends on CEngine. CEngine never depends on a specific game.

Scene asset deserialization is registered with `AssetManager`. Scene
instantiation belongs to `GameWorld`. This prevents a separate scene-loading
layer from owning parts of both domains.

## 14. Architecture constraints

These rules are normative:

- `GameWorld` is the root of one live game simulation.
- Entities are the primary scene-authored game extension mechanism.
- CEngine and game modules register entity classes through the same factory.
- A scene is an asset; a scene instance is live world state.
- `AssetManager` loads scene data; `GameWorld` instantiates it.
- CEngine does not require a general runtime prefab system.
- Runtime systems own their specialized objects and backend resources.
- Entities and game code use backend-neutral APIs and opaque handles.
- Disk formats contain no runtime pointers or backend objects.
- Scene instantiation is transactional.
- Worker callbacks never execute arbitrary game behavior.
- Large payloads live in dedicated assets and are referenced by scenes.
- Systems perform bulk work through narrow APIs.
- Advanced scheduling and streaming techniques require measured justification.

## 15. Delivery sequence

Build the architecture in dependency order:

1. Establish foundation types, generation handles, diagnostics, and platform
   abstractions.
2. Implement the typed `AssetManager`, target-format validation, dependencies,
   and immutable publication.
3. Define the entity-class descriptor, property schema, and registration model
   for both engine and game modules.
4. Implement `GameWorld` identity, lifecycle, lookup, entity I/O, timers, and
   deferred structural changes.
5. Load `SceneAsset` through `AssetManager` and implement transactional scene
   instantiation in `GameWorld`.
6. Stabilize generation-handle APIs and entity bindings for rendering and
   physics.
7. Add map collision, queries, triggers, and a character controller.
8. Add named input actions and focus routing.
9. Add animation playback, blending, events, and GPU skinning.
10. Add audio commands, mixing, spatial state, and streaming.
11. Add save/restore and game-rule integration.
12. Add world partition, asset-detail streaming, navigation, and networking
    when game requirements justify them.

Each stage must leave a coherent usable engine. Later stages extend stable
ownership boundaries instead of replacing the entity and world model.
