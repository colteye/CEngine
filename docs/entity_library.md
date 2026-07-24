# Base Entity Library

> **Status: implemented, 2026-07-23.** This document records the deliberately
> small engine-owned entity set. Game-specific behavior remains native game
> code instead of growing the base library into a catalogue of genre features.

## Design basis

CEngine keeps Source's strongest level-authoring idea: named concrete entities
with validated inputs and outputs. A connection records the source output,
target input, optional parameter, delay, and bounded fire count. Runtime
dispatch preserves caller/activator identity, orders equal-time work
deterministically, and bounds pending work.

Unity and Unreal inform the modern subsystem split. Cameras, audio emitters,
colliders, triggers, lights, and presentation settings bind directly to their
owning systems. They are concrete entity classes in CEngine because the current
scene format and game extension model use native entity types, not a public
component framework.

The resulting set covers the placeable primitives needed to assemble a level
without claiming that an engine-owned door, pickup, damage volume, checkpoint,
NPC, or weapon can express every game's rules.

## Implemented classes

| Entity | Engine responsibility | Blender representation |
| --- | --- | --- |
| `prop` | rendered mesh, optional rigid body, visibility inputs | Mesh |
| `collider` | non-rendered static/dynamic/kinematic rigid body and contact outputs | Mesh used only for `.cphys` |
| `trigger_volume` | non-rendered sensor, enter/exit/stay/trigger outputs, rigid-body and character overlap | Mesh used only for `.cphys` |
| `physics_constraint` | fixed, point, hinge, slider, distance, or cone relationship | Empty |
| `light` | renderer light binding | Light |
| `camera` | selectable renderer view and audio-listener source | Camera |
| `audio_source` | 2D/3D playback, buses, attenuation, cones, loops, fades, and playback I/O | Speaker |
| `audio_environment` | the current single global reverb environment | Empty |
| `skybox` | global image-based lighting and sky presentation | Empty |
| `fog` | global height fog | Empty |
| `post_process` | global post process and SSAO settings | Empty |
| `logic_relay` | immediate or delayed fan-out, optional fire once | Empty |
| `logic_timer` | start delay, repeat, reset, and explicit fire | Empty |
| `logic_auto` | scene-activation output | Empty |
| `marker` | generic named transform for paths, targets, and game code | Empty |

All classes share `Enable`, `Disable`, and `Toggle` inputs and
`OnEnabled`/`OnDisabled` outputs. Class-specific endpoints are declared in
`schemas/engine.game.json`; both the Python cooker and the C++ scene loader
reject connections that do not match those declarations.

## Scene and runtime contract

- Connections are resolved to generation-checked entity handles during scene
  loading. Strings are retained only for declared endpoint names and optional
  parameters.
- Delayed inputs use scene simulation time. Work emitted while dispatching is
  deferred to the next dispatch pass, preventing recursive unbounded call
  chains.
- Physics bodies and virtual characters register their owning entity with the
  scene. Contact records are drained after the physics step and delivered only
  on the simulation thread.
- A Jolt virtual character has an inner query/contact proxy. Physics queries
  distinguish body and character hits, and trigger activators resolve to the
  player entity.
- The active scene view must reference an enabled `camera` or game `player`
  entity. Blender's active scene camera becomes that serialized reference.
- Blender Camera, Light, Speaker, and Mesh values are used directly where their
  semantics match. Custom schema properties cover only engine-specific values.
- Blender output connections are authored on the source object and validated
  against source and target schemas before cooking.

## Intentionally game-owned

The base library does not add specialized doors, buttons, moving platforms,
teleports, hurt/heal volumes, pickups, checkpoints, counters, comparisons,
filters, dialogue, quests, AI nodes, player spawn points, or spawn managers. These have
game-specific authority, save, damage, inventory, team, and networking rules.
They can use `trigger_volume`, logic entities, markers, props/colliders, and
native game entity classes without changing the engine scene contract.

The viewer demonstrates this boundary with a game-owned `player_spawn`
entity and `GameCoordinator`. Spawn points provide team/group/priority and
clearance metadata; the coordinator owns join, respawn, disconnect, and
primary-view policy and creates live `PlayerEntity` instances through
`Scene::CreateEntity`.

The library also omits entities whose subsystem does not exist yet: animated
actors, decals, reflection probes, local audio volumes, navigation links,
terrain, and runtime particle emitters. They should be promoted only with a
real runtime path, Blender authoring, schema validation, and automated gates.
