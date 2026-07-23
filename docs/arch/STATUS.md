# Architecture Implementation Status

> **Status: authoritative execution ledger.** Update this file whenever an M0
> vertical step starts or completes. Architecture documents define contracts;
> this file says what exists and what an implementation agent should do next.

## Reading path

1. Read this file to see what exists and what happens next.
2. Open [Implementation architecture](IMPLEMENTATION.md) only when changing the
   active design or implementing the current step.
3. Open [Delivery, promotion, and performance](DELIVERY.md) for milestone scope,
   complexity decisions, or acceptance gates.
4. Treat [Core](CORE.md), [Systems](SYSTEMS.md), and [Network](NETWORK.md) as
   searchable references. They are not required front-to-back reading.

## Active work

| Field | Value |
| --- | --- |
| active milestone | **M0: playable room** |
| active vertical step | **1: load and render one deterministic cooked room, with authored player placement** |
| state | in progress |
| next deliverable | a committed `assets/demo/m0/m0_room.cscene` fixture that loads through the target-asset path and renders its static `prop` entities |
| next automated gate | a headless fixture test that loads the M0 room and verifies its scene, entity, asset-reference, and static-content counts |
| explicitly deferred | authoritative tick loop, `UserCommand`, presentation snapshots, door behavior, remote transport, animation, audio, Render Thread; the temporary viewer player controller is not the future authoritative command path |

An agent should complete the next deliverable and automated gate before starting
vertical step 2. Existing divergences listed below are not independent cleanup
projects unless they block that step.

## Verified baseline

Verified on 2026-07-23:

```sh
cmake --preset mac-debug
cmake --build --preset mac-debug -j 6
ctest --test-dir build/mac-debug --output-on-failure
python3 -m unittest discover tools/ceasset/tests
```

Results:

- CMake configure and build passed;
- 10 of 10 CTest tests passed, including the cooked Sponza scene with its
  game-owned player entity;
- 112 asset-tool tests ran successfully (111 passed, 1 skipped because Pillow is
  supplied by the packaged Blender add-on rather than the host Python);
- Markdown links and `git diff --check` passed.

## Current implementation inventory

| Capability | State | Evidence and gap |
| --- | --- | --- |
| target asset headers/readers | implemented | `src/assets/asset_format.*`, typed readers, and asset tests exist |
| asset loading boundary | implemented for active formats | cooked file I/O and decoding live under `src/assets`; entities request typed mesh/material/texture values through `AssetDatabase`, while render backends only upload those values |
| game/entity schemas | implemented for `.cscene` entities | portable engine/viewer game JSON files generate standard-library-only, explicit little-endian C++ readers plus the flattened schema packaged in the Blender add-on |
| deterministic Python/Blender cooking paths | partial | exporters and 112 tests exist; no committed M0 room fixture exists |
| `.cscene` reading and validation | implemented | `src/assets/cscene_reader.*` plus malformed-range/version tests |
| scene loading and activation | partial | `src/assets/scene_loader.*` constructs an unpublished runtime scene; `src/scene` contains only live scene state/execution, but the committed M0 fixture is missing |
| generation-checked entity identity | implemented | `EntityId { index, generation }`, stale-handle tests, and reusable slots exist |
| initial entity storage | implemented | `Scene` stores `std::unique_ptr<Entity>` in generation slots |
| unified prop model | implemented | one `prop` entity class uses a single `dynamic` flag; static is the default |
| deferred structural changes | missing | create and destroy are currently immediate |
| scheduled entity behavior | missing | no authoritative scheduler or fixed-tick simulation root exists |
| main-thread renderer | implemented | the viewer owns one concrete `RenderSystem` instance and invokes it directly on the main thread; renderer state is instance-owned, each prop owns exactly one retained renderable record, and one backend is selected per build behind the existing `IRenderBackend` boundary |
| environment presentation | implemented, optional | typed `skybox` and `exponential_height_fog` scene entities drive HDR panorama sky rendering, diffuse/specular IBL, and analytic height fog; this does not change the active M0 gate |
| generation-checked render handles | implemented | reusable renderable and light slots carry generations and reject stale handles before reaching the backend |
| scene/render binding | partial/divergent | entity lifecycle methods currently create and update retained renderer records directly through `RenderSystem`; the M0 presentation snapshot remains deferred |
| basic Jolt bodies and fixed substeps | implemented | prop lifecycle methods create/destroy static or dynamic bodies through `PhysicsSystem`; the four-substep accumulator remains system-owned |
| M0 deterministic Jolt policy | implemented | Jolt runs through `JobSystemSingleThreaded` for the authoritative M0 step |
| generation-checked physics handles | implemented | reusable Jolt body slots carry generations and reject stale handles |
| queries, triggers, contacts, character path | missing | required M0 physics surface is not implemented |
| action snapshot and `UserCommand` | deferred | `InputSystem` owns a backend-neutral device API with GLFW isolated behind `GlfwInputBackend`; viewer-owned controls populate named frame actions consumed by entity updates, with no tick-addressed command yet |
| authoritative simulation/tick owner | missing | physics currently owns its own real-delta accumulator |
| `PresentationSnapshot` and `ClientView` | missing | renderer bindings currently update from live scene entities |
| player/controller | partial, game-owned | the viewer sample registers its own action IDs, default bindings, and `PlayerEntity`; collision-backed authoritative movement remains deferred |
| authored player placement and viewer possession | implemented ahead of the original step order | the viewer registers its generated player data with `EntityFactory`; its game-owned `PlayerEntity` publishes pose/lens parameters to the renderer-owned camera. The engine has no built-in player class. |
| game/add-on extension path | implemented | a temporary custom game/entity test generates and executes standalone C++, loads the same schema in Python, and the viewer build packages its flattened schema into the add-on |
| interactive door | missing | no door entity or recorded interaction path exists |
| M0 command capture/state hash | missing | no canonical declared-state capture exists |
| M0 performance harness | missing | profile targets exist but there is no `CEngineM0Profile` executable or JSON result writer |

## Known divergences that may remain temporarily

- `IPhysicsBackend` and `IRenderBackend` predate the simplified plan and remain
  owner-private implementation seams. Exactly one render backend is compiled
  into a build; there is no runtime backend option, enum, or configuration path.
- Current scene rendering reads entity transforms directly. Vertical step 5
  replaces that path with `PresentationSnapshot`; do not create a second parallel
  presentation architecture earlier.
- Existing PBR, shadows, transparency, lightmaps, SSAO, and Vulkan source may
  remain. They do not gate M0 and must not expand its public architecture.
- Existing `.casset` composition remains readable while the cooker/runtime asset
  boundary is migrated. It does not authorize a new public runtime prefab graph.

## M0 vertical steps

| Step | Exit condition | State |
| --- | --- | --- |
| 1. Cooked room | committed M0 fixture loads headlessly and renders its static `prop` entities | **in progress** |
| 2. Authoritative simulation | one fixed-tick owner and deferred entity changes run headlessly | pending |
| 3. Physics binding | deterministic single-thread Jolt path, generation handles, static collision, controller queries/triggers | pending |
| 4. Command path | actions become tick-addressed commands under the M0 sampling policy | pending |
| 5. Presentation boundary | complete snapshots drive `ClientView`; renderer stops reading entity storage | pending |
| 6. Playable slice | recorded player approaches and opens the authoritative door | pending |
| 7. Sign-off | command replay, failure injection, capacity, timing, and soak gates pass | pending |

## Updating this file

When a step completes:

1. link the implementation and tests in the inventory;
2. mark the completed step;
3. advance exactly one active step;
4. name one next deliverable and one automated gate;
5. record the verification commands and date;
6. do not mark a capability implemented when only an interface or placeholder
   exists.
