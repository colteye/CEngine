# Physics System Contract

This document defines what “standard 3D physics” means for CEngine and records
the implemented path. [`arch/IMPLEMENTATION.md`](arch/IMPLEMENTATION.md)
defines ownership and identity rules; [`arch/STATUS.md`](arch/STATUS.md) records
the current verification state.

## Product requirements

A complete path must let an author declare collision in Blender, cook a
deterministic engine-neutral asset, load it without Blender, create Jolt state,
advance at a caller-owned fixed tick, query it, receive normalized events, and
tear it down without exposing Jolt objects.

The standard surface is:

- static, dynamic, kinematic, and sensor bodies;
- box, sphere, capsule, cylinder, static-plane, convex-hull, triangle-mesh,
  height-field, and compound shapes;
- friction, restitution, mass, damping, gravity scale, sleeping, CCD, initial
  velocity, and six-axis locks;
- 32 collision layers with a symmetric pair matrix and independent query masks;
- transform/velocity state, teleport, kinematic targets, forces, torque, and
  impulses;
- closest and bounded-all ray casts, convex shape casts, and bounded overlaps;
- contact and trigger begin/persist/end events with bounded overflow behavior;
- fixed, point/ball, hinge, slider, distance/spring, and cone constraints;
- velocity/position motors for hinge and slider constraints;
- one virtual character path with slopes, steps, grounding, jumping, crouch
  resizing, moving-ground state, and penetration recovery.

Movable triangle meshes, height fields, and planes are intentionally
unsupported. Dynamic and kinematic bodies use finite primitives, convex hulls,
or compounds made only from movable shapes.

## Render meshes and collision meshes

Rendering and physics consume separate cooked values:

```text
Blender object
  render geometry + materials  -> .cmesh + .cmat
  collider declaration/source  -> .cphys

prop scene record
  MeshInstance binding         -> RenderSystem
  collision + body settings    -> PhysicsSystem
```

`.cmesh` contains render attributes and an index buffer. `.cphys` contains only
physics-ready shape data. A collision asset may be generated from the visible
mesh or from an explicit collision-only child mesh. The helper’s scale is baked
into its geometry and its local translation/rotation is stored on the root
shape. Collision helpers do not become visible props.

Runtime code never derives concave collision from a render mesh. This keeps
load behavior deterministic, makes malformed data independently rejectable,
and allows render and collision topology to evolve separately.

## Runtime ownership

`PhysicsSystem` owns:

- Jolt initialization, allocators, listeners, layer filters, and world state;
- reusable body, constraint, and character slots;
- reverse Jolt body-ID lookup used to normalize query/event results;
- the bounded pending contact-event buffer.

Callers own engine values and opaque generation handles. `PhysicsShape` is a
call-scoped creation/query value; `CreateBody` builds and retains a Jolt shape
before returning. No Jolt reference crosses the public header.

Body descriptions contain configuration only. Collision geometry is a separate
argument so large shape trees are not copied into every body description.
Mutable state is read through `PhysicsBodyState`.

## Fixed tick and transform authority

The application advances a 60 Hz accumulator and calls:

```text
entity pre-step commands
PhysicsSystem::Step(fixed_delta)
entity post-step state readback
```

- static bodies take their authored transform once;
- kinematic entities issue a target before the step;
- dynamic bodies own their simulated position and rotation after the step;
- rendering consumes the entity’s post-step transform.

`PhysicsSystem` never reads a wall clock and never silently substeps.

## Filtering, queries, and events

Each body has one layer in `[0, 31]`. `SetLayerCollision(a, b, enabled)` updates
both directions. Query masks select layers without changing physical response,
and queries may ignore one body.

Query output is engine-owned and bounded:

- ray and shape casts return fraction, world position/normal, body handle, and
  sub-shape ID;
- all-ray results are sorted by fraction;
- overlaps are stable, unique body handles;
- null output/capacity and stale ignore handles are safe.

Jolt callbacks append minimal raw contact data up to the configured capacity.
Drain maps live Jolt IDs to generation handles and returns normalized
begin/persist/end records. Overflow increments a monotonic dropped-event count.

## Constraints

Constraint entities refer to two scene props by semantic entity index. Blender
authors those references by object name and writes constraints after body
entities, so both bodies exist during activation. The constraint entity owns
the live constraint handle and destroys it during reverse scene shutdown.

Anchors and axes are currently world-space:

- the constraint entity transform position is the first anchor;
- `second_anchor` is the second body anchor;
- `axis` and perpendicular `normal` define hinge/slider frames;
- `minimum`, `maximum`, frequency, and damping define limits/springs;
- hinge/slider motors use Off, Velocity, or Position mode with a target,
  force/torque limit, frequency, and damping.

Invalid limits, axes, body references, movable concave shapes, or motors on an
unsupported constraint fail before a live handle remains published.

## Character

The character shape is a Z-up capsule whose origin is at its bottom. The
runtime supports velocity commands, gravity, stairs, floor sticking, slope
classification, ground body/velocity reporting, and safe capsule-height
replacement. The viewer converts the authored eye transform to this bottom
origin and back, applies planar WASD/sprint movement, and jumps on Space when
grounded.

## Blender requirements

The add-on must:

- expose explicit None/Static/Dynamic/Kinematic motion;
- expose collider kind, body coefficients, layer, sensor, CCD, sleeping, and
  locks through schema-generated fields;
- fit primitive values from object dimensions;
- cook evaluated collision topology deterministically;
- support an explicit parented collision-only object;
- build square uniform-grid height fields and ordered child compounds;
- reject non-finite/degenerate data and movable concave shapes;
- preview primitive bounds or source wireframes;
- author connected objects, anchors, axes, limits, springs, and motors;
- report object-specific validation errors before export.

## Cooked `.cphys` validation

The version-2 payload contains a fixed header, preorder shape records, and
separate vertex, index, and height arrays. The loader validates:

- asset and physics magic/version/header sizes;
- file/table ranges, strides, counts, caps, and integer overflow;
- known shape kinds, one root, preorder parent relationships, and nesting cap;
- finite transforms with nonzero rotations (normalized before Jolt) and
  positive primitive dimensions;
- convex point counts;
- triangle topology and index bounds;
- square height sample count and positive scale;
- non-empty compounds and shape-specific unused/range invariants.

Only a fully validated `PhysicsShape` is published.

## Explicit remaining limits

- no break-force or break-torque constraint thresholds;
- no local-frame constraint gizmo;
- no physics-material asset/density policy (coefficients are per body);
- no Jolt debug-draw bridge;
- no step-time, active-body, shape-memory, or high-water telemetry;
- no content-hash collision-asset deduplication;
- no viewer crouch binding, although runtime height changes are supported.

These are future work only when a concrete consumer and test justify the added
surface.
