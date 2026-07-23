# CEngine Delivery, Promotion, and Performance

> **Status: consolidated reference.** Milestone scope, complexity-promotion rules, general performance policy, and the active M0 profile.
> `STATUS.md` and `IMPLEMENTATION.md` determine what is active; this file
> preserves detailed target information without expanding current scope.

## Contents

- [Release Scope](DELIVERY.md#release-scope)
- [Tradeoffs and Promotion](DELIVERY.md#tradeoffs-and-promotion)
- [Performance Envelope](DELIVERY.md#performance-envelope)
- [M0 Performance Profile: MacBook Air M4](DELIVERY.md#m0-performance-profile-macbook-air-m4)

## Release Scope

> **Status: authoritative milestone scope and build order.** `IMPLEMENTATION.md`
> owns the active architectural contracts. Detailed subsystem documents preserve
> later target behavior but do not add it to the current milestone.

See [Architecture implementation status](STATUS.md) for the currently active
milestone and vertical step.

### 1. Rule

Each milestone proves one complete, measurable path. It is not a partial
implementation of every document in this directory.

A mechanism enters the active milestone only when its exit gates require it.
Formats and APIs do not contain placeholders for later milestones.

### 2. M0: playable room

The initial path is:

```text
cooked room -> fixed authoritative ticks -> Jolt physics
            -> complete presentation snapshot -> rendered frame
```

M0 contains:

- one statically linked game configuration;
- one synchronous cooked scene loaded during startup;
- `prop` entities with one explicit static/dynamic flag and
  system-owned batching for static render/collision bindings;
- one player/controller and one interactive door;
- concrete native entities stored in generation-checked pointer slots;
- scheduled behavior and deferred spawn/destruction;
- one authoritative fixed-tick simulation;
- Jolt-backed rigid bodies, queries, triggers, and character movement;
- one complete post-tick presentation snapshot;
- one lightweight `ClientView` with retained presentation records;
- one compiled graphics backend running on the main thread;
- the minimum existing material, lighting, debug, and UI support required to
  inspect and play the room;
- device input mapped to actions and `UserCommand`;
- diagnostics, stable-memory checks, orderly startup/shutdown, and headless
  command-capture tests;
- one checked-in product/performance profile with concrete numbers;
- deterministic cached/no-op and single-dependency content cooking within the M0
  iteration budgets.

M0 does not require remote networking, paired client entity classes, prediction,
interpolation, delta snapshots, relevance, animation, audio, a Render Thread,
async loading, map travel, live scene transactions, scripting, or hot reload.

#### 2.1 Playable-slice acceptance scenario

The checked-in M0 fixture and command capture define one exact scenario:

1. Load `assets/demo/m0/m0_room.cscene` and its declared content build.
2. Spawn the player two meters from a closed door.
3. Apply forward movement at two meters per second for ticks 1 through 30.
4. Emit exactly one `interact` impulse at tick 31; no later tick repeats it.
5. The authoritative door changes from `Closed` to `Opening` at tick 31, then
   advances one fixed-step increment during ticks 32 through 151, moving two
   meters at one meter per second with a kinematic collision body.
6. At tick 151 the door is `Open`; its authoritative transform, collision-body
   transform, and `ClientView` presentation transform agree.
7. The headless run matches the checked-in declared-state hash. A render-facade
   test observes the same final transform without reading entity storage.

The fixture may contain additional static room records, but no optional renderer
feature may change this outcome or gate the test.

### 3. M1: local authority boundary

M1 adds:

- bounded in-process command and snapshot queues;
- stable presentation/network candidate identity;
- byte serialization and validation for commands and complete snapshots;
- complete-snapshot application by `ClientView`;
- loopback parity, malformed-input, and capacity tests.

M1 still does not add UDP, delta baselines, relevance, prediction, or a second
general-purpose client entity runtime.

### 4. M2: remote multiplayer

M2 adds:

- a proven transport dependency;
- dedicated-server and remote-client compositions;
- protocol and content negotiation;
- complete bounded snapshots over the packet path;
- interpolation of remote state;
- prediction/reconciliation only for local character movement;
- declared packet loss, jitter, reorder, bandwidth, reconnect, and security
  tests.

Acknowledged baselines, dirty groups, relevance, and prioritization are added only
if the checked-in M2 workload misses its bandwidth or CPU budget.

### 5. M3: production presentation and iteration

M3 adds the subset required by the selected game slice from:

- clip playback, cross-fade, skinning, and cosmetic animation events;
- audio playback, buses, spatial state, and prediction deduplication;
- UI composition and presentation polish;
- automatic recook/restart, scene reload, and richer authoring diagnostics;
- shader iteration support;
- explicit C++, asset, and shader edit-to-result budgets.

### 6. Deferred measured promotions

The following remain outside the active milestone until the evidence in
`DELIVERY.md#tradeoffs-and-promotion` exists:

- class-specific entity slabs or a public ECS;
- a shared task scheduler or general job graph;
- Render Thread, RHI thread, or renderer worker pool;
- render graph or runtime backend selection;
- delta snapshot baselines and complex interest management;
- asynchronous loading, streaming, map travel, and large-world rebasing;
- multiple live asset revisions and content patching;
- animation graphs, root motion, IK, layering, and procedural animation;
- server rewind, navigation, persistence, replay, and split-screen;
- scripting, dynamic game modules, and game-code hot reload;
- runtime prefabs and editor plugins.

These features retain detailed design references in this directory. Retaining the
information does not authorize placeholder code or serialized fields.

### 7. Vertical implementation order

1. Load one cooked room and render its static `prop` entities.
2. Add one authoritative fixed-tick owner and deferred structural changes.
3. Add the deterministic Jolt path, generation handles, static collision,
   queries/triggers, and one controller path.
4. Convert actions into tick-addressed `UserCommand` records.
5. Capture complete snapshots, apply `ClientView`, and remove renderer reads of
   entity storage.
6. Add the player, authoritative door, and checked-in acceptance capture.
7. Pass command replay, failure injection, capacity, timing, and soak gates.

Each step must run, fail cleanly, and remain measurable. Infrastructure not
exercised by the current step is left unwritten. M1 begins only after every M0
exit gate passes.

---

## Tradeoffs and Promotion

> **Status: active change-control policy.** This document decides when a deferred
> mechanism described elsewhere becomes part of the implementation contract.

### 1. Purpose

This document records why CEngine chooses its current constraints, what cost it
accepts, and what evidence permits a more complex design. It is the only place
for promotion criteria. Subsystem documents preserve target contracts;
`IMPLEMENTATION.md`, `DELIVERY.md#release-scope`, and `STATUS.md` decide which contracts
are active now.

### 2. Promotion rule

A proposal for additional machinery includes all of the following:

1. a named shipping game or workflow that cannot meet its requirement;
2. a reproducible workload on a pinned machine or toolchain;
3. the measured failure of the current direct design;
4. the cheapest local alternative and its measured result;
5. the new owner, lifetime, thread, memory bound, failure behavior, and overload
   policy;
6. an acceptance test that fails before the change and passes after it;
7. the public API or format cost, including migration when applicable.

Possible future use, symmetry, backend fashion, and “flexibility” are not
promotion evidence. A local implementation is preferred until two real owners
need the same mechanism.

### 3. Core choices

| Choice | Benefit | Cost accepted | Promotion criterion |
| --- | --- | --- | --- |
| concrete native entities | direct game code, fixed fields, simple debugging | heterogeneous behavior is not one flat array | a shipped game demonstrates repeated composition churn that is simpler and faster in another public model |
| private packed system records | hot bulk work stays contiguous | entity and system representations are paired by handles | retain; this is the data-oriented layer and does not require a public ECS |
| server authority in single- and multiplayer | one game-semantics path and real trust boundary | local play still applies commands and snapshots | change only if the product permanently drops multiplayer |
| fixed authoritative tick | reproducible ordering and stable physics/network time | overload produces lateness rather than variable steps | another time model passes prediction, replay, and physics tests with lower total cost |
| snapshot replication | supports late join, relevance, loss, and heterogeneous clients | baselines and prediction history consume bounded memory | lockstep is considered only for a game with deterministic simulation and tiny synchronized inputs |
| static game linking | ordinary typed C++, fastest build/debug path | no binary mods or live game-code replacement | a shipping mod or deployment workflow requires independently replaceable binaries |
| synchronous complete-asset loading | one load state and simple lifetimes | loading screens and a bounded resident working set | measured load latency or memory exceeds the product envelope |
| one complete scene | no residency graph, cell protocol, or cross-partition references | map size is bounded by precision and memory | a captured shipping map exceeds the working set or float precision |
| one graphics backend per build | no runtime dispatch or lowest-common-denominator API | backend choice happens at build time | two supported backends must ship in the same executable |
| one main ownership thread | direct ordering, debugging, and no engine cross-thread protocol | CPU/GPU overlap is initially limited | add a Render Thread or local workers only when a named packed loop misses budget after local data/layout optimization |
| backend-private parallelism | Jolt or another dependency owns its data and joins at the facade | the engine does not schedule arbitrary work there | add engine subsystem-local workers only for a measured owner-local workload |
| gameplay-owned navigation helper | AI rules remain local and direct | no universal tiled path service | a game requires shared paths that exceed a bounded grid/waypoint helper |

### 4. Feature promotion

| Feature | Current path | Required evidence |
| --- | --- | --- |
| async loading | load complete assets during startup or a loading screen | measured loading overlap is required to hit a declared transition budget |
| asset detail streaming | load complete release assets | real content exceeds CPU/GPU/audio memory with measured access patterns |
| spatial scene streaming | load one scene with packed chunks | a shipping traversal workload exceeds memory or coordinate precision |
| content hot reload | restart or reload the scene | iteration telemetry shows restart time is a material production bottleneck |
| map travel | reconnect and load one scene | a shipping game specifies retained player state and failure UX |
| split-screen | one local player | a shipping game specifies users, views, audio listeners, and input routing |
| persistence framework | explicit game-owned checkpoint file | a persistent game supplies schema, slots, platform storage, and migration requirements |
| navigation system | game-local bounded search | at least two agent types share tiled data, obstacles, and asynchronous query budgets |
| server rewind | current-tick authoritative hits | competitive fairness policy specifies history duration, memory, and accepted command time |
| animation graph | game code selects clips and cross-fades | repeated game code demonstrates a stable reusable graph vocabulary |
| authoritative animation/root motion | gameplay timers and physics own outcomes | a shipping move requires bone/root evaluation for authority and prediction |
| IK, layering, procedural animation | cooked clips and cross-fade | the approved character target cannot be met with authored clips |
| Render Thread | main thread owns rendering and fixed passes | measured CPU/GPU overlap is required and a bounded submission/retirement protocol passes stall tests |
| Jolt worker pool | authoritative M0 physics uses the single-threaded job system | a captured physics workload misses its CPU budget and the worker-count correctness/tolerance contract passes |
| RHI thread | selected rendering owner submits graphics commands | driver submission is its measured bottleneck |
| renderer worker pool | selected rendering owner performs culling/draw preparation | one identified packed render loop misses its CPU budget |
| GPU-driven rendering | CPU frustum culling and instancing | captured scenes are CPU submission limited after batching |
| advanced lighting/effects | baseline PBR, shadows, sky, tone mapping | an approved visual target names the technique and its frame/memory budget |
| dynamic game modules | static linking | binary mods or independent deployment are shipping requirements |
| scripting | native entities | content-team iteration data justifies VM, debugging, safety, and binding cost |
| transport mechanics | use a proven transport beneath CEngine messages and own only integration/policy mapping | own transport mechanics only when a measured required semantic cannot be provided or adapted safely |
| audio device and mixer | use a proven small audio foundation and own only integration/backend policy | own low-level device/mixer code only when a named audio requirement cannot be met behind the command boundary |

### 5. Dependency choices

Dependencies are judged by total owned code and failure risk avoided:

| Dependency | Choice | Tradeoff |
| --- | --- | --- |
| SDL3 | use | one portable platform layer supplies window, events, input state, graphics surfaces, clocks, and audio devices |
| Jolt Physics | use | avoids owning a robust 3D solver, broad phase, contacts, constraints, and character collision |
| GLM | use internally | compact existing math dependency; its types are not a binary API promise |
| GLAD | use with OpenGL | generated API loading is mechanical and backend-private |
| nv_dds | prefer tool/cooker use | runtime consumes validated cooked texture payloads rather than a general image API |
| Vulkan backend | build only when selected | it does not gate the active milestone backend or create runtime selection machinery |
| GameNetworkingSockets or ENet | evaluate at M2 | transport supplies tested connection/reliability mechanics while CEngine retains command, snapshot, and replication meaning |
| miniaudio | use without device I/O | decoding, mixing, buses, effects, and 3D spatialization stay behind CEngine audio commands while SDL3 owns platform devices |

Dependencies are pinned to immutable revisions or vendored snapshots. Unused
examples, tests, tools, and backends stay disabled. Third-party types stop at
the owning implementation.

Scene and snapshot layouts remain small, typed, and domain-validated, so they do
not use a general serialization framework. CEngine owns command, snapshot,
replication, validation, and authority meaning. At M2 it first adapts a proven
transport for connection, sequencing, reliability, fragmentation, and security
mechanics. Owning those mechanics requires measured evidence that adaptation
cannot meet the shipping contract.

---

## Performance Envelope

> **Status: active acceptance policy.** The current milestone supplies one
> concrete checked-in workload profile as required by `IMPLEMENTATION.md`. M0
> uses [M0 performance profile](DELIVERY.md#m0-performance-profile-macbook-air-m4).

### 1. Purpose

This document makes CEngine's scalability claims falsifiable. It defines the
cost model, the minimum reference workloads, and the evidence required before
an architectural optimization or abstraction is accepted.

Performance is not one number. A product ships a named, versioned envelope that
states its hardware, operating system, resolution, tick and frame rates, player
count, content workload, memory limits, and network conditions. Results without
that input are anecdotes.

### 2. Fundamental budgets

For target frame rate `F` and authoritative tick rate `T`:

```text
frame interval ms = 1000 / F
tick interval ms  = 1000 / T
usable budget     = interval - OS/device/network safety reserve
```

At 60 frames or ticks per second the interval is 16.67 ms. The default server
acceptance gate uses 14.0 ms at p99, preserving 2.67 ms for scheduling and I/O
variation. A product may choose another tick rate, but it must recalculate the
budget rather than silently use a variable step.

For per-connection payload limit `B` bytes/s and snapshot rate `S`:

```text
average snapshot payload <= B / S
server payload egress    <= clients * B
history bytes            <= history ticks * retained bytes per tick
```

Memory is equally explicit:

```text
resident bytes = immutable payloads + live slots + histories
               + retained backend state + transient high-water marks
```

Allocations are not a substitute for a missing capacity decision. Each term has
a configured bound, high-water instrumentation, and a defined response when the
bound is reached.

### 3. Required product profile

Every executable configuration supplies this data before performance sign-off:

| Input | Examples of what it fixes |
| --- | --- |
| reference machine | CPU, core count, RAM, GPU, VRAM, storage, OS, drivers |
| schedule | server tick rate, snapshot rate, target frame rate |
| session | clients, expected RTT, jitter, loss, and reconnect rate |
| simulation | resident entities, scheduled entities, spawns/tick, timers, events |
| physics/game AI | bodies, contacts, queries, and path-search expansions |
| presentation | visible instances, lights, shadows, bones, voices, transient draws |
| scene | static instances, dynamic objects, visible work, and asset bytes |
| network | relevant entities/client, dirty values/tick, payload and history bytes |
| loading | cold start and scene activation latency limits |

The checked-in profile contains concrete numbers. “Typical,” “large,” and
“modern hardware” are not valid values.

### 4. Capacity comes from the product

CEngine does not invent universal entity, player, light, or scene counts. Those
numbers would turn guesses into premature storage and optimization work. The
vertical-slice game supplies its profile; additional games add independent
profiles without changing prior results.

For each bounded resource, the profile records:

```text
observed peak -> shipping capacity -> response at capacity + 1
```

Shipping capacity includes explicit headroom justified by content or session
variance. It is not automatically doubled and it is not “unlimited.” Tests run
at the declared capacity and deliberately exceed it once to verify rejection,
quality reduction, baseline recovery, or disconnect behavior.

The performance run pins a reference-machine and workload manifest beside its
results. A timing claim is invalid when either manifest is missing. Until a
profile has measured results, its provisional targets may drive acceptance-test
construction but not claims of demonstrated scale. Only representation
invariants—such as no resident-entity scan, no unbounded allocation fallback,
and no allocation or blocking inside a real-time callback—may justify an
optimization without a captured result. Specialized slabs, arenas, and worker
protocols require a captured workload or an active backend constraint.

### 5. Cost must follow relevant work

| Area | Cost may scale with | Cost must not inherently scale with |
| --- | --- | --- |
| simulation | scheduled entities, commands, timers, structural changes | every resident entity each tick |
| physics | awake islands, contacts, submitted queries | every static shape each tick |
| replication | relevant dirty state and connections | all entities times all connections |
| rendering | candidate chunks, visible work, lights affecting views | every authored scene item each frame |
| animation | evaluated instances, active bones, selected LOD | every loaded animation asset |
| game pathfinding | requested searches and bounded expansions | every agent times the entire search space |
| audio | active/virtualized voices and mixed samples | every authored sound source |

An optimization that improves constants but changes the right-hand column into
recurring work is rejected.

### 6. Acceptance gates

Each applicable profile passes all of the following:

1. Server tick p99 fits its usable budget; client CPU and GPU p99 independently
   fit their declared budgets.
2. The audio callback and declared cross-thread real-time paths make zero
   general-purpose heap allocation calls. Tick, rendering, physics, and packet
   paths report allocation counts and satisfy the checked-in profile; a profile
   may promote any of them to a tested zero-allocation contract.
3. Memory reaches a stable band during steady state. Long runs show no monotonic
   growth in resident payloads, slots, histories, queues, or deferred work.
4. No correctness-critical queue overflows. Induced overflow produces the
   documented rejection, baseline, quality degradation, or disconnect behavior.
5. Once a milestone includes packet transport, a loopback session and a
   packet-transport session given the same accepted command stream produce the
   same authoritative tick hashes.
6. Once a profile enables multiple workers, every supported worker count
   produces the same declared gameplay results or stays within an explicitly
   documented numeric tolerance. M0 instead verifies that authoritative Jolt
   remains single-threaded.
7. Scene activation, device loss, malformed packets/assets, and capacity
   exhaustion leave no partially published state.
8. Metrics report counts, bytes, time, high-water marks, drops, and fallbacks per
   owner. An unexplained aggregate frame time is insufficient evidence.

Measurements include median, p95, p99, maximum, warm-up policy, run duration,
build configuration, and content hash. Average-only results do not pass.

---

## M0 Performance Profile: MacBook Air M4

> **Status: active provisional development target; measurements pending.** The
> numbers below are test ceilings, not shipping capacities, until observed peaks
> and justified headroom are recorded. Replace pending results before M0 sign-off.

### Reference environment

| Item | Value |
| --- | --- |
| machine | MacBook Air, Apple M4 |
| CPU | 10 cores: 4 performance, 6 efficiency |
| GPU | integrated Apple M4, 10 cores |
| memory | 16 GiB unified memory |
| architecture | arm64 |
| operating system | macOS 26.3.1 |
| compiler | Apple Clang 17.0.0 |
| CMake | 4.2.1 |
| graphics path | selected OpenGL development backend |
| CMake preset | `mac-release` |
| framebuffer | 1920 x 1080, windowed, scale resolved to physical pixels |
| timing mode | VSync off; no frame cap during capture |
| normal play mode | VSync on, 60 Hz target |
| authoritative Jolt jobs | single-threaded M0 job system |
| tick catch-up limit | 4 ticks per outer frame |
| configuration | optimized build with diagnostics and profiling counters enabled |

This machine is the M0 development reference, not a declaration that it is the
eventual minimum shipping machine. A shipping product adds its own profile.

### Schedule and latency targets

| Metric | Target |
| --- | --- |
| authoritative tick | 60 Hz fixed step |
| displayed frame | 60 Hz |
| server tick CPU | p99 <= 4.0 ms, maximum <= 8.0 ms |
| complete presentation capture/apply CPU | p99 <= 1.0 ms |
| total client CPU frame | p99 <= 12.0 ms |
| total GPU frame | p99 <= 12.0 ms using graphics API timer queries |
| cold configure excluded | build-tool setup is not a runtime result |
| cold room activation | <= 5.0 s |
| cached no-op content build | <= 1.0 s |
| one changed texture/mesh dependency cook | <= 3.0 s excluding Blender bake |
| one ordinary C++ source edit to relink | <= 10.0 s after initial build |

Timing capture uses 600 warm-up ticks/frames followed by 36,000 measured
ticks/frames. The 30-minute soak is separate. Reports include median, p95, p99,
maximum, rejected samples, content hash, commit, preset, compiler, framebuffer,
and whether the window was visible or offscreen. Missing GPU timer support fails
the graphical profile rather than silently skipping its gate.

### Capacity workload

| Resource | Observed M0 peak | Provisional test ceiling | Ceiling + 1 behavior |
| --- | --- | --- | --- |
| resident entities | pending | 128 | scene/startup rejection with diagnostic |
| scheduled entities per tick | pending | 32 | new schedule rejected; existing schedule preserved |
| structural spawns per tick | pending | 16 | excess spawn group rejected atomically |
| deferred destroys per tick | pending | 32 | overload is recorded; bounded backlog policy preserves correctness |
| static render instances | pending | 2,048 | scene/startup rejection |
| visible render instances | pending | 1,024 | the workload fails; correctness is not truncated |
| retained dynamic render objects | pending | 128 | creation rejected without a published handle |
| dynamic lights | pending | 4 | lower-priority presentation light omitted with diagnostic |
| physics bodies | pending | 256 | creation rejected without a published handle |
| contact/trigger records per tick | pending | 512 | correctness workload fails; no silent loss |
| synchronous physics queries per tick | pending | 64 | additional query rejected with explicit status |
| complete snapshot entities | pending | 128 | snapshot build fails without partial publication |
| snapshot bytes | pending | 64 KiB | snapshot rejected and high-water diagnostic emitted |
| input actions | pending | 64 | invalid binding/content rejected during load |

M0 contains zero animation bones, audio voices, remote clients, packet loss,
network history, and streaming residency. Those dimensions belong to later
profiles.

### Memory targets

| Memory class | Target |
| --- | --- |
| total process resident memory after warm-up | <= 2 GiB |
| immutable CPU asset payload | <= 512 MiB |
| renderer/GPU-visible payload estimate | <= 512 MiB |
| simulation, physics, presentation, and transient CPU state | <= 256 MiB |
| growth during a 30-minute steady-state run | no monotonic growth after warm-up |

Every owner reports current bytes, peak bytes, capacity, rejected work, and the
reason for any fallback.

### Correctness gates

1. Run at declared capacity for at least 30 minutes after warm-up.
2. Exceed every bounded resource once and observe the declared behavior.
3. Replay one accepted command capture at least 100 times and compare declared
   authoritative state after every tick.
4. Verify the authoritative profile uses the single-threaded Jolt job system and
   rejects a configuration that silently enables a worker pool.
5. Inject failure into every fallible startup stage and verify no live scene,
   entity, physics body, or render handle remains published.
6. Verify stale entity, physics, and render handles cannot address reused slots.
7. Report allocation counts per phase. Real-time callback and cross-thread paths,
   when present, must remain allocation-free; other paths must meet their timing
   and stable-memory targets without unbounded fallback.

Multi-worker physics correctness is an M4 promotion gate, not an M0 acceptance
gate.

### Reproducible commands

The required sign-off target is currently missing and is tracked in `STATUS.md`.
When vertical step 7 begins, it must support these stable commands:

```sh
cmake --preset mac-release
cmake --build --preset mac-release --target CEngineM0Profile

./build/mac-release/CEngineM0Profile \
  --fixture assets/demo/m0/m0_room.cscene \
  --capture tests/fixtures/m0_door.commands \
  --warmup-ticks 600 \
  --measure-ticks 36000 \
  --output build/profile/m0-room.json

./build/mac-release/CEngineM0Profile \
  --synthetic-capacity \
  --soak-ticks 108000 \
  --output build/profile/m0-capacity-soak.json
```

The executable exits nonzero when any applicable gate fails. JSON output records
the exact fixture/capture hashes, executable commit, environment, phase samples,
percentiles, maxima, allocations, owner memory, capacities, drops, fallbacks, and
failure-injection results. The graphical run uses API timer queries; a separate
headless mode runs authoritative replay and failure tests without a window.

### Results

| Run | Content/fixture | Commit | Result |
| --- | --- | --- | --- |
| playable room | pending | pending | pending |
| synthetic capacity | pending | pending | pending |
| 30-minute soak | pending | pending | pending |
| failure injection | pending | pending | pending |
