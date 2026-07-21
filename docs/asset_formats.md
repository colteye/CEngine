# CEngine Asset Format Decisions

> **Status: current implementation and format reference.** The active milestone
> and asset behavior are defined by `docs/arch/IMPLEMENTATION.md`. Entries for
> unimplemented asset families preserve design information only; they do not add
> those formats, streaming, packages, or runtime prefabs to the active scope.

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

## Current and Candidate Runtime Choices

“Current” describes an implemented format family. “Partial” preserves an
implemented boundary whose advanced fields or runtime behavior remain deferred.
“Candidate” records design information only and requires milestone promotion.

| Asset family | Runtime format | Status | Decision and reason |
| --- | --- | --- | --- |
| Textures | `.dds` | Current | Use Pillow in Python tools. DDS is already a direct GPU texture container for desktop block formats. A custom wrapper is justified only by real metadata or multiple target payloads. |
| Materials | `.cmat` | Current | Materials are fixed engine shader contracts rather than imported DCC graphs. |
| Mesh geometry | `.cmesh` | Current | Store load-ready geometry, bounds, submeshes, material slots, and supported skin streams without runtime interchange parsing. |
| Skeletons/rig metadata | `.cskel` | Current | Keep shared hierarchy, reference pose, bind data, and current metadata separate from meshes and clips. Advanced sockets, masks, IK, retargeting, and jiggle data are promoted later. |
| Animation/curves | `.canim` | Partial | Preserve independent clips and curves. Runtime compression, root motion, advanced events, morph/material tracks, and error settings remain deferred. |
| Physics/simulation | `.cphys` | Candidate | Add only when reusable cooked collision cannot be represented by the active scene/mesh path. The eventual asset remains engine-neutral rather than a serialized Jolt object. |
| Reusable assemblies | current `.casset` | Partial/migrate | The current implementation records reusable composition. The target cooker may flatten authored assemblies into ordinary scene records; `.casset` does not require a public runtime prefab system. |
| Scenes | `.cscene` | Current | Store scene composition and packed typed entity-class records while heavy payloads remain separate. Schema/layout migration follows `docs/arch/SYSTEMS.md#assets-and-scenes`. |
| Audio | `.opus`/`.ogg`, optional `.caudio` metadata | Candidate | Use standard payloads first. Add metadata only for required loop, attenuation, subtitle, dialogue, bank, or streaming contracts. |
| VFX | `.cvfx` | Candidate | Introduce only after a selected game effect demonstrates a stable engine-authored vocabulary. |
| Navigation | `.cnav` | Candidate | Introduce only after the navigation promotion criteria are met. |
| Shaders | backend-native blobs plus optional `.cshader` metadata | Candidate | Add a container only for required variants, reflection, layouts, or multi-backend payload identity. |
| Packages | `.cpak` | Candidate | Loose complete assets are the development and initial-release path. Packaging follows a measured deployment, locality, patching, or streaming requirement. |

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
Component rows store component kind ids and paths relative to the `.casset`
bundle, such as `meshes/SM_Body.cmesh`. Heavy mesh, material, skeleton,
animation, and texture data stays in those component target assets. A scene
loader resolves a `prefab_instance` once into ordinary entity records; the
runtime does not retain a mutable prefab hierarchy.

The current `.cmat` v3 payload is binary material data. It stores a fixed header,
texture binding rows, and a UTF-8 string table. The header carries base-color,
metalness, roughness, and ambient-occlusion constants. The shader is an explicit
id mapped to `MaterialShaderType`; the format is not hardcoded to PBR. Texture
rows store engine slot ids and exported `.dds` target paths.

The active PBR cooker reads only the inputs used by the current shader: base
color, normal, metalness, roughness, and ambient occlusion. Base color and normal
are optional slots. If any metalness, roughness, or AO image is connected, the
cooker builds one packed slot-3 MRA DDS with metalness in red, roughness in green,
and ambient occlusion in blue. Missing packed channels are white and are
multiplied by their stored constants. With no authored M/R/AO image, no MRA DDS
is emitted and all three values come from constants. A grayscale image that
reaches Principled Normal through a Blender Bump or Normal Map node is treated
as height data and converted to a tangent-space normal DDS at cook time. A
non-grayscale Normal Map image is used directly. Specular, transmission,
emission, alpha, and arbitrary node
graphs are outside this basic shader contract and are not serialized. Runtime
fallbacks are neutral white and flat normal, never a generic checkerboard. The
runtime intentionally rejects older `.cmat` payload versions.

Blender light energy is serialized directly. The PBR shader consumes that value
as scene-linear intensity; there is no legacy renderer-side brightness
multiplier. Non-directional lights retain inverse-square distance attenuation
and the authored cutoff range.

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
accepts only the current v2 layouts: 40-byte static vertices and 56-byte skinned
vertices. Older pre-UV1 layouts are intentionally rejected rather than migrated
at runtime.

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

## `.cscene` v4 Contract

`.cscene` is a complete world; `.casset` remains a reusable prefab hierarchy.
Runtime code never interprets Blender object types. A scene uses the common
`DiskAssetHeader` with `AssetType::Scene`, followed by a little-endian `CSCN`
payload. Fixed structures are packed to one-byte alignment. All 64-bit offsets
are relative to the start of the scene payload. Writers align non-empty tables
and class payloads to 16 bytes; readers validate bounds and integer overflow
and do not rely on that alignment.

The payload order is: header, settings, asset references, entity declarations,
class-block descriptors, per-class entity-index and fixed-record arrays,
per-class auxiliary arrays, connections, and a UTF-8 string table. Strings are
offset/size byte ranges without a terminator. Every entity declaration belongs
to exactly one class block.

The current class records are `empty`, `prop`, `camera`, `light`,
`prefab_instance`, `trigger`, and `info_player_start`. `prop` is one class for
both static and dynamic objects. Its fixed record stores transform, mesh and
material references, optional lightmap binding, flags, collision half-extents,
and mass. Prop flag bit `1` is visible, bit `2` enables collision, and bit `4`
makes the prop dynamic; an unset dynamic bit means static. Only static props may
carry baked lightmaps.

A `prefab_instance` record owns a range into its class auxiliary lightmap table.
Each row identifies a deterministically sorted `.casset` object index and stores
the scene lightmap texture reference, UV scale/offset, and RGBM range. The scene
loader copies that binding to every material-split `prop` realized from the
object. Lighting is therefore placement-owned; reusable `.casset` files retain
UV1 but never contain a scene bake.

Blender/Cycles bakes two albedo-independent diffuse passes into the lightmap:
indirect illumination from `baked` and `mixed` lights, plus direct illumination
from fully `baked` lights. Mixed lights remain active for realtime direct light
and shadows; baked lights are omitted from the runtime light list. The shader
multiplies decoded lightmap irradiance by material albedo before adding realtime
direct lighting.

Entity flag bit `1` is enabled. Class-block flag bit `1` marks a required block.
The current runtime rejects unknown classnames and unsupported class record
versions. References use zero-based indices and `0xffffffff` for no reference.
UUIDs/GUIDs are raw 16-byte values. Deterministic writers sort normalized asset
references and class blocks, sort connections by their stable fields, use
deterministic entity order supplied by the normalized scene, and zero alignment
padding.

The final word of a `light` record is a flag mask: bit `1` enables the light and
bit `2` makes realtime and mixed lights cast shadows. The Blender exporter maps
the light datablock's shadow setting to the cast-shadow bit. A shadow-casting Sun
is rendered through the directional-light cascade path.

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
