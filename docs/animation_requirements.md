# CEngine Skeletal Animation System

> **Status: implemented first vertical slice.**
> The Blender exporter writes backend-neutral engine assets. The runtime
> animation backend converts those assets to its private representation.

## Product goal

CEngine provides one client-side path from a Blender armature to a rendered
skinned mesh:

```text
Blender armature/actions
  -> raw engine .cskel/.canim
  -> Store immutable backend-neutral assets
  -> AnimationSystem playback policy
  -> IAnimationBackend
       -> Ozz backend conversion, sampling, blending, model joints
  -> RenderSystem GPU skinning
```

The first slice supports:

- reference-pose display;
- looping and one-shot clip playback;
- timed cross-fades, including interruption from a frozen blended pose;
- non-negative playback rate, pause, restart, and normalized start time;
- bounded cosmetic events;
- model-space joint output for presentation attachments;
- four-weight matrix-palette skinning in geometry, forward, depth, and point
  shadow passes;
- generation-checked instances and explicit instance, event, and pose budgets.

Animation graphs, partial/additive layers, root motion, retargeting, IK,
ragdoll blending, morph targets, reverse playback, worker evaluation, and
animation LOD remain deferred. Game code selects clips directly.

## Library and backend decision

The Ozz backend uses
[ozz-animation](https://github.com/guillaumeblanc/ozz-animation) 0.16.0,
pinned to commit
`6cbdc790123aa4731d82e255df187b3a8a808256`.

Ozz supplies skeleton and clip construction, sampling contexts, two-pose
blending, local-to-model evaluation, and SIMD pose storage. CEngine owns public
types, asset formats, validation, instance handles, playback and event policy,
resource limits, and renderer integration.

Ozz is replaceable behind `IAnimationBackend`, following the same ownership
shape as physics and audio:

```text
AnimationSystem
  owns commands, time, transitions, events, handles, budgets
        |
        v
IAnimationBackend
  owns representation conversion and pose evaluation
        |
        v
Ozz::AnimationBackend
  owns every ozz skeleton, clip, context, and pose object
```

No Ozz header, type, archive, or naming appears in `AnimationAsset`,
`SkeletonAsset`, the scene layer, game code, or renderer. Ozz references are
confined to `src/animation/ozz`.

### Build shape

The pinned static dependency is configured with:

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

`CEngineAssets` does not link Ozz. `CEngineCore` privately links the required
Ozz runtime and offline-building libraries because the Ozz backend converts
engine records into Ozz objects at runtime. There is no separate animation
processor executable, processor JSON, Blender-to-Ozz bridge, or Ozz data in an
exported asset.

Replacing Ozz requires a new `IAnimationBackend` implementation, not a Blender
export change or asset migration.

## Ownership and frame order

`Store` caches `shared_ptr<const Assets::Skeleton>` and
`shared_ptr<const Assets::Animation>`. These are immutable engine records.

`AnimationSystem` owns generation-checked `AnimationInstanceHandle` slots and:

- current and destination clip ownership;
- playback cursors and pause/loop/rate policy;
- cross-fade timing and interruption policy;
- bounded cosmetic-event traversal;
- capacity and diagnostics.

The selected backend owns:

- converted runtime skeletons and clips;
- sampling contexts;
- source, destination, blended, and frozen local poses;
- model-space joint matrices;
- final skinning palettes.

The application owns `AnimationSystem` beside `Store`, `PhysicsSystem`, and
`RenderSystem`. `Context` carries a non-owning animation pointer. Scene order is:

```text
Entity::Update
PhysicsSystem::Step
AnimationSystem::Evaluate
Entity::LateUpdate
RenderSystem::Render
```

`prop` entities load their mesh, skeleton, and optional default animation,
validate their identities, create the reference pose or playback during
activation, and copy the evaluated palette to the renderer during
`LateUpdate`.

## Public engine surface

The first API is deliberately smaller than an animation graph:

```cpp
struct AnimationSystemDesc {
    std::uint32_t max_instances = 1024;
    std::uint32_t max_events = 4096;
    std::uint32_t max_pose_joints = 65536;
};

struct AnimationInstanceDesc {
    std::shared_ptr<const Assets::Skeleton> skeleton;
};

struct AnimationPlayback {
    float rate = 1.0f;
    float normalized_start = 0.0f;
    bool looping = true;
};

bool Initialize(const AnimationSystemDesc& = {});
void Shutdown();
AnimationInstanceHandle CreateInstance(const AnimationInstanceDesc&);
bool DestroyInstance(AnimationInstanceHandle);
bool Play(AnimationInstanceHandle,
          std::shared_ptr<const Assets::Animation>,
          const AnimationPlayback& = {});
bool CrossFade(AnimationInstanceHandle,
               std::shared_ptr<const Assets::Animation>,
               float duration_seconds,
               const AnimationPlayback& = {});
bool SetPaused(AnimationInstanceHandle, bool);
void Evaluate(float presentation_delta_seconds);
std::span<const AnimationEvent> Events() const;
std::span<const glm::mat4> SkinningPalette(AnimationInstanceHandle) const;
std::optional<glm::mat4> ModelJoint(AnimationInstanceHandle,
                                    std::uint16_t joint) const;
```

The invariants are:

- create publishes no partial slot and returns an invalid handle on failure;
- play and cross-fade require matching skeleton GUID and track count;
- time, rate, and transition inputs are finite and rates are non-negative;
- a zero-duration cross-fade is a direct play;
- play cancels the active transition;
- transition interruption freezes the last blended backend pose;
- zero delta preserves the pose and emits no event;
- stale handles are rejected;
- returned pose spans expire on later evaluation or instance destruction.

Game/entity code owns semantic decisions such as idle versus run.
`AnimationSystem` does not decide gameplay state or invoke entity behavior
during pose traversal.

## Exported engine assets

The Blender exporter evaluates the armature, constraints, and drivers and
writes complete engine-local TRS samples. It never constructs or serializes an
Ozz object.

### Skeleton payload

One `.cskel` contains:

```text
name
bones[]
  name
  parent                         // parent-before-child, depth-first
  rest
    translation[3]
    rotation[4]
    scale[3]
  joint_from_armature_bind[16]   // inverse armature-space rest matrix
```

The exporter and loader validate:

- exactly one root at index zero;
- no more than 1,024 joints;
- unique names and parent-before-child depth-first order;
- finite rest transforms, normalized quaternions, and positive uniform scale;
- finite, invertible inverse-bind matrices.

Canonical joint order is shared by skeleton bones, animation tracks, inverse
bind matrices, and mesh vertex joint indices.

Every skinned `.cmesh` stores its skeleton GUID. Unskinned meshes store a zero
GUID. Mesh loading validates four influences, weight normalization, unused
indices, and the 1,024-joint format cap.

### Animation payload

One `.canim` contains:

```text
name
skeleton reference
duration_seconds
events[]
  time_seconds
  id                             // FNV-1a 64 of authored name
  name
tracks[]                         // one track per canonical skeleton bone
  samples[]
    time_seconds
    value
      translation[3]
      rotation[4]
      scale[3]
```

The exporter samples evaluated local poses across the action range and keeps
quaternion signs continuous. Timeline markers prefixed `ce_event:` become
cosmetic events.

The loader validates a positive duration, typed skeleton reference, bounded
track/sample counts, strictly increasing sample times, endpoints inside the
clip range, finite transforms, normalized quaternions, positive uniform scale,
sorted event times, and matching event hashes.

The runtime Ozz backend transforms these neutral records into
`RawSkeleton`/`RawAnimation`, invokes Ozz builders, and retains the resulting
runtime objects privately.

## Evaluation and events

For each live instance, `Evaluate`:

1. advances CEngine-owned cursors;
2. chooses one deterministic event owner during a transition;
3. sends neutral clip pointers, normalized sample ratios, and blend weight to
   `IAnimationBackend`;
4. lets the backend sample, blend, calculate model joints, and build the
   skinning palette;
5. publishes diagnostics, events, model joints, and palettes.

The Ozz backend calculates each skinning matrix as:

```text
armature_from_joint_pose * joint_from_armature_bind
```

Events use forward traversal:

- ordinary advance reports markers in `(previous, current]`;
- loop traversal reports the tail and then the beginning;
- zero delta and pause emit nothing;
- one-shot playback emits the final crossing once and holds the last pose;
- large deltas calculate crossings without an unbounded loop;
- overflow increments a dropped-event counter.

During a cross-fade, source owns events below weight `0.5`; destination owns
events at and above `0.5`. Events are cosmetic and cannot apply authoritative
game outcomes.

## Renderer boundary

`RenderSystem::UpdateMeshSkinning` accepts engine matrices and forwards them to
the selected render backend. The renderer never receives an animation handle,
Ozz object, clip, or skeleton.

The OpenGL backend owns one texture buffer per retained skinned mesh instance.
Palette updates stream `GL_RGBA32F` matrix data. Each skinned draw binds its
palette buffer and joint count; static draws use an identity skin transform.
The geometry, forward, depth-only, and point-shadow vertex paths all consume
the same four joint indices and weights.

Registration/update validation rejects empty, oversized, or non-finite
palettes. A skinned draw without a valid palette is skipped. The incomplete
Vulkan backend explicitly rejects skinning updates while continuing to compile
against the renderer contract.

## Failure and diagnostics

Expected failures include asset path/GUID context and relevant skeleton, clip,
mesh, or instance identity. A required animated prop fails scene activation if
its asset tuple or backend conversion is invalid.

Diagnostics expose:

- live and high-water instances;
- evaluated instances and joints;
- active cross-fades;
- live and high-water pose joints;
- rejected commands and stale handles;
- dropped cosmetic events;
- backend conversion/evaluation failures.

Evaluation does not publish a partially written palette.

## Verification

Automated coverage includes:

- schema generation and cross-language skeleton/animation fixtures;
- Blender canonical joint order, inverse binds, evaluated local sampling,
  quaternion continuity, marker validation, and scene bindings;
- backend-neutral asset loading and typed `Store` caching;
- an Ozz backend regression that converts raw engine assets and verifies the
  sampled midpoint palette;
- playback, pause, marker, cross-fade, destroy, and stale-handle behavior;
- renderer/backend compilation for skinning in color and shadow paths;
- complete C++ and Python suites plus `git diff --check`.

## Deliberate exclusions

This slice does not add:

- an animation graph, state machine, parameter registry, or string transition
  language;
- root motion or server-side pose evaluation;
- partial, additive, masked, IK, procedural, or ragdoll layers;
- retargeting between skeleton GUIDs;
- source-format import in the runtime;
- worker threads, queues, or a general job scheduler;
- per-bone entities or callbacks during pose traversal;
- CPU skinning fallback;
- runtime backend selection or plugin discovery.
