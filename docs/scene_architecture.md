# CEngine Scene and Entity Architecture

CEngine scenes are flat, mesh-centric maps. They contain entities, scene settings,
and references to separate runtime assets. They do not contain BSP geometry,
generic hierarchy data, or a runtime-attached data framework.

## Runtime model

`Scene` owns one slot array of `std::unique_ptr<Entity>`. A slot generation forms
the transient `EntityId`. Destroying an entity advances its slot generation, so
stale runtime handles fail cleanly. Serialized relationships use file-local
entity indices that become runtime handles when loaded. Classnames are canonical
strings; there is no persistent UUID, classname hash, or parallel numeric class
ID.

`Entity` directly owns the fields shared by every entity: runtime ID, name,
flags, and transform. It is a small base class with modern lifecycle hooks. Each
concrete entity is a direct, `final` subclass with fixed C++ fields and its own
header and implementation under `src/entity/`:

| Classname | C++ type | Main data |
| --- | --- | --- |
| `empty` | `EmptyEntity` | transform |
| `prop_static` | `StaticProp` | mesh, material overrides, lightmap binding |
| `prop_dynamic` | `DynamicProp` | mesh, material overrides, runtime flags |
| `light` | `LightEntity` | light type, mode, color, intensity, range |
| `camera` | `CameraEntity` | projection and lens/clip settings |
| `trigger` | `TriggerEntity` | bounds and trigger flags |
| `prefab_instance` | `PrefabEntity` | prefab reference |
| `info_player_start` | `PlayerStart` | team |

A deliberately small factory maps canonical classnames to concrete objects.
`Scene::CreateEntity` owns slot allocation and identity assignment. Adding a class
means adding its `.h/.cpp` and one explicit factory entry; it does not require a
registry, schema graph, hash table, or family of `Scene::CreateX` methods.

There is no generic `Update` requirement. Static entities cost no per-frame
dispatch. Bulk render, physics, animation, and audio work stays in explicit
systems. Only entities that need gameplay behavior opt into lifecycle/event
hooks. CPU job scheduling is deferred until profiling demonstrates a need.

## Lifecycle and relationships

The intended loaded-scene order is:

```text
construct concrete entity
deserialize its fixed record
OnLoaded
resolve every asset and explicit entity reference
OnSceneReady
dispatch events while active
OnStop before removal or scene shutdown
```

Runtime spawning may add a separate initialization hook when it is implemented;
it must not overwrite deserialized values. Structural changes made during event
dispatch are deferred to a safe point.

Parenting is not first class. Blender parent transforms are baked into world
transforms at export. A gameplay dependency uses an explicit semantic reference,
such as owner, follow target, attachment, or door target. The relevant entity
defines cycle, update-order, and destruction behavior.

## `.cscene`

`.cscene` is composition data only. Reusable geometry, materials, textures,
skeletons, animations, collision, and prefabs stay in their own assets. A placed
reusable asset is represented by a `.casset` reference in the scene; its mesh and
material contents are not copied into `.cscene`.

```text
DiskSceneHeader
DiskSceneSettings
DiskAssetReference[]
DiskSceneEntity[]
DiskEntityClassBlock[]
class entity-index arrays
fixed records for each concrete class
class-specific auxiliary arrays
future explicit event/reference records
UTF-8 string table
```

An entity row stores classname string range, name string range, and flags.
A class block stores its classname, record version, entity indices, fixed record
stride, and optional auxiliary data. Class blocks let the loader process equal
concrete types together while keeping the runtime model as fixed entity classes.

The writer preserves the exporter’s final entity order, sorts asset references by
normalized path and class blocks by classname, zeroes padding, and produces
byte-identical output for identical input. The reader validates all ranges,
strides, indices, versions, and
string ranges with overflow-safe arithmetic before exposing a view.

Loading is transactional: parse into temporary state, construct concrete
entities through the factory, deserialize class records, resolve typed asset
handles and file-local entity references, run lifecycle validation, then swap
the completed scene into place. A failure leaves the active scene unchanged.

## Assets and export

A `.cscene` contains one deduplicated table of GUID, expected type, and normalized
project-relative path. Entity records store compact indices into that table.
Meshes keep geometry, bounds, UVs, submeshes, and LODs in `.cmesh`; materials stay
in `.cmat`; lightmap pixels stay in DDS. Placement-only overrides and lightmap UV
scale/offset belong in the entity record.

Blender export maps mesh objects to `prop_static` by default, movable meshes to
`prop_dynamic`, and cameras/lights/empties to their matching classnames. Before
publishing the scene, it walks all dependencies. If a referenced runtime mesh,
material, texture, skeleton, animation, collision asset, or prefab is missing or
stale but its source data is available, the exporter generates it through the
normal asset pipeline. It writes `.cscene` atomically only after every dependency
exists and validates. The scene never embeds those generated payloads.

Collections are authoring organization, not runtime parents. Reusable collection
assets remain `.casset`; retained instances use `prefab_instance`, while a future
compiler may flatten instances whose identity is unnecessary.

## Rendering, optimization, lightmaps, and physics

Scene activation turns prop entities into render instances using shared asset
handles. Static extraction can be cached and compatible placements can be
instanced automatically. World bounds come from `.cmesh` bounds plus transforms.
The compiler/runtime builds spatial candidates and performs frustum culling;
authors do not create draw batches, visibility cells, or manual merge groups.

The Blender tool selects or generates UV1 without changing UV0, plans atlases,
bakes with Cycles, restores Blender state, encodes HDR lightmaps as RGBM DDS, and
stores only texture references plus UV transforms in static-prop records. Baked,
realtime, and mixed lights must avoid double contribution.

Physics is added as fixed data on the entity classes that need it, or as explicit
new entity classes. Scene activation creates Jolt
bodies after validation and removes them before entity storage is released.

## Future scripting boundary

The first release does not choose a VM. Future scripts operate on safe entity
handles, fixed reflected properties, typed events/actions, and explicit entity
references. They never receive ownership of native entities or pointers whose
lifetime can silently end. New scriptable entity classes should preserve the
same flat public model even if their property storage is implemented separately.
