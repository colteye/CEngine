# CEngine Target Architecture: Systems

> **Status: consolidated reference.** Complete long-term asset, scene, input, physics, rendering, animation, and audio contracts.
> `STATUS.md` and `IMPLEMENTATION.md` determine what is active; this file
> preserves detailed target information without expanding current scope.

## Contents

- [Assets and Scenes](SYSTEMS.md#assets-and-scenes)
- [Input](SYSTEMS.md#input)
- [Physics](SYSTEMS.md#physics)
- [Rendering](SYSTEMS.md#rendering)
- [Animation](SYSTEMS.md#animation)
- [Audio](SYSTEMS.md#audio)

## Assets and Scenes

> **Status: detailed target contract.** `IMPLEMENTATION.md` defines the simpler
> active loading, identity, and startup behavior. Advanced residency and live
> transaction behavior is promoted only when required.

### 1. Purpose

This document defines cooked asset identity, synchronous loading, complete
residency, scene data, and fail-before-publication scene instantiation.

### 2. Core distinction

An asset is immutable published content. A runtime object is live mutable state.

```text
AssetService                         Runtime owners
------------                         --------------
SceneAsset        --instantiate-->   simulation entities
MeshAsset         --upload------->   RenderSystem GPU mesh
CollisionAsset    --create------->   PhysicsSystem shape
AudioAsset        --decode------->   AudioSystem buffers
AnimationAsset    --evaluate----->   AnimationSystem instance
```

The published asset can be shared by many runtime objects. Runtime objects never
become asset payloads.

### 3. AssetService responsibility

`AssetService` owns:

- stable asset identity and type;
- canonical project paths used for diagnostics and tools;
- cooked target-format readers;
- validation and immutable publication;
- dependency records and readiness;
- reference lifetimes;
- CPU-memory residency and budgets.

It does not own:

- GPU allocations;
- physics bodies or mutable backend shapes;
- audio voices or device buffers;
- live entities or scene instances;
- game-specific activation policy.

### 4. Identity and references

`AssetId` is stable across cooked builds when the authored asset retains its
identity. A path is useful for authoring and diagnostics but is not the only
identity mechanism.

An asset reference contains the information required to detect mismatches:

```cpp
namespace CEngine {

struct AssetReference {
    AssetId id{};
    AssetTypeId type{};
    ContentRevision revision{};
};

} // namespace CEngine
```

`AssetRef<T>` is a typed lifetime reference to an immutable published payload.
It pins exactly the referenced revision and does not expose manual
reference-count calls to ordinary game code.

```text
Acquire<T>(AssetReference) -> Result<AssetRef<T>>
```

A type or revision mismatch fails before publication to the caller. Cooked
runtime references are exact: the content build resolves “latest” authoring
choices into concrete revisions. Development tools may ask the manifest for the
latest revision before acquiring; live game code does not hide that lookup in
`AssetRef`.

### 5. Asset records and states

An internal asset record tracks identity independently from any one payload
revision.

```text
Unloaded
  -> Loading
  -> Ready
  -> Unloading

Loading -> Failed
```

Only `Ready` exposes a published payload. Loading is synchronous during startup
or scene staging, where blocking is explicit and no simulation tick is
running. Requests for the same exact asset reuse the published payload.

### 6. Target assets

Source files are inputs to tools. Runtime code consumes cooked target assets.

| Asset | Content |
| --- | --- |
| `SceneAsset` | settings, packed typed entity declarations, and dependencies |
| `ModelAsset` | reusable mesh LOD, material-slot, skeleton, and optional collision references for one model |
| `MeshAsset` | geometry, bounds, submeshes, and LODs |
| `MaterialAsset` | parameters, pipeline class, and texture references |
| `TextureAsset` | metadata, format, pixels, and mips |
| `CollisionAsset` | triangle, convex, or compound collision data |
| `SkeletonAsset` | hierarchy, reference pose, and bind data |
| `AnimationAsset` | clips, curves, and cosmetic events |
| `AudioAsset` | encoded samples, loop points, and stream metadata |
| `ShaderAsset` | cooked programs, variants, and reflection data |

`ModelAsset` is reusable content, not a nested set of live entities. CEngine
does not require a runtime prefab type.

#### 6.1 Content build

Runtime asset formats are a product of one deterministic content build:

```text
source files + project settings + registered game schemas
  -> import into normalized tool data
  -> validate references, units, coordinates, and entity properties
  -> partition and derive target data
  -> write versioned cooked assets
  -> write content manifest and revision
```

Importers are tool-only adapters. Runtime systems never depend on FBX, glTF,
image-editor, or DCC SDK object models. After import, all formats use the same
normalized path.

The content manifest maps stable `AssetId` values to cooked storage records and
contains type, revision, dependencies, byte ranges, and side/quality sections.
A dedicated-server build strips client-only payload sections while preserving
identities and shared revision compatibility. A client never infers server
compatibility from filenames or timestamps.

Games extend content only by registering entity/property schemas and, when a
new runtime asset type is genuinely required, one importer/cooker/reader
contract. A new source-file extension by itself does not justify a new runtime
asset abstraction.

### 7. Dependency graph

Each cooked asset declares dependencies by typed identity. The cooker validates
the graph and reports missing assets and illegal cycles.

Dependencies carry readiness policy:

- required to decode;
- required before runtime construction;
- required before activation;
- optional with a fallback;
- client-only, server-only, or shared.

For example, collision required for authoritative gameplay blocks server scene
activation. Published payloads retain the dependencies required by their
runtime contract.

### 8. Loading and publication

A load performs:

1. resolve asset identity to cooked storage;
2. read the header and dependency table;
3. validate version, type, sizes, ranges, and checksums;
4. load required dependencies;
5. decode or map the payload;
6. perform type-specific semantic validation;
7. atomically publish the immutable payload;
8. return the typed immutable reference.

Every binary reader uses overflow-safe range checks before exposing views.
Serialized pointers and backend handles are forbidden.

Readers prefer one validated mapping or contiguous allocation plus internal
offsets/spans. They do not reconstruct an object graph with one allocation per
node, material slot, mesh section, or entity property. Endianness, alignment,
compression, and maximum decoded sizes are part of the cooked-format contract.

### 9. Residency

The service loads a complete asset revision and retains it while referenced. There
is one residency state for the complete payload. Scene staging checks the
declared memory budget before publication and fails cleanly if the complete
working set does not fit.

### 10. SceneAsset

A `SceneAsset` is immutable serialized composition. It contains:

- scene settings and content revision;
- a flat deterministic list of entity declarations;
- canonical classnames and class schema versions;
- stable serialized identities where required;
- typed property data;
- explicit entity references;
- entity input/output connections;
- asset dependencies with side/readiness policy;
- packed typed class records, including `prop` records with a static/dynamic flag.

Large geometry, materials, textures, collision, animation, and audio remain in
dedicated assets and are referenced by identity.

#### 10.1 Entity property representation

Cooked scene properties must allow game-defined entity classes without requiring
the engine scene reader to know every class.

A practical model is a schema-bound property block:

```text
entity declaration
  classname
  class schema version
  stable identity
  common fields
  typed property IDs and values
  output connections
```

The cooker validates names and converts them to stable property IDs. The runtime
class descriptor validates the version and copies values into fixed C++ fields.
Runtime entity behavior does not repeatedly query a generic property map.

#### 10.2 Prop representation and system batching

Authored mesh objects are `prop` entities. Static and dynamic props share one
concrete class and one serialized record; one flag selects the behavior rather
than the entity type.

```text
SceneAsset
  settings, revision, dependencies, entity declarations
  packed prop class records
    mesh and material references
    transform and static/dynamic flag
    visibility and collision settings
    optional baked-lighting binding

Prop entity -> MeshAsset + material bindings + transform + flags
```

The cooker groups prop records by class in deterministic packed arrays. A live
`SceneInstance` creates an entity and `EntityId` for every prop. Rendering and
physics may group static prop bindings by coarse bounds and compatible storage
behind handles such as `RenderSceneChunkHandle` or `PhysicsStaticChunkHandle`.
That batching is private to the owning system and never replaces entity
identity, authored fields, or lifecycle.

Bulk batch handles and entity-created system handles are generation checked.
Scene removal makes simulation records unreachable at a safe point, submits
destruction to each owning system, and retains asset revisions until backend
frames or physics work retire.

This split is the default scene model:

| Content need | Representation |
| --- | --- |
| static surface | `prop` entity with `dynamic == false`; system may batch its bindings |
| moving object | `prop` entity with `dynamic == true` |
| baked or static light/probe | entity record; renderer may batch its immutable data |
| mutable gameplay light | entity with a client render-light binding |
| large terrain | ordinary chunked mesh assets |

The renderer and physics system consume chunk records in bulk. They never
require one command or allocation per triangle or baked light.

#### 10.3 Determinism

Cooking the same normalized source with the same toolchain, target, and settings
produces byte-identical manifests, schemas, scene declarations, IDs, and
gameplay data. Writers use deterministic order, zero padding, normalized
strings, explicit endianness, and versioned layouts. Platform-specific GPU/audio
encodings may differ across targets; their target key and resulting revision are
part of identity and never participate in protocol compatibility by accident.

### 11. Side-specific scene instantiation

Server and client load the same compatible scene revision but can instantiate
different declarations or properties.

```text
Scene entity domain
  server-only
  client-only
  independent-both
  networked
```

Creation source is fixed by domain; it is not decided ad hoc during loading:

| Domain | Server creation | Client creation |
| --- | --- | --- |
| server-only | server scene | none |
| client-only | none | client scene |
| independent-both | server scene | client scene; no network identity |
| networked | server scene or runtime spawn | network create message only |

For a placed networked entity, the server create record can carry its stable
serialized scene identity for diagnostics and local-content association. The
client never also constructs it directly from the scene, avoiding duplicate
representations and timing races.

Static shared presentation is created locally and is not replicated
object-by-object.

### 12. Transactional scene instantiation

The owning simulation owns the transaction:

1. retain the ready `SceneAsset`;
2. select declarations appropriate to the simulation side;
3. validate class registrations and schema versions;
4. construct staged entities through `EntityFactory`;
5. assign runtime identities and `SceneInstanceId` ownership;
6. deserialize typed fields;
7. remap serialized entity references;
8. acquire activation-critical assets;
9. prepare required bulk chunk bindings in physics, rendering, and lighting;
10. run entity spawn, which prepares class-specific system bindings;
11. validate all prepared bindings and activation conditions;
12. commit prepared bindings with non-failing safe-point operations;
13. publish the complete `SceneInstance`;
14. call non-failing activation notifications.

Failure before publication destroys bindings and staged entities and releases
references. Existing public simulation state is unchanged.

Validation is the last fallible step. Commit, simulation publication, and activation
notification do not allocate and cannot report recoverable failure; violated
invariants terminate the transaction host rather than attempting partial
rollback after visibility changed.

### 13. Coordinates

Game and serialized spatial positions use an ordinary 32-bit floating-point
`Vec3`. The project convention is right-handed, `+X` forward, `+Y` left, and
`+Z` up. Distance is meters, angles are radians, and time is seconds. Backend
clip-space, handedness, and unit conversions remain private to the owning
system.

The convention is included in cooked-project and protocol compatibility. Scene
coordinates must remain within the precision and magnitude limits declared by
the product profile.

### 14. Threading

- `AssetService` runs on the Game Thread during staging.
- Storage and decode use staging-owned temporary memory.
- Publication exposes an immutable payload only after validation succeeds.
- Backend resource creation occurs on the owning system thread.
- Scene entity construction and publication occur on the simulation thread.

### 15. Failure behavior

- Invalid binary ranges, versions, types, and checksums fail the asset request.
- Missing optional content uses an explicitly registered typed fallback.
- Missing authoritative content prevents server activation.
- Diagnostics include asset identity, path when available, type, revision,
  dependency chain, and load phase.

### 16. Invariants

- A scene is an asset; a scene instance is live simulation state.
- Asset publication is immutable and atomic.
- `AssetRef<T>` always pins one exact payload revision.
- Runtime systems own resources derived from assets.
- The simulation owns entity instantiation and scene lifetime.
- The engine scene reader does not hard-code game entity classes.
- Dependencies declare side and readiness policy.
- Static shared content is identified by validated content revision.
- No cooked asset contains a live pointer or backend object.
- Props are entities. Systems may batch immutable static-prop data without
  changing that public representation.
- A networked entity has exactly one client creation source: the network stream.
- Scene activation is transactional across the simulation and its required
  system bindings.

---

## Input

> **Status: detailed target contract.** The action-to-`UserCommand` boundary is
> active first; optional device and context behavior follows milestone needs.

### 1. Purpose

This document defines `InputSystem`, device processing, bindings, action state,
focus, user-command construction, threading, and the client/server boundary.

### 2. Responsibility

`InputSystem` owns:

- keyboard, mouse, gamepad, and supported device adapters;
- raw device state and event normalization;
- control bindings and action maps;
- dead zones, curves, sensitivity, and chords;
- focus and input-context routing;
- one immutable action snapshot for each client frame;
- local rebinding state.

It does not own:

- authoritative player movement;
- network transport;
- gameplay consequences;
- UI model state;
- platform event pumping outside its platform adapter.

### 3. Platform boundary

Platform services pump OS/window events. Input adapters convert backend values
such as GLFW keys or platform gamepad codes into CEngine device controls.

Game and entity code never depends on backend key constants.

```text
platform event/device state
  -> normalized controls
  -> bindings and contexts
  -> named actions
  -> immutable ActionSnapshot
```

### 4. Actions and contexts

An action is a semantic local input such as:

- `move`;
- `look`;
- `jump`;
- `interact`;
- `fire_primary`;
- `menu_back`.

Action values can be digital, scalar, or vector. The action snapshot records
current value plus transitions needed by the frame, such as pressed and released.

Action and context names resolve to stable IDs when binding data loads.
Per-frame evaluation uses packed control/action arrays and a frame arena; it
does not hash strings or allocate events for every held control.

Contexts control routing and priority:

```text
console
menu
gameplay
spectator
vehicle
```

A higher-priority context can consume an action before lower-priority contexts
observe it. Text input remains distinct from physical key actions.

### 5. User commands

`GameClient` converts relevant gameplay actions into a tick-addressed
`UserCommand` defined by `GameShared`.

```text
ActionSnapshot                 UserCommand
--------------                 -----------
local UI and gameplay state -> compact networked simulation intent
frame cadence                  fixed simulation tick
rich local controls            validated bounded fields
```

InputSystem does not serialize network packets and does not decide authority.
The game client chooses which actions enter the command.

For high frame rates or differing tick rates, command construction integrates
or samples input according to a documented policy so presses are not lost and
look/movement values are not applied twice.

### 6. Prediction and replay

The client retains produced `UserCommand` records until the server acknowledges
them. Reconciliation replays commands, not raw OS events.

InputSystem does not rewind devices. Command history belongs to client
prediction/replication.

### 7. Threading

- Platform event pumping occurs on the platform-required thread.
- Device state is normalized before action snapshot publication.
- Published snapshots are immutable for the client frame.
- Rebinding applies at a frame boundary.
- Device callbacks do not invoke arbitrary entity behavior.
- Server simulation never reads OS input devices.

### 8. Failure behavior

- Device disconnect clears or neutralizes affected actions deterministically.
- Focus loss prevents stuck pressed state and follows configured pause policy.
- Invalid binding data falls back to known-safe defaults and reports diagnostics.
- Unsupported devices do not prevent other input devices from operating.
- Input event overflow is visible and resets unsafe transient state.

### 9. Invariants

- InputSystem produces local semantic actions, not gameplay outcomes.
- Game code does not depend on platform key codes.
- User commands are constructed by GameClient from immutable action snapshots.
- Server code never trusts or accesses client devices.
- Reconciliation replays commands, not raw input events.
- Focus and consumption policy are explicit.

---

## Physics

> **Status: detailed target contract.** M0 uses the smallest Jolt-backed subset
> named by `IMPLEMENTATION.md`; networking and advanced movement policies are
> promoted as required.

### 1. Purpose

This document defines `PhysicsSystem`, its authoritative server role, optional
client prediction role, body and shape ownership, queries, events, character
movement, transform authority, threading, and backend boundary.

### 2. Responsibility

`PhysicsSystem` owns:

- physics scenes;
- collision shapes and runtime shape cache;
- static, dynamic, and kinematic bodies;
- constraints;
- triggers and sensors;
- ray, shape, overlap, and sweep queries;
- character movement implementation;
- contact and trigger-event collection;
- fixed-step integration;
- backend workers and physics-memory policy.

It does not own:

- entity lifetime or game rules;
- authoritative damage and interaction outcomes;
- mesh or collision asset I/O;
- rendering transforms;
- network replication policy.

### 3. Server and client physics

The authoritative `PhysicsSystem` belongs to the server runtime. It produces
physical outcomes consumed by `ServerSimulation`.

The client can host a separate, limited physics scene for:

- local-player prediction;
- client-only cosmetic simulation;
- presentation traces that cannot affect authoritative results.

Server and client physics bodies are separate objects even in a listen-server
process.

### 4. Public API

The game-facing API uses engine-neutral descriptions and generation handles:

```text
CreateBody(PhysicsBodyDesc) -> PhysicsBodyHandle
UpdateBody(handle, PhysicsBodyCommand)
DestroyBody(handle)

CreateStaticChunk(PhysicsStaticChunkDesc) -> PhysicsStaticChunkHandle
DestroyStaticChunk(handle)

CreateConstraint(desc) -> ConstraintHandle
DestroyConstraint(handle)

RayCast(query) -> hit results
ShapeCast(query) -> hit results
Overlap(query) -> overlap results

Step(fixed_delta_time)
DrainEvents() -> contact and trigger events
```

Descriptions use CEngine math, collision-layer IDs, material IDs, asset
references, and safe owner tokens. Jolt types remain private to its backend.
Batch variants accept caller-owned spans. Static prop collision bindings may
enter physics in bulk rather than through one allocating call per triangle or
mesh instance; each binding still has a safe owner `EntityId`.

### 5. Body and shape ownership

`PhysicsBodyHandle` identifies a PhysicsSystem front-end record. That record can
store the owning server `EntityId` used when producing events.

Immutable collision geometry originates in `CollisionAsset`. PhysicsSystem
creates or caches backend shapes derived from the asset revision. Bodies retain
shape references according to backend lifetime requirements.

Recommended shape policy:

- static world surfaces: triangle mesh or authored compounds;
- movable rigid bodies: convex or compound convex shapes;
- characters: dedicated character shape/controller policy;
- triggers: simple authored sensor shapes;
- complex dynamic triangle meshes: unsupported.

### 6. Fixed-step simulation

The server advances physics at a fixed simulation interval. The server clock
accumulates real time and executes a bounded number of fixed ticks.

Each tick:

1. apply queued body and constraint commands;
2. apply kinematic targets and character inputs;
3. step the backend;
4. collect body transforms and velocities;
5. collect and normalize contact/trigger events;
6. return results to `ServerSimulation`;
7. dispatch game behavior after the backend step has ended.

Commands do not mutate the physics scene while the backend is actively stepping.
Command, active-body, contact, and query-result storage is retained or allocated
from tick arenas with configured bounds. Exceeding contact or query capacity
uses the caller's declared truncation/failure policy; it never grows the heap in
an unbounded callback path.

### 7. Transform authority

Each bound entity declares a transform policy:

| Mode | Producer | Consumer |
| --- | --- | --- |
| Static | scene/entity at creation | physics and rendering |
| Kinematic | entity/shared movement target | physics collision resolution |
| Dynamic | server physics | entity authoritative transform |
| Predicted character | shared client/server movement | reconciliation and presentation |
| Ragdoll | server physics with animation transition | entity and client replication |

Authority transitions are explicit state changes. Entering ragdoll, teleporting,
or returning to animation resets the relevant histories and velocities according
to policy.

Rendering uses interpolated client presentation state, not raw mutable backend
body pointers.

### 8. Character movement

Character movement is a game-facing physics subsystem with a shared deterministic
contract where client prediction is required.

It supports:

- slopes and step handling;
- ground detection;
- jump and air movement policy;
- moving platforms;
- crouch/shape transitions;
- penetration recovery;
- bounded velocity and acceleration;
- server validation of user commands.

The shared movement layer owns game movement rules. PhysicsSystem provides
collision queries and backend integration. Server results remain authoritative.

For prediction, the contract defines the complete replay state: position,
velocity, ground/support identity, movement mode, stance, timers, and any
game-specific movement parameters. It also defines which collision inputs are
reproducible on the client. Interactions with unpredicted dynamic bodies are
treated as corrections or explicit predicted dependencies, never hidden backend
state.

The character path is one engine-owned controller/query algorithm used
on both sides. A game can parameterize movement behavior; it does not replace
the controller with unrelated client and server implementations and still call
the result predicted.

### 9. Queries

Queries specify:

- physics-scene context;
- shape or ray;
- collision-layer mask;
- ignored owner/entity IDs;
- required result fields;
- closest, any, or all-hit policy.

Queries run synchronously on the Game Thread outside the backend step. Batch
variants accept input and output spans so one facade call performs many queries.

Hit results expose engine identifiers, positions, normals, fractions, and
material classifications—not backend body pointers.

All-hit and overlap queries require caller-provided capacity. Results have a
documented deterministic sort key when gameplay can observe ordering. `Any` and
`closest` queries avoid materializing unused hits.

### 10. Contacts and triggers

Backend worker callbacks write minimal normalized records to a bounded event
collector. They never call entity or game code.

After the step:

```text
backend contacts
  -> PhysicsSystem event normalization
  -> owner EntityId mapping
  -> ServerSimulation event queue
  -> entity/game-rule dispatch
```

Game code decides damage, sounds, effects, pickups, and trigger meaning. Physics
reports physical observations.

Contact delivery policy defines begin, persist, and end events; deduplication;
ordering; destroyed-body behavior; and overflow diagnostics.

### 11. Networking

The server replicates game-meaningful body state through game replication.
PhysicsSystem does not serialize backend bodies.

Client prediction uses shared movement state and an optional prediction physics
scene. Remote bodies are normally interpolated from server snapshots rather than
fully simulated as authority on the client.

Lag-compensated queries use a game-server history facility with physics query
support. They never permanently rewind the live authoritative simulation.

### 12. Backend boundary

The boundary is a source and ownership boundary, not an `IPhysicsBackend`
hierarchy. Only `PhysicsSystem` implementation files include Jolt headers or
identifiers. `PhysicsSystem` calls Jolt directly behind the public facade.

The facade owns:

- handle allocation and generation validation;
- engine collision layers and filters;
- command/event normalization;
- asset revision mapping;
- fixed-step policy;
- owner identity mapping.

### 13. Threading

- The simulation thread queues commands outside a backend step.
- Backend jobs operate only on physics-owned state.
- Entity storage is not accessed from backend workers.
- Event records cross to the simulation thread after stepping.
- Shape creation/destruction follows backend thread and step restrictions.
- Shutdown waits for backend jobs before invalidating facade records.

### 14. Failure behavior

- Invalid body descriptions fail without publishing a handle.
- Missing required collision assets fail server scene activation.
- Stale body handles are rejected.
- Event queue overflow produces a visible diagnostic and defined degradation;
  it is never silent memory corruption.
- Backend assertion or initialization failures include body/entity context when
  available.
- Failed optional client cosmetic physics does not affect server authority.

### 15. Invariants

- Server physics is authoritative for declared dynamic physical state.
- Physics workers never execute arbitrary game behavior.
- Game code never receives a backend body pointer.
- Body create and destroy operations are symmetrical.
- Physics mutation does not occur during an active backend step.
- Collision assets and derived backend shapes have explicit revision lifetimes.
- Client physics cannot commit authoritative game outcomes.
- Static scene collision is created and removed in bulk chunks.
- Gameplay-observable query ordering never depends on backend worker order.

---

## Rendering

> **Status: detailed target contract.** M0 renders on the main thread with fixed
> passes. Render-thread and other advanced behavior becomes active only through
> `IMPLEMENTATION.md` and `DELIVERY.md#tradeoffs-and-promotion`.

### 1. Purpose

This document defines `RenderSystem`, its retained render scene, frame
submission, compiled backend boundary, threading, and failure behavior.

### 2. Responsibility

`RenderSystem` receives prepared client-simulation presentation data and produces
pixels. It owns:

- retained render objects and generation handles;
- GPU meshes, textures, material instances, and pipelines;
- camera and view state;
- visibility, culling, and draw preparation;
- direct and ambient lighting;
- shadows;
- skeletal skinning data consumption;
- sky, exposure, and tone mapping;
- debug drawing and final 2D composition;
- graphics-device lifetime;
- GPU-memory residency and budgets.

It does not own:

- authoritative entity state;
- game rules or damage logic;
- animation state-machine decisions;
- asset file I/O;
- client/server replication;
- gameplay transform authority.

### 3. Front-end model

CEngine uses a retained render scene plus per-frame view submission.

```text
Retained state                         Per-frame state
--------------                         ---------------
render objects                         camera/view
mesh/material bindings                 interpolated transforms
lights and persistent emitters         visible-layer policy
GPU resource records                   transient draws and debug data
```

Conceptual API:

```text
CreateRenderObject(desc) -> RenderObjectHandle
UpdateRenderObject(handle, update)
DestroyRenderObject(handle)

CreateLight(desc) -> RenderLightHandle
UpdateLight(handle, update)
DestroyLight(handle)

CreateSceneChunk(desc) -> RenderSceneChunkHandle
ReplaceSceneChunk(handle, desc)
DestroySceneChunk(handle)

Submit(RenderSubmission)
```

Handles identify front-end records, not API-specific resources.

The Game Thread facade reserves a handle generation and records the create in
the next submission. The Render Thread creates the matching proxy in submission
order. Destroy invalidates the producer handle immediately; its slot is not
reused until the Render Thread reports the destroy sequence retired. This is the
only producer-side render state and prevents entity pointers or locks from
crossing threads.

`RenderSceneChunkHandle` represents an optional renderer-owned batch of static
prop and baked-lighting records with coarse bounds. It is an implementation
detail derived from prop entities. Mutable props and lights use individual
handles because they need independent updates.

### 4. Client-simulation integration

Client entities create and destroy retained render objects through a
game-facing render interface. They can keep opaque handles.

The client runtime extracts a `RenderFrame` only after client snapshot
application, prediction reconciliation, interpolation, and final animation pose
evaluation are complete.

The renderer does not search `ClientSimulation`, dereference entities, or call
entity hooks. Submitted frame data remains valid for the documented consumption
period.

Extraction uses a double- or triple-buffered linear arena. Dirty retained
updates and transient draws are written as packed arrays, then published once.
There is no heap allocation or cross-thread call per visible object. Unchanged
retained objects produce no update record.

### 5. Assets and GPU resources

`AssetService` owns immutable CPU asset payloads. `RenderSystem` owns GPU
representations derived from them.

An internal GPU resource key includes asset identity, asset revision, and
render-backend/device context. Multiple render objects share compatible GPU
resources.

During scene staging, the renderer uploads the complete selected mesh LODs and
texture mips on the Render Thread. Compatible render objects share those GPU
resources. Destruction is deferred until the GPU retires the last referencing
frame. Staging fails when the declared GPU budget cannot hold the scene.

### 6. Render frame

A frame packet contains immutable presentation data such as:

- one view;
- camera matrices and exposure inputs;
- render-object transforms for the presentation time;
- skinning palette references;
- light and environment state;
- debug primitives;
- UI composition commands;
- frame and simulation timing metadata.

The frame packet contains no entity pointers. IDs used inside it address
renderer-owned records or packet-local arrays.

### 7. Visibility and scene organization

The retained render scene maintains spatial bounds and candidate structures.
Visibility can include:

- frustum culling;
- layer and view masks;
- distance and projected-size tests;
- occlusion methods when justified;
- LOD selection;
- instancing compatibility.

The visual spatial index does not own game entities. Authors do not manually
create renderer draw batches as a gameplay concept.

The cooker supplies conservative chunk and instance bounds. The renderer uses
chunk/instance frustum culling, instance LOD, material sorting, and instancing.
Visibility is conservative: uncertain content is drawn rather than incorrectly
omitted.

### 8. Lighting and image model

The renderer is physically based and linear-HDR. Materials use one
metallic/roughness model with opaque, alpha-tested, transparent, and emissive
variants. Color textures declare color space; lighting calculations occur in
linear space; exposure and tone mapping occur once near final composition.

Lighting data has two lifetimes:

- static scene chunks contain baked lightmaps and ambient probe data;
- retained dynamic lights contain directional, point, and spot parameters.

The baseline high-quality path supports:

1. visible opaque and alpha-tested geometry;
2. bounded shadows;
3. direct lights plus baked/ambient contribution;
4. basic transparency;
5. sky, exposure, tone mapping, debug drawing, and UI.

Shadow resolution, shadow updates, and light count are budgeted for the view.
Lights are prioritized by screen influence and authored importance. Exceeding a
visual budget reduces shadow updates or omits lower-priority lights.

### 9. Animation and skinning

`AnimationSystem` chooses clips and computes poses. It publishes skinning
palettes or palette upload data. `RenderSystem` consumes them.

The renderer uploads palettes and performs GPU skinning, but it does not control
animation transitions or gameplay events.

### 10. Debug drawing and UI

UI model and interaction policy belong to `GameClient`; `RenderSystem` owns
the final text, vector, image, and 2D draw composition. A menu can exist without
a live server simulation.

### 11. Backend boundary

The required boundary is a source and ownership boundary, not a mandatory
virtual interface. Backend-specific headers and objects exist only inside the
`RenderSystem` implementation. Engine/game-facing headers contain only CEngine
descriptions, handles, and frame packets.

The selected production backend is compiled directly into `RenderSystem`.
Per-draw virtual dispatch and a mirror interface for every backend object are
forbidden.

The backend owns:

- device and swapchain objects;
- command buffers and synchronization;
- API pipelines and descriptors;
- API resource creation and deletion;
- fixed render-pass execution;
- presentation.

The front end owns render meaning and lifetime policy. Changing the compiled
backend does not change entity or asset APIs.

The front end defines only the fixed semantic passes needed by the shipped
renderer.

### 12. Threading

- The Game Thread is the sole producer; the Render Thread is the sole consumer.
- `Submit` copies all changes and frame data into one of two preallocated render
  packets. The Render Thread may be at most one frame behind.
- Create/update/destroy operations are records in that submission; there are no
  cross-thread calls per object.
- The Render Thread owns the retained render records and graphics device.
- GPU uploads and deletion occur on the device-owning thread.
- Culling, draw preparation, and graphics submission run on the Render Thread.
- Deferred GPU destruction respects render sequence retirement.
- Waiting occurs only at bounded one-frame backpressure, loading, or shutdown;
  ordinary entity/render operations never flush the Render Thread.

### 13. Device loss and failure

- Missing optional presentation assets use typed fallbacks.
- A failed shader or pipeline build retains a prior valid revision when possible
  and emits diagnostics.
- Required device initialization failure prevents client presentation startup.
- Device loss terminates client presentation with a structured diagnostic.
- Render-object creation failure does not leave a published half-valid handle.
- Diagnostics include asset revision, backend, pass/pipeline, and view context.

### 14. Performance contract

The renderer uses retained records, CPU frustum culling, instancing, and
conventional indexed draws. Work and memory follow the active product profile.
Additional techniques use the evidence rules in
[Tradeoffs and promotion](DELIVERY.md#tradeoffs-and-promotion).

### 15. Invariants

- Rendering is never authoritative for game state.
- RenderSystem owns every GPU resource.
- Client entities use opaque render handles, never backend pointers.
- The renderer consumes prepared data and never searches a simulation.
- Asset revision is part of derived GPU-resource identity.
- Persistent render state and per-frame view data have distinct lifetimes.
- Changing the compiled backend does not change game entity code.
- Static props may enter the renderer through packed system-owned batches while
  remaining entities in the scene model.
- Unchanged retained objects require no per-frame update or allocation.
- Lighting is linear-HDR and physically based; quality tiers do not change game
  meaning.
- Render workload and memory growth are governed by explicit per-view and
  device budgets.

---

## Animation

> **Status: detailed target contract.** Implement only when the active milestone
> in `IMPLEMENTATION.md` promotes animation.

### 1. Purpose

This document defines client-side clip playback, cross-fade, pose evaluation,
cosmetic events, and skinning output.

### 2. Responsibility

`AnimationSystem` owns:

- animation instances and generation handles;
- clip cursors and cross-fade state;
- clip sampling and blending;
- local- and model-space pose calculation;
- animation-event extraction;
- skinning palette production;
- contiguous pose storage and animation-memory budgets.

It does not own:

- game-state decisions such as whether a player attacks;
- authoritative movement policy;
- entity lifetime;
- GPU rendering resources;
- asset file I/O;
- audio or effect selection.

### 3. Server and client roles

Animation runs only on the client Game Thread. Replicated and
predicted semantic state tells game code which clip to play or cross-fade; the
resulting pose is presentation only.

The server uses explicit gameplay timers, physics shapes, and attachment
transforms. Damage, movement, and action timing never depend on a visual clip or
bone.

### 4. Assets and instances

`SkeletonAsset` contains hierarchy, reference pose, and bind data.
`AnimationAsset` contains clips, curves, and cosmetic authored events.

`AnimationInstanceHandle` identifies mutable AnimationSystem state bound to a
compatible skeleton and configuration. Asset payloads remain immutable and can
be shared across instances.

Instances live in generation-checked pools. Clip cursors, cross-fade state, pose
buffers, events, and palettes use contiguous per-instance blocks or size-classed
pages reserved at instance construction. Clip IDs are resolved during creation;
evaluation does not perform string lookup or allocate one object per bone,
curve, or event.

Conceptual API:

```text
CreateAnimationInstance(desc) -> AnimationInstanceHandle
PlayClip(handle, clip, playback)
CrossFade(handle, clip, duration)
DestroyAnimationInstance(handle)
Evaluate(AnimationFrameContext)
DrainEvents() -> animation events
GetPoseOutput(handle) -> frame-scoped pose reference
```

### 5. Semantic state boundary

Entity/game code controls semantic intent:

```text
movement speed
grounded state
aim direction
weapon state
attack request
damage reaction
```

Game client code maps those values to `PlayClip` or `CrossFade`. AnimationSystem
does not contain a general gameplay state machine and does not decide that an
attack hit, an item was consumed, or damage should occur.

Replicate compact semantic animation state when possible rather than bone
transforms. The client reconstructs visual poses from the same compatible assets
and state.

### 6. Evaluation phases

An animation frame contains:

1. apply play/cross-fade commands;
2. advance clip times;
3. sample and blend local poses;
4. calculate model-space bones;
5. produce skinning palettes;
6. publish cosmetic events and frame-scoped outputs.

### 7. Movement

Gameplay and character physics own movement. Animation does not extract root
motion. Game code may select playback rate from actual velocity so the visual
pose follows simulation without becoming authoritative.

### 8. Animation events

Events identify authored timing such as footsteps, weapon-ready points, or
effect markers. AnimationSystem reports the event with instance, clip/state,
event ID, and simulation/presentation time.

Game code determines meaning:

```text
Footstep event
  -> entity selects surface/game sound
  -> audio command
  -> optional visual-effect command
```

Events are client cosmetic cues only. Authoritative actions use server timers or
confirmed replicated state; a client animation event cannot apply damage.

Event delivery defines loop, reverse playback, large time step, blend, replay,
and prediction-correction behavior to prevent duplicate or lost effects.

### 9. Rendering boundary

AnimationSystem produces frame-scoped skinning palette data. `RenderSystem`
owns GPU upload, buffer allocation, and skinning execution.

The renderer does not select clips, advance animation time, or emit gameplay
animation events.

Attachments needed by client presentation can consume published bone transforms.
Authoritative attachments use explicit game transforms.

### 10. Threading

- Animation runs on the Game Thread after interpolation and before render
  submission.
- Commands for many instances are applied as packed spans.
- Frame-scoped pose references expire after the Render Thread retires the
  submission that contains them.

### 11. Failure behavior

- Skeleton/clip incompatibility fails instance construction or state transition.
- Missing optional client animation uses a reference-pose or declared fallback.
- Invalid parameters and stale handles are rejected with instance/entity context.
- Failed evaluation does not publish a partial palette.

### 12. Invariants

- Entity/game code owns semantic animation intent.
- AnimationSystem owns mutable animation instances and calculated poses.
- RenderSystem owns GPU skinning resources.
- Client animation events cannot commit server-authoritative outcomes.
- Animation evaluation runs on the Game Thread and does not invoke entity
  behavior during pose traversal.
- Pose evaluation and event extraction perform no unbounded per-instance or
  per-bone heap allocation.

---

## Audio

> **Status: detailed target contract.** Implement only when the active milestone
> in `IMPLEMENTATION.md` promotes audio.

### 1. Purpose

This document defines `AudioSystem`, sound commands, mixing, spatial state,
device-thread behavior, client/server boundaries, and failure handling.

### 2. Responsibility

`AudioSystem` owns:

- audio device and callback integration;
- mixer and output format;
- buses, gains, and routing;
- sound voices and generation handles;
- listener state;
- spatialization and attenuation;
- decoded sound buffers;
- voice prioritization and budgets;
- device-loss and reinitialization policy.

It does not own:

- authoritative game events;
- entity lifetime;
- asset storage I/O;
- scene or network authority;
- the game-specific decision of which sound represents an event.

### 3. Client and server roles

Normal sound reproduction exists only in the client runtime. A dedicated server
does not create an audio device.

The server can emit replicated game events or state from which the client game
module chooses sound. For example, the server confirms that a door opened; the
client door entity issues an audio command appropriate to its presentation.

Client-predicted sounds declare correction behavior. Immediate local footsteps
or weapon feedback can play speculatively; durable outcomes must follow server
authority.

### 4. Public API

Conceptual commands include:

```text
PlayOneShot(desc)
CreateEmitter(desc) -> AudioEmitterHandle
UpdateEmitter(handle, update)
StopEmitter(handle, fade)
DestroyEmitter(handle)
SetBusParameter(bus, parameter, value)
SubmitSpatialFrame(listener and emitter transforms)
```

Calls produce bounded commands for the audio thread. They do not expose device
voices, library channels, or decode-buffer pointers.

Asset, bus, parameter, and event names resolve to stable IDs before entering the
audio command queue. Commands are fixed-size or reference producer-owned packet
storage with an explicit retirement fence; the callback never follows a graph
of game-owned allocations.

### 5. Assets

`AudioAsset` contains encoded sample data, loop points, channel/layout
information, and import settings.

`AssetService` owns storage reads and immutable encoded data. During scene
staging, `AudioSystem` decodes complete device-ready buffers. The audio callback
never performs file I/O, asset loading, allocation, decoding, or blocking waits.

### 6. Mixing and buses

The bus model includes:

- master;
- music;
- sound effects;
- dialog;

Buses own gain, mute, and routing policy. Voice limits and priorities prevent
unbounded work.

Game code controls semantic parameters; the mixer controls sample-accurate
ramps and device output.

### 7. Spatial audio

The client runtime submits immutable spatial state after interpolation:

- listener transform and velocity;
- emitter transforms and velocities;
- attenuation parameters;
- environment/room parameters when supported;
- game-time and frame metadata.

AudioSystem interpolates or smooths state at the device cadence. It never reads
live entity transforms from `ClientSimulation` on the callback thread.

Occlusion and obstruction queries are calculated outside the audio callback
using client presentation physics/query facilities, then submitted as parameters.

### 8. Events and deduplication

Game entities map semantic events to audio assets and parameters. AudioSystem
only executes sound commands.

Network and prediction paths attach stable event tokens when duplicate
suppression is required. A predicted event later confirmed by the server should
not necessarily play twice. Rejected predictions define whether to stop, fade,
or leave a short cosmetic sound unchanged.

### 9. Threading

- The audio callback owns real-time device interaction.
- The game/client thread writes bounded commands and spatial snapshots.
- Complete sound assets decode during scene staging on the Game Thread.
- The callback performs no blocking locks, file I/O, or arbitrary game code.
- Voice destruction uses command acknowledgement or generation-safe retirement.
- Shutdown drains or rejects new commands before closing the device.

### 10. Failure behavior

- No available audio device can degrade to a null backend when product policy
  permits the game to continue.
- Missing optional sounds produce diagnostics and silence or a declared fallback.
- Command-queue overflow uses explicit priority/drop policy and diagnostics.
- Invalid handles and parameters are rejected outside the callback when possible.
- Device reinitialization does not duplicate durable game events.

### 11. Invariants

- Audio is never authoritative for game state.
- Dedicated servers do not require AudioSystem.
- The callback never accesses entity storage or performs storage I/O.
- AudioSystem owns all device voices and decoded buffers.
- Game code selects semantic sounds; AudioSystem mixes them.
- Command overflow has an explicit policy.
