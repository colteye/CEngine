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
| active vertical step | **1: load and render one deterministic cooked room** |
| state | in progress |
| next deliverable | a committed `assets/demo/m0/m0_room.cscene` fixture that loads through the target-asset path and renders its static `prop` entities |
| next automated gate | a headless fixture test that loads the M0 room and verifies its scene, entity, asset-reference, and static-content counts |
| explicitly deferred | authoritative tick loop, `UserCommand`, presentation snapshots, door behavior, remote transport, animation, audio, Render Thread |

An agent should complete the next deliverable and automated gate before starting
vertical step 2. Existing divergences listed below are not independent cleanup
projects unless they block that step.

## Verified baseline

Verified on 2026-07-21:

```sh
cmake --preset mac-debug
cmake --build --preset mac-debug -j 6
ctest --test-dir build/mac-debug --output-on-failure
python3 -m unittest discover tools/ceasset/tests
```

Results:

- CMake configure and build passed;
- 8 of 8 CTest tests passed;
- 79 asset-tool tests ran successfully (78 passed, 1 skipped because Pillow is
  supplied by the packaged Blender add-on rather than the host Python);
- Markdown links and `git diff --check` passed.

## Current implementation inventory

| Capability | State | Evidence and gap |
| --- | --- | --- |
| target asset headers/readers | implemented | `src/assets/asset_format.*`, typed readers, and asset tests exist |
| deterministic Python/Blender cooking paths | partial | exporters and 78 tests exist; no committed M0 room fixture exists |
| `.cscene` reading and validation | implemented | `src/assets/cscene_reader.*` plus malformed-range/version tests |
| scene loading and activation | partial | `src/scene/scene_loader.*` constructs an unpublished scene; the viewer binds render/physics state before entity activation, but the committed M0 fixture is missing |
| generation-checked entity identity | implemented | `EntityId { index, generation }`, stale-handle tests, and reusable slots exist |
| initial entity storage | implemented | `Scene` stores `std::unique_ptr<Entity>` in generation slots |
| unified prop model | implemented | one `prop` entity class uses a single `dynamic` flag; static is the default |
| deferred structural changes | missing | create and destroy are currently immediate |
| scheduled entity behavior | missing | no authoritative scheduler or fixed-tick simulation root exists |
| main-thread renderer | implemented | the viewer invokes the retained renderer directly on the main thread; one backend is selected per build behind the existing `IRenderBackend` boundary |
| generation-checked render handles | implemented | reusable renderable and light slots carry generations and reject stale handles before reaching the backend |
| scene/render binding | partial/divergent | `SceneRenderState` creates retained objects, but dynamic updates directly read entity storage rather than applying a presentation snapshot |
| basic Jolt bodies and fixed substeps | implemented | static/dynamic prop boxes, spheres, and a four-substep accumulator exist |
| M0 deterministic Jolt policy | implemented | Jolt runs through `JobSystemSingleThreaded` for the authoritative M0 step |
| generation-checked physics handles | implemented | reusable Jolt body slots carry generations and reject stale handles |
| queries, triggers, contacts, character path | missing | required M0 physics surface is not implemented |
| action snapshot and `UserCommand` | missing | current controls read GLFW directly and mutate the camera |
| authoritative simulation/tick owner | missing | physics currently owns its own real-delta accumulator |
| `PresentationSnapshot` and `ClientView` | missing | renderer bindings currently update from live scene entities |
| player/controller | missing | only free-camera controls and `info_player_start` data exist |
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
