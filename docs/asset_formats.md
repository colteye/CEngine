# CEngine Asset Format Decisions

This document records the current asset boundary decisions. The engine runtime
should load only target assets. Standard source formats such as
`.blend`, `.psd`, `.png`, `.tga`, and `.fbx` are tool inputs and should be
preprocessed by Python tooling before the engine sees them. `.blend` files are
handled only by the Blender add-on, not by `ceasset`.

## Format Rule

Use an OSS standard format when it is already the best runtime payload. Use a
custom `.c*` format only when CEngine needs engine-specific layout, composition
records, streaming/package tables, or data that can be loaded with fewer runtime
allocations and less translation.

The desired user-facing pipeline is direct:

```text
.blend via Blender add-on, .png/.tga/.psd/etc via Python tools -> target runtime assets
```

Do not emit intermediate/debug assets as part of normal conversion. If a source
format needs more implementation work, add a tested Python conversion path for
that format before accepting it. Debugging should happen through tests,
validation, and explicit assertions rather than hidden generated files.

External OSS tools are acceptable conversion backends. They should be invoked
explicitly by Python with argument lists, not through shell scripts or hidden
generated state. Machine-local tool paths and options stay command-line inputs;
the compact registry stores source path, target path, GUID, asset type, and source
hash.

## Final Runtime Choices

| Asset family | Runtime format | Decision | Reason |
| --- | --- | --- | --- |
| Textures | `.dds` | Use Pillow in Python tools | DDS is already a direct GPU texture container for desktop block formats. A custom texture wrapper is only justified later if it stores real engine metadata or multiple platform payloads, not if it wraps DDS bytes. |
| Materials | `.cmat` | Keep | Materials are engine shader contracts: render mode, shader family, texture bindings, sampler state, scalar/vector params, alpha cutoff, shadow flags, and feature bits. glTF/Blender material graphs are source data; runtime wants fixed engine layouts. |
| Mesh geometry | `.cmesh` | Keep | Runtime mesh data should be GPU-ready: vertex streams, packed formats, index buffers, submeshes, LODs, bounds, material slots, skin streams, and morph deltas. glTF is useful reference material for semantics, but the runtime should avoid glTF parsing and accessor translation. |
| Skeletons/rig metadata | `.cskel` | Keep | Rigs are shared across meshes and animations. Store bone hierarchy, inverse bind pose, bone names or stable hashes, sockets, masks, IK metadata, retarget data, flex names, and jiggle-chain references separately from mesh and animation bulk data. |
| Animation/curves | `.canim` | Keep | Animation should stream and share independently. Store compressed tracks, curve targets, root motion, animation events, morph weights, material curves, camera/light tracks, and per-track error settings in runtime form. |
| Physics/simulation | `.cphys` | Keep | Physics data should be engine-neutral at the asset layer: primitives, convex hulls, static triangle meshes, ragdoll bodies, constraints, hitboxes, triggers, cloth setup, and jiggle/spring settings. Backend-specific optimized caches can be added later but should not be the only source. |
| Prefabs | `.casset` | Keep | Reusable entity hierarchies reference separate mesh, material, skeleton, animation, physics, audio, and VFX assets. |
| Scenes | `.cscene` | Keep | Complete Blender-authored scenes contain stable classnamed entities and type-grouped, versioned field blocks. Heavy asset data remains separate. |
| Audio | `.opus`/`.ogg` payloads plus optional `.caudio` metadata | Defer `.caudio` until needed | Opus/Vorbis in Ogg are OSS, mature runtime payloads. A `.caudio` wrapper is only worth it for loop points, attenuation, subtitles, dialogue IDs, banks, streaming chunks, or target-platform variants. Do not wrap audio just to rename it. |
| VFX | `.cvfx` | Keep | VFX is engine-authored behavior: emitter graphs, spawn modules, curves, material bindings, mesh particle refs, flipbook refs, trails, beams, and simulation settings. No standard runtime format covers the engine contract cleanly. |
| Navigation | `.cnav` | Keep | Navigation data is engine/game-specific: nav meshes, tiles, off-mesh links, cover points, costs, AI zones, and streaming chunks. Keep it custom and load-ready. |
| Shaders | backend-native blobs plus `.cshader` metadata | Keep `.cshader` as metadata/container | SPIR-V/DXIL/MSL/GLSL are the payloads. `.cshader` should store variant keys, reflection metadata, parameter layouts, backend payload refs, and compatibility/versioning. |
| Packages | `.cpak` | Keep | Shipping should bundle converted assets for locality, compression, dependency preloading, async streaming, residency, and patching. Loose files are a development mode. |

## Character Import Path

The intended character path is:

```text
Blender .blend
  -> CEngine Blender add-on
  -> .cmesh + .cskel + .canim + .cmat + .cphys + textures/audio/VFX
  -> .casset root asset
```

The engine should not import `.blend`, `.glb`, `.gltf`, `.fbx`, `.png`, `.psd`,
or similar authoring/interchange formats at runtime. It should load the target
runtime outputs only.

A character is not a separate asset family. It is a prefab with a rich component
set: mesh components, skeletal/animation components, material bindings, physics
bodies, sockets, VFX emitters, audio emitters, script/gameplay data, and other
component records as needed.

The first Blender add-on export contract is selected-root-collection based:

- `PREFAB_<name>` collections export hierarchy/component target assets as
  `.casset`.
- `SCENE_<name>` collections export complete scenes as `.cscene`.
- Collection custom properties may use `ce_asset_type = prefab|scene` and
  `ce_asset_name = <name>` when naming prefixes are inconvenient.

The add-on exports only the active top-level collection selected for export. It
walks nested child collections through that root and ignores orphan datablocks,
unrelated collections, and image datablocks not referenced by selected-object
materials.

This path emits target files directly under `assets/compiled/<blend-name>/` and
updates the compact asset registry/cache. It does not write manifests,
intermediate assets, debug files, or command-line `.blend` conversion reports.
The add-on currently writes `.casset` hierarchy/composition payloads, `.cmesh`
triangle geometry buffers with optional skinning streams, `.cmat` material
payloads, `.cskel` skeleton metadata, `.canim` action F-curves, and `.dds`
textures discovered from Blender image datablocks. Runtime pose-track
compression and sampling-friendly animation buffers are future explicit
additions.

The common `.c*` file header is intentionally small: magic, version, asset type,
GUID, source hash, platform target, payload offset, payload size, and file size.
There is exactly one typed binary payload. There is no common dependency table,
payload table, JSON payload, sidecar manifest, or debug output.

The initial `.casset` payload is binary composition data. It stores a fixed
header, sorted object rows, component rows, and a UTF-8 string table. Object rows
contain object name, role id, Blender object type id, parent name, first
component row, component count, and a row-major `world_from_local` matrix.
Component rows store component kind ids and project-relative target paths such
as `assets/compiled/hero/meshes/SM_Body.cmesh`. Heavy mesh, material, skeleton,
animation, and texture data stays in those component target assets.

The initial `.cmat` payload is binary material data. It stores a fixed header,
texture binding rows, and a UTF-8 string table. The shader is an explicit id
mapped to `MaterialShaderType`; the format is not hardcoded to PBR. Texture rows
store engine slot ids and exported `.dds` target paths.

The initial `.cskel` payload contains a fixed skeleton header, fixed bone rows,
and a UTF-8 string table for armature and bone names. Bone rows contain parent
index and the row-major Blender `matrix_local` value as `armature_from_bone`.
`SkeletonAsset` validates the binary payload and exposes bone rows and names
without parsing text.

The initial `.cmesh` payload contains a fixed mesh header followed by geometry
bytes. Metadata records vertex/index counts, bounds, vertex stride, index size,
skinned/lightmap-UV flags, material slot count, and geometry offset. Static
vertices are tightly packed as `float3 position, float3 normal, float2 uv0,
float2 uv1`. Skinned vertices append `uint16x4 bone_indices` and `unorm16x4
bone_weights`, normalized to 65535. UV1 is the normalized per-mesh lightmap
unwrap; placement-specific atlas scale/offset stays in `.cscene`. All mesh blobs
end with `uint32` triangle indices. The exporter fans
Blender polygon loop data into triangles at write time. `LoadMeshAsset`
validates the payload and fills the renderer `Mesh` using explicit material
slots supplied by the caller. Morph deltas, LOD tables, animation tracks, and
optimized vertex/index reordering are future explicit additions. The loader
still accepts legacy 32-byte static and 48-byte skinned vertices without UV1;
such meshes cannot be assigned a baked lightmap.

The initial `.canim` payload is binary animation data. It stores a fixed header,
track rows, keyframe rows, and a UTF-8 string table. Track rows contain Blender
`data_path`, `array_index`, first keyframe, and keyframe count. Keyframe rows
store frame, value, and interpolation id. Runtime track compression and
sampling-friendly animation buffers are future explicit additions.

## OSS Dependency Bias

Preferred dependencies and formats:

- Blender Python API for direct artist export tooling.
- glTF 2.0 as a reference model for nodes, materials, skins, morphs, and
  animation semantics.
- meshoptimizer/gltfpack-style algorithms for vertex cache optimization,
  overdraw optimization, quantization, simplification, and animation resampling.
- DDS for desktop GPU-ready texture payloads. The current Pillow path writes
  DXT1/DXT3/DXT5/BC5.
- Opus or Ogg/Vorbis for open audio payloads.
- Jolt as the physics backend, with `.cphys` remaining backend-neutral first.

Avoid dependencies whose runtime behavior depends on unavailable proprietary
source, opaque build steps, or undocumented binary formats.

## `.cscene` v1 Contract

`.cscene` is a complete world; `.casset` remains a reusable prefab hierarchy.
Runtime code never interprets Blender object types. A scene uses the common
`DiskAssetHeader` with `AssetType::Scene`, followed by a little-endian `CSCN`
payload. Fixed structures are packed to one-byte alignment. All 64-bit offsets
are relative to the start of the scene payload. Writers align non-empty tables
and component payloads to 16 bytes; readers validate bounds and integer overflow
and do not rely on that alignment.

The payload order is: header, settings, asset references, entities, component
block descriptors, per-block entity-index and typed-payload arrays, auxiliary
arrays, and a UTF-8 string table. Strings are offset/size byte ranges without a
terminator. The mesh-renderer material-index list is a variable auxiliary array.

| ID | Component | Version | Requirement |
| ---: | --- | ---: | --- |
| 1 | Transform | 1 | Required exactly once per entity; no generic parent relationship |
| 2 | MeshRenderer | 1 | Optional |
| 3 | Camera | 1 | Optional |
| 4 | Light | 1 | Optional |
| 5 | PrefabInstance | 1 | Optional |
| 6 | LightmapBinding | 1 | Optional |

Entity flags are enabled (`1`) and static (`2`). Component flags are required
(`1`) and enabled (`2`). Unknown required blocks fail loading; unknown optional
blocks are skipped. References use zero-based indices and `0xffffffff` for no
reference. UUIDs/GUIDs are raw 16-byte values. Deterministic writers sort
entities by UUID, group blocks by component ID, deduplicate and sort normalized
project-relative asset paths, and zero all padding.

## Sources

- Khronos glTF describes scenes, nodes, meshes, buffers, materials, textures,
  skins, and animations, which are useful reference semantics:
  https://www.khronos.org/gltf/
- glTF 2.0 specifies skins, joint hierarchies, inverse bind matrices, and PBR
  materials: https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html
- meshoptimizer/gltfpack documents mesh optimization, quantization, and animation
  resampling options:
  https://github.com/zeux/meshoptimizer/blob/master/gltf/README.md
- Microsoft documents DDS as a binary container for compressed/uncompressed
  texture data and mip/cube/array payloads:
  https://learn.microsoft.com/en-us/windows/win32/direct3ddds/dx-graphics-dds-pguide
- Pillow documents DDS read/write support:
  https://pillow.readthedocs.io/en/stable/handbook/image-file-formats.html#dds
- Opus is open, royalty-free, and suitable for storage/streaming:
  https://www.opus-codec.org/
- Ogg is an open stream-oriented multimedia container:
  https://www.xiph.org/ogg/
