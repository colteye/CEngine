# CEngine Mesh-Map and `.cscene` Implementation Plan

## Direction

CEngine maps are Blender-authored arrangements of reusable assets and gameplay
entities. They are mesh based: no BSP, brushes, portals, PVS authoring, generic
transform tree, or required manual optimization pass.

The runtime is deliberately small and Source-like. A scene owns one flat entity
list. Every entity has a canonical classname and fixed C++ fields. `Entity` is a
shallow base; concrete types are direct `final` subclasses with their own `.h`
and `.cpp` files. Relationships are explicit. Classnames remain strings—there is
no classname hash, numeric class ID, schema registry, or family of per-type scene
creation methods. A small factory is the single classname-to-C++ mapping point.

## File and ownership boundaries

The source layout is:

```text
src/assets/              asset database, .cscene format and reader
src/entity/entity.*      shared entity identity, flags, transform, lifecycle
src/entity/<type>.*      one fixed concrete entity type per pair of files
src/entity/entity_factory.*
src/scene/scene.*        scene ownership, slots, handles and scene lifetime
```

`.cscene` stores only scene composition: entity identity, classname, placement,
fixed placement fields, scene settings, explicit relationships/events, lightmap
bindings, and optional scene-specific acceleration data.

Reusable data remains external:

| Data | Referenced asset |
| --- | --- |
| Reusable placed object/prefab | `.casset` |
| Geometry, UVs, bounds, submeshes, LODs | `.cmesh` |
| Shader and material parameters | `.cmat` |
| Textures and lightmap pixels | DDS |
| Skeletons | `.cskel` |
| Animations | `.canim` |
| Reusable collision | `.cphys` or a documented `.cmesh` section |

In particular, a scene placement may reference a `.casset`; `.cscene` does not
copy the `.casset` object, mesh, or material payload. Direct `.cmesh` references
are also allowed for simple prop entities. Default materials come from the
referenced asset. A placement stores only genuine overrides.

## Runtime entity model

`Scene` owns a slot array of `std::unique_ptr<Entity>`. A transient `EntityId`
contains slot index and generation, making stale handles detectable. Serialized
relationships use compact file-local entity indices and resolve to runtime
handles during loading. There is no persistent entity UUID or separate ID folder.

The initial concrete types are:

| Classname | Type | Fixed data beyond base fields |
| --- | --- | --- |
| `empty` | `EmptyEntity` | none |
| `prop_static` | `StaticProp` | mesh/asset refs, material overrides, lightmap binding, visibility |
| `prop_dynamic` | `DynamicProp` | mesh/asset refs, material overrides, runtime flags |
| `light` | `LightEntity` | kind, mode, color, intensity, range and angles |
| `camera` | `CameraEntity` | projection, FOV/size and clip planes |
| `trigger` | `TriggerEntity` | bounds and trigger flags |
| `prefab_instance` | `PrefabEntity` | `.casset` reference |
| `info_player_start` | `PlayerStart` | team |

The player follows the same rule: a `Player` entity with fixed player fields and
behavior, not a bag of runtime-attached data. Game-specific types live outside
the core library and can register an explicit factory extension later.

`Scene::CreateEntity(classname, name)` asks the factory for the concrete
object, assigns identity, and installs it in a slot. Adding a built-in class means
adding its two source files, disk record, serializer/deserializer, and one factory
entry. Creation/loading may allocate; steady-state render and update loops should
not allocate.

### Lifecycle

The first implemented hooks are `OnLoaded`, `OnSceneReady`, `OnEvent`, and
`OnStop`. Loading order is:

```text
construct → deserialize → OnLoaded → resolve all references/assets
          → OnSceneReady → activate scene
```

`OnStop` runs before an entity or scene releases active external state. Runtime
spawn initialization and opt-in update/fixed-update hooks can be added when real
gameplay requires them. Static entities do not receive pointless frame calls;
rendering, physics, animation, and audio remain explicit bulk systems.

Structural mutations requested during dispatch are queued until a safe point.
CPU multithreading is intentionally deferred; nothing here requires choosing a
job system now.

### Relationships

Parenting is not first class. Blender hierarchy is baked into world transforms.
Entity dependencies are named fields such as owner, follow target, attachment,
or door target, storing file-local indices on disk and resolved handles at
runtime. Each owning entity defines update order, cycles, and destruction policy.
Events store source entity index, event name/type, target entity index, action
name/type, optional value, and delay.

## `.cscene` v1

The packed payload layout is:

```text
DiskSceneHeader
DiskSceneSettings
DiskAssetReference[]
DiskSceneEntity[]
DiskEntityClassBlock[]
class entity-index arrays
fixed concrete-class records
class-specific auxiliary arrays
future connection/reference tables
UTF-8 string table
```

Entity rows contain classname string range, name string range, and flags.
Class blocks contain classname, version, flags, count, fixed record stride,
entity indices, record bytes, and optional auxiliary bytes. For example, static
prop records contain transform and compact indices for mesh/material/lightmap
assets. Equal concrete types are grouped for compact loading and iteration, but
remain ordinary entities in memory.

The asset table contains deduplicated GUID, expected type, and normalized
project-relative path. The writer preserves final entity order, sorts assets by
canonical path and class blocks by classname, aligns tables, zeroes padding, and emits
identical bytes for identical input.

The reader validates the common asset envelope, every overflow-sensitive range,
stride, index, version, and string. The scene loader then works transactionally:

1. parse into temporary storage;
2. resolve and validate every external asset;
3. create each concrete entity through the factory;
4. deserialize its fixed class record;
5. resolve file-local entity relationships and connections;
6. run `OnLoaded`, then `OnSceneReady` after the whole scene is valid;
7. prepare render/physics state and atomically replace the active scene.

Failure destroys temporary state and leaves the current scene untouched.

## Blender export and dependencies

The basic author workflow is: place reusable assets, choose exceptional
classnames/settings, connect gameplay behavior, then bake/export. Collections
organize authoring only and do not become runtime hierarchy nodes.

Conversion defaults:

- a reusable collection/asset placement references its `.casset`;
- a direct mesh object becomes `prop_static`;
- a movable mesh becomes `prop_dynamic`;
- cameras, lights, empties and retained collection instances map to their named
  entity classes;
- every Blender parent chain is composed into an exported world transform.

Before writing `.cscene`, export performs dependency completion:

1. collect all referenced `.casset`, `.cmesh`, `.cmat`, DDS, `.cskel`, `.canim`,
   collision and lightmap targets;
2. determine each normalized project-relative output path and expected type;
3. reuse valid, current outputs;
4. generate missing or stale outputs whenever the referenced Blender/source data
   is available, using the normal exporter;
5. follow dependencies transitively, including those referenced by `.casset`;
6. report the responsible entity/source when generation is impossible;
7. atomically publish `.cscene` only after every referenced asset validates.

This guarantees references exist without bloating the scene. Unrelated and
orphan source data is ignored.

## Automatic map preparation

Authors do not create batches, visibility cells, atlas rectangles, or mesh merge
groups. The engine/compiler derives world bounds from asset bounds, groups
compatible placements for instancing, caches static extraction, selects asset LOD
defaults, builds a BVH or loose grid, and performs frustum culling. Later HZB/GPU
occlusion, streaming cells, meshlets, or HLOD can consume the same source scene.

Configuration follows project defaults, then asset defaults, then rare placement
overrides. Event-addressable or movable entities are never destructively merged.

## Lightmaps

Blender export selects an existing lightmap UV layer or creates UV1 without
altering UV0, estimates resolution from surface area and texel density, packs
atlases deterministically, and bakes direct/indirect diffuse, emissive, and
optional AO through Cycles. Temporary images/nodes/settings are restored on
success, failure, or cancellation.

HDR results are initially RGBM RGBA8 and use the existing DXT5 DDS path. Pixels
remain external DDS assets. Static-prop scene records store only the lightmap
asset index, UV scale/offset, RGBM range, and flags. Realtime, baked, and mixed
light modes must avoid double contribution at runtime.

## Integration waves

### Wave 0 — Contracts and binary foundation (current)

- Flat entity base plus direct fixed subclasses and classname factory.
- Generational entity handles and slot lookup.
- `.cscene` packed definitions in `src/assets/`.
- Hardened low-level C++ reader.
- Transactional concrete-entity construction with typed asset resolution.
- Flat source-event to target-action records resolved through file-local indices.
- Deterministic Python description/writer and cross-language fixture.
- Compact typed asset database.
- Documentation and sample build kept current.

Gate: C++ build, CTest, Python suite, layout assertions, malformed-input tests,
and Python-to-C++ fixture all pass.

### Wave 1 — Transactional runtime loading

- Convert validated class records into concrete entities.
- Resolve `.casset`, mesh, material, texture, and prefab handles.
- Add active-scene open/replace/close lifecycle.
- Add explicit reference/event records and deferred structural changes.
- Preserve current active scene on every load failure.

Gate: a `.cscene` loads, unloads, reloads, and fails safely under malformed or
missing assets.

### Wave 2 — Blender scene exporter

- Convert selected Blender scene objects to fixed entity descriptions.
- Resolve Blender object links into final file-local indices and bake parent
  chains into transforms.
- Generate or refresh all missing/stale referenced assets, including `.casset`
  dependencies, then write `.cscene` last.
- Add useful validation and atomic output.
- Add UV1 generation and deterministic atlas planning.

Gate: Blender fixtures export byte-stable scenes accepted by the C++ loader, and
failed dependencies never replace a valid prior scene.

Implemented foundation: `SCENE_` collections now flatten `matrix_world` into
engine-space transforms, convert mesh/camera/light/empty/gameplay objects into
fixed records, generate referenced `.casset` files and normal mesh/material/
texture dependencies first, then publish `.cscene` atomically.

### Wave 3 — Rendering and baked lighting

- Extract props, cameras, and lights into current renderer structures.
- Reuse assets and automatically instance compatible placements.
- Add spatial candidates/frustum culling without author configuration.
- Bake Cycles lightmaps, export DDS, and sample UV1/RGBM at runtime.
- Define flattening versus retained `.casset` instance behavior.

Gate: a representative mesh map renders repeated assets, direct `.cmesh` props,
`.casset` placements, cameras, and realtime/baked/mixed lighting.

Implemented foundation: scene render activation loads each referenced material
once, reuses mesh/material pairs across placements, registers static/dynamic
props and non-baked lights, and owns their removal. The viewer accepts a
`.cscene` argument as a separate mode and tears the scene down before renderer
shutdown. The lightmap path is also complete: Blender preserves or generates
`CEngineLightmap` UV1, assigns deterministic padded atlas tiles, bakes static
placements through Cycles, encodes RGBM directly to BC3 DDS, and writes only the
texture reference plus atlas transform into `.cscene`. Runtime activation shares
each atlas, uploads UV1 as a separate vertex stream, decodes RGBM in deferred and
forward rendering, suppresses duplicate ambient light on baked placements, and
releases the textures with the scene.

### Wave 4 — Physics, gameplay, scripting boundary, and tools

- Add fixed physics fields/classes and safe Jolt activation/deactivation.
- Complete event/action dispatch and lifecycle behavior.
- Add player, door, pickup, and trigger examples as fixed entity types.
- Add `ceasset inspect` and `ceasset validate` for `.cscene`.
- Expose reflected fixed properties and safe handles for a future script VM,
  without choosing the VM yet.

Gate: physics and gameplay state reload cleanly, tools diagnose bad content, and
native examples prove the future scripting surface.

Implemented tooling foundation: `ceasset inspect` reports scene classes, asset
references, connections, and the active camera. `ceasset validate` additionally
checks packed ranges, concrete record strides and values, class membership,
typed asset indices, dependency existence/type/GUID, and connection integrity.

Implemented physics foundation: `prop_dynamic` owns explicit box half-extents
and mass fields. Scene physics activation creates Jolt bodies, copies simulated
transforms back to entities and renderables, and destroys every body on unload.

### Wave 5 — Evidence and measured optimization

- Build a representative Blender-to-runtime integration map.
- Test repeated assets, `.casset` references, dynamic/static props, explicit
  relationships/events, all light modes, collision, and multiple atlas regions.
- Benchmark creation, loading, lookup, extraction, spatial queries, and unload at
  1k/10k/100k entities.
- Optimize only measured hotspots and audit deterministic outputs/package files.

## First-release definition of done

- Blender exports deterministic mesh-centric `.cscene` maps.
- `.cscene` references `.casset` and other assets without embedding them.
- Missing/stale referenced assets are generated before atomic scene publication.
- Flat fixed entity classes, handles, lifecycle, explicit references, and
  events work without generic parenting or classname hashes.
- Loading is hardened and transactional.
- Props, cameras, lights, prefabs, physics, and baked lightmaps activate/unload.
- Repeated assets are grouped automatically and visibility requires no manual
  map optimization metadata.
- Viewer open/reload/replace/close and inspection/validation tools work.
- A future scripting VM can use safe handles, fixed properties, events, actions,
  and lifecycle without redesigning scene ownership.
- All C++, Python, Blender, integration, and compatibility tests pass.
