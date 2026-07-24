# CEngine Skeletal Animation System

> **Status: proposed design for the M3 presentation slice; not implemented.**
> [`arch/IMPLEMENTATION.md`](arch/IMPLEMENTATION.md) remains the active
> implementation contract. This document does not promote animation into the
> current milestone.

## Product goal

CEngine needs one client-side path from a Blender armature to a rendered
skinned mesh:

```text
Blender armature/actions
  -> evaluated, target-ready .cskel/.canim
  -> Store immutable assets
  -> AnimationSystem clip playback and cross-fade
  -> model-space joints and skinning palette
  -> RenderSystem GPU skinning
```

The first slice supports:

- reference-pose display;
- one looping or one-shot clip per instance;
- a timed cross-fade between two compatible clips;
- positive playback rate, pause, restart, and explicit normalized start time;
- cosmetic authored events;
- model-space joint output for presentation attachments;
- four-weight matrix-palette skinning in every mesh and shadow pass;
- bounded instances, events, CPU pose memory, and GPU palette memory.

Animation graphs, blend trees, partial/additive layers, root motion,
retargeting, IK, ragdoll blending, morph targets, reverse playback, animation
LOD, and worker evaluation remain deferred. Game code selects clips directly.

## Library decision

Use [ozz-animation](https://github.com/guillaumeblanc/ozz-animation) 0.16.0,
pinned to commit
`6cbdc790123aa4731d82e255df187b3a8a808256`.

Ozz is the narrowest dependency that supplies the complete low-level runtime
path required here:

- compressed immutable runtime skeletons and clips;
- [`SamplingJob`](https://guillaumeblanc.github.io/ozz-animation/documentation/animation_runtime/)
  and reusable sampling contexts;
- `BlendingJob` for the two-pose cross-fade;
- `LocalToModelJob`;
- renderer-independent output and no imposed animation graph;
- a separate
  [offline library](https://guillaumeblanc.github.io/ozz-animation/documentation/animation_offline/)
  for building and optimizing target data;
- tested desktop, ARM, and WebAssembly paths under the MIT license.

CEngine owns instance lifetime, playback policy, event meaning, resource
limits, renderer integration, and all public types. Ozz types appear only in
asset implementation files, the animation implementation, and the host cooker.
There is no runtime animation-backend registry or public `IAnimationBackend`.

ACL is not selected for the first slice. It is a strong compression and
decompression library, but CEngine would still need to own or adopt the pose
blending, hierarchy traversal, and local-to-model path that ozz already
provides. Assimp is an importer rather than a runtime animation system and does
not fit the cooked-runtime boundary.

### Build shape

The runtime build fetches the pinned revision with static libraries and sets:

```text
BUILD_SHARED_LIBS=OFF
ozz_build_tools=OFF
ozz_build_fbx=OFF
ozz_build_gltf=OFF
ozz_build_data=OFF
ozz_build_samples=OFF
ozz_build_howtos=OFF
ozz_build_tests=OFF
ozz_build_postfix=OFF
```

`CEngineAnimation` links `ozz_animation`; ozz samples, render code, importers,
and geometry skinning are not shipped. Ozz 0.16.0 requires CMake 3.24, so the
engine minimum moves from 3.22 to 3.24 when this design is implemented.

A host-only `CEngineAnimationProcessor` target links `ozz_animation_offline`. It accepts a
generated, tool-only raw skeleton/clip record from the Blender exporter,
validates and optimizes it, and writes an ozz runtime archive back to the
exporter. `schemas/animation.processor.json` owns this temporary interchange; it
is not an asset type and is not installed with the game. The Python exporter
then embeds the returned archive in the ordinary schema-owned CEngine payload.
Packaged tools place `CEngineAnimationProcessor` beside the exporter; development exports receive
its path explicitly. The offline library is not linked into the runtime.

The ozz commit participates in the cooker fingerprint. Upgrading ozz requires
reprocessing skeletons and clips and passing the full animation fixture gate.

## Ownership and identity

`Store` continues to own typed caches of
`shared_ptr<const Assets::Skeleton>` and
`shared_ptr<const Assets::Animation>`. These values hide their deserialized
ozz runtime objects behind private implementation data. A failed ozz tag,
archive, hierarchy, count, or metadata validation is not published in the
cache.

`AnimationSystem` owns generation-checked `AnimationInstanceHandle` slots.
Each live instance retains:

- one skeleton asset;
- its current clip and optional cross-fade destination clip;
- two ozz sampling contexts sized at instance creation;
- current/destination local-pose buffers;
- one blended local pose;
- model-space joint matrices;
- the final skinning palette;
- two clip cursors and bounded cross-fade state.

Instance creation is the only point that allocates pose storage. Evaluation
does not resize a vector or allocate per joint, curve, or event. Destroying a
live slot releases retained assets, clears its pose ranges, increments the
generation, and makes stale commands fail.

The renderer does not receive an animation handle. A `MeshInstance` remains
the retained placement record; its optional skinning storage is renderer-owned
and sized once when a skinned instance is registered.

The application owns `AnimationSystem` beside `Store`, `PhysicsSystem`, and
`RenderSystem`; `Context` carries the same kind of optional non-owning pointer
used for the other systems. In the current direct scene flow, one frame is:

```text
Entity::Update
PhysicsSystem::Step
AnimationSystem::Evaluate
Entity::LateUpdate
RenderSystem::Render
```

Entities issue play/cross-fade intent in `Update`. Animation evaluates after
the fixed physics step. `LateUpdate` then reads final physics transforms,
combines them with any model-joint attachment, and copies the evaluated palette
to its retained mesh instance. A later client-simulation path places the same
evaluation stage after interpolation and before presentation extraction.

## Public engine surface

The first concrete API is intentionally smaller than an animation graph:

```cpp
struct AnimationSystemDesc {
    std::uint32_t max_instances;
    std::uint32_t max_events;
    std::uint32_t max_pose_joints;
};

struct AnimationInstanceDesc {
    std::shared_ptr<const Assets::Skeleton> skeleton;
};

struct AnimationPlayback {
    float rate = 1.0f;
    float normalized_start = 0.0f;
    bool looping = true;
};

AnimationInstanceHandle CreateInstance(const AnimationInstanceDesc&);
bool Play(AnimationInstanceHandle,
          std::shared_ptr<const Assets::Animation>,
          const AnimationPlayback&);
bool CrossFade(AnimationInstanceHandle,
               std::shared_ptr<const Assets::Animation>,
               float duration_seconds,
               const AnimationPlayback&);
bool SetPaused(AnimationInstanceHandle, bool);
void DestroyInstance(AnimationInstanceHandle);
void Evaluate(float presentation_delta_seconds);
std::span<const AnimationEvent> Events() const;
std::span<const glm::mat4> SkinningPalette(AnimationInstanceHandle) const;
std::optional<glm::mat4> ModelJoint(AnimationInstanceHandle,
                                    std::uint16_t joint) const;
```

The names are illustrative; the invariants are normative:

- create returns an invalid handle on failure and publishes no partial slot;
- play and cross-fade require the clip's skeleton GUID and joint count to match
  the instance skeleton;
- all time, rate, and duration arguments must be finite;
- baseline playback rate is non-negative;
- a zero-duration cross-fade is a direct `Play`;
- `Play` cancels any active transition and starts from the requested cursor;
- a cross-fade with no current clip blends from the rest pose;
- a cross-fade that replaces an active transition uses the last published
  blended local pose as a frozen source, so it needs no third sampling context
  and emits no source events;
- evaluation with zero delta preserves the pose and emits no event;
- pose and event access reject stale handles;
- output spans expire at the next `Evaluate`, instance destruction, or system
  shutdown.

Game/entity code owns semantic decisions such as idle versus run. A game may
map measured velocity to a clip and playback rate. `AnimationSystem` never
decides gameplay state and never calls entity behavior during evaluation.

## Cooked assets

The current `.cskel` hierarchy and `.canim` scalar F-curves prove the authoring
and schema path, but are not sufficient runtime input. Raw Blender F-curve
paths do not encode evaluated constraints, complete local TRS tracks, Bezier
handles, or the canonical joint remap expected by ozz.

Because no runtime evaluator consumes these payloads yet, implementation
replaces them in place and recooks fixtures. It does not add a compatibility
version or a second animation format.

### Skeleton payload

The target `.cskel` record contains:

```text
name
joints[]
  name
  parent                         // parent-before-child, depth-first
  joint_from_armature_bind[16]   // inverse armature-space rest matrix
ozz_runtime_archive[]
```

The maximum is 1,024 joints, matching
`ozz::animation::Skeleton::kMaxJoints`. Joint names are unique. There is at
exactly one root at joint zero, each non-root parent precedes its child,
matrices are finite and invertible, and the joint count/order/names/parents in
CEngine metadata exactly match the deserialized ozz skeleton.

The canonical order is computed once in the Blender cooker and is shared by:

- ozz skeleton and clip tracks;
- inverse-bind matrices;
- mesh vertex joint indices;
- attachment joint indices.

Every skinned `.cmesh` stores the compatible skeleton GUID in its payload.
Unskinned meshes store a zero GUID. The mesh loader validates normalized
weights, zero-weight unused indices, and joint indices against the 1,024-joint
format cap; animated-instance creation additionally validates every nonzero
influence against the selected skeleton's actual joint count.

### Animation payload

One `.canim` remains one clip:

```text
name
skeleton reference
duration_seconds
events[]
  time_seconds
  id                             // FNV-1a 64 of the authored name
  name                           // retained for diagnostics
ozz_runtime_archive[]
```

Events are sorted by time, lie in `[0, duration]`, and are capped at 4,096 per
clip. The cooker rejects a hash collision between distinct event names.
Duplicate event IDs at different times are valid.

The loader verifies:

- a non-empty clip name and positive finite duration;
- a valid skeleton reference;
- a bounded non-empty archive with the expected ozz animation tag;
- archive duration equal to metadata within the cooker tolerance;
- archive track count equal to the referenced skeleton count when the pair is
  bound;
- sorted, finite events with correct stable IDs.

The ozz archive is an opaque target payload, not a public engine type.
Replacing ozz is a recook migration. CEngine's envelope, GUID, type, source
hash, path validation, and semantic metadata remain engine-owned.

## Blender and host-cooker rules

For every exported action, Blender evaluates the armature pose at the declared
sample rate over the action range. It does not copy scalar F-curves.
Evaluation therefore bakes constraints and driver results that affect the
deform skeleton.

The exporter:

1. validates one armature root transform and a canonical depth-first joint
   order;
2. derives each rest joint's local TRS from its armature-space rest matrix and
   its parent's inverse;
3. computes the inverse armature-space bind matrix;
4. samples every joint's evaluated local TRS for every clip sample;
5. normalizes quaternion signs between adjacent samples;
6. sends raw local TRS data to `CEngineAnimationProcessor`;
7. lets ozz optimize and compress the clip;
8. verifies the returned runtime joint order and track count;
9. packs the runtime archive and engine event metadata through the generated
   schema writer.

The baseline rejects non-finite transforms, singular bind matrices, shear,
negative scale, and non-uniform rest or animated scale. This permits one matrix
palette for positions, normals, and tangents in the initial GPU path. Support
for inverse-transpose normal palettes is a measured later extension.

Timeline markers with the prefix `ce_event:` become cosmetic events. The suffix
is the authored event name; empty names and hash collisions are errors.
Markers outside the action range are ignored with an export diagnostic.

## Evaluation and event semantics

`Evaluate` runs on the main Game Thread after the fixed physics step and before
entity `LateUpdate` and `RenderSystem::Render`:

1. clear the fixed-capacity frame event buffer;
2. advance current and destination cursors;
3. sample current and destination local poses with `SamplingJob`;
4. blend two layers with `BlendingJob` while a transition is active;
5. fall back to the skeleton rest pose when no clip is active;
6. run `LocalToModelJob`;
7. calculate `armature_from_joint_pose * joint_from_armature_bind` for each
   skinning matrix;
8. publish model joints, palettes, diagnostics, and cosmetic events.

During a cross-fade, the source cursor continues to advance. Source events are
eligible while source weight is greater than `0.5`; destination events are
eligible when destination weight is greater than or equal to `0.5`. This gives
one deterministic event owner at every point in the transition.

Events use forward half-open traversal semantics:

- ordinary advance reports markers in `(previous, current]`;
- a loop reports `(previous, duration]`, then `[0, current]`;
- starting a clip at time zero does not implicitly fire a zero-time marker;
- one-shot playback clamps at the duration, reports the final crossing once,
  and keeps the final pose without repeating events;
- restart first resets the cursor, so subsequent advancement is unambiguous;
- pause and zero delta emit nothing.

A large delta may cross multiple loops. Evaluation computes the crossing count
without an unbounded loop, writes events only up to `max_events`, and increments
a dropped-event diagnostic for the remainder. Cosmetic events cannot apply
damage, movement, inventory changes, or other authoritative outcomes.

## Renderer boundary

`RenderSystem` owns GPU palette upload and skinning. Registration of a skinned
mesh instance includes its skeleton joint count and a reference-pose palette;
registration fails if the mesh has no skeleton GUID, the skeleton differs, or
the renderer's declared palette budget would be exceeded.

Before `Render`, presentation code copies the instance's current palette with a
direct update:

```text
AnimationSystem::SkinningPalette(animation)
  -> RenderSystem::UpdateMeshSkinning(mesh_instance, palette)
```

The retained renderer slot owns fixed-size CPU palette storage, so the update
does not retain an animation pointer and does not allocate. Static mesh
instances have no palette storage and continue down the existing path.

The OpenGL 3.3 backend packs visible palettes into one streaming texture-buffer
allocation per frame. Each draw item carries a base matrix index. Vertex,
deferred geometry, forward, directional-shadow, and point-shadow vertex
shaders all apply the same four-weight skin matrix. Positions use the full
matrix; normals and tangents use the upper 3x3 and are normalized.

The backend validates the texture-buffer capacity before drawing. Overflow is
reported and the affected skinned instance is skipped; it is never drawn with
another instance's palette. Vulkan may continue to reject skinned registration
until its mesh path is implemented, but its public backend contract must
compile.

Skinned bounds use conservative cooked animation bounds for the initial slice.
The cooker unions sampled deformed mesh bounds across every exported clip and
stores that bound on the mesh. Per-frame CPU vertex deformation and per-bone
bounds are not introduced.

## Failure and diagnostics

Expected failures include asset path and GUID context plus the skeleton, clip,
mesh, or instance identity when available.

Diagnostics expose:

- live and high-water instance counts;
- evaluated instances and joints this frame;
- active cross-fades;
- CPU pose bytes and GPU palette bytes/high-water;
- rejected stale commands and incompatible asset bindings;
- dropped cosmetic events;
- asset decode and ozz job failures.

If an optional presentation animation fails, the caller may keep the instance
in its validated reference pose. A required animated fixture fails scene
activation. Evaluation never publishes a partially written pose or palette.

## Vertical delivery and automated gates

Implementation proceeds only after animation is promoted into the active
milestone:

1. **Dependency and assets:** pin ozz, add `CEngineAnimationProcessor`, replace the two payload
   records, recook fixtures, and reject malformed archives and mismatched
   metadata.
2. **Reference pose:** bind one skinned mesh to one skeleton, upload the
   reference palette, and render it in color and all shadow passes.
3. **Playback:** add generation slots and one looping clip using
   `SamplingJob` and `LocalToModelJob`.
4. **Cross-fade:** add the second preallocated sampling path and timed
   `BlendingJob` transition.
5. **Events and attachment:** add bounded marker traversal and one
   presentation attachment test.
6. **Budget gate:** exercise the declared character/joint workload and verify
   stable allocation counts after warmup.

Required automated evidence:

- schema round trips for `.cskel`, `.canim`, and the mesh skeleton GUID;
- Blender tests for canonical remap, evaluated local TRS, inverse binds,
  markers, malformed scale/shear, and deterministic output;
- asset tests for truncated/wrong-tag ozz archives, hierarchy/count mismatch,
  NaN/Inf, singular binds, event order/range/hash, and GUID mismatch;
- handle create/destroy/reuse and stale-command tests;
- exact sampling tests at start, midpoint, end, pause, loop, and one-shot end;
- cross-fade endpoint and monotonic-weight tests;
- event tests for ordinary advance, loop, large delta, transition ownership,
  restart, pause, and overflow;
- CPU reference-pose and animated-pose palette fixtures;
- an OpenGL image fixture proving deformation in the main, directional-shadow,
  and point-shadow passes;
- a Vulkan-selected compile/link check;
- a soak test proving no per-frame allocation growth after instance creation;
- the repository's complete build, CTest, Python, and `git diff --check` gates.

The initial checked workload declares its character count, joints per
character, clips, event rate, CPU pose budget, GPU palette budget, and target
machine before performance sign-off. A worker pool, animation LOD, or compressed
GPU palette is promoted only if that workload misses its measured budget after
the direct main-thread path is profiled.

## Deliberate exclusions

This design does not add:

- an animation graph, state machine, parameter registry, or string-selected
  transition language;
- root motion or server-side pose evaluation;
- partial, additive, masked, IK, procedural, or ragdoll layers;
- retargeting between skeleton GUIDs;
- runtime clip building, runtime archive conversion, or source-format import;
- animation worker threads, queues, or a general job scheduler;
- per-bone objects or entity callbacks during traversal;
- a second mesh representation or CPU skinning fallback;
- runtime dependency selection or compatibility loaders for old unconsumed
  animation payloads.
