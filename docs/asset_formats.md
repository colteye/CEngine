# CEngine Asset Formats

> **Status: current version-one format reference.** The architecture boundary is
> defined by [`arch/IMPLEMENTATION.md`](arch/IMPLEMENTATION.md).

## One data path

The engine reads cooked target assets only. Blender and Python tools turn source
content into runtime files:

```text
Blender source -> Blender add-on -> .casset/.cscene and component assets
images         -> Python tools   -> .dds
audio          -> Python tools   -> .wav/.flac/.mp3/.ogg
```

Every engine-owned binary payload is declared in
[`schemas/engine.game.json`](../schemas/engine.game.json). The schema generator
emits:

- owned C++ `Wire::*` values;
- complete bounded C++ readers using `Assets::Reader`;
- the flattened JSON consumed by the Python exporter.

Python has one generic schema packer/unpacker. There are no handwritten payload
headers, table records, disk mirrors, compatibility readers, or format-specific
primitive decoders.

Handwritten `*_asset.cpp` code is limited to semantic validation and conversion
from generated data into a runtime object. DDS and standard audio files are
external formats and keep their own parsers.

## Common envelope

All `.c*` files use common envelope version `1`:

```text
magic "CEAF"
u16 version = 1
u16 header size
u32 asset type
16-byte GUID
u64 source hash
16-byte platform name
u64 payload offset
u64 payload size
u64 file size
```

The payload is exactly one generated schema record. Multi-version runtime
compatibility is intentionally absent: format changes require recooking.

A reference is always the complete value:

```text
Reference {
  guid
  type
  normalized project-relative path
}
```

`Store` validates references and shares immutable decoded assets through
`shared_ptr<const T>`.

## Implemented families

| Family | Runtime file | Schema record / behavior |
| --- | --- | --- |
| Texture | `.dds` | Standard DDS, loaded directly |
| Material | `.cmat` | name, shader/render mode, texture references, PBR constants |
| Mesh | `.cmesh` | bounds, feature flags, 1–8 ordered LODs, vertices, indices, UV0/UV1, joints, weights |
| Skeleton | `.cskel` | armature name and ordered bone hierarchy |
| Animation | `.canim` | skeleton reference, clip range/FPS, tracks and keys |
| Physics | `.cphys` | backend-neutral primitive, hull, triangle, heightfield, and compound nodes |
| Composition | `.casset` | reusable named object hierarchy and component references |
| Scene | `.cscene` | whole-map settings, references, entity rows, class records, connections |
| Particle | `.cparticle` | emission, lifetime, speed/spread, gravity, size/color curves, texture, blend, looping |
| Audio | `.wav`/`.flac`/`.mp3`/`.ogg` | Standard PCM or compressed stream, loaded directly |

Deferred navigation, shader-package, and deployment-package formats are not
implemented.

## Meshes and LODs

Each mesh contains a descending list of screen-size thresholds. LOD0 has
threshold `1.0`; `Mesh::Lod(screen_size)` selects the first satisfied threshold
and falls back to the last LOD.

The Blender exporter recognizes child mesh objects named `LOD1_*` through
`LOD7_*`. Their optional `ce_lod_screen_size` value defaults to `0.5`,
`0.25`, and so on. Thresholds must strictly descend, and every LOD must use the
same skin/lightmap feature set as LOD0.

Vertices contain position, normal, material UV, lightmap UV, four joint
indices, and four normalized weights. Unused skin data is zero. Indices are
`uint32` triangles. The exporter triangulates polygons and deduplicates complete
vertex values.

## Materials and textures

Materials use explicit shader and render-mode ids. The current PBR contract
supports base color, normal, and packed metalness/roughness/AO texture slots
plus their scalar factors and alpha cutoff.

Blender converts source images into DDS. If any metalness, roughness, or AO
image exists, the cooker builds one packed MRA texture. Missing packed channels
are white. Grayscale height input connected through a Bump/Normal Map node is
converted to a tangent-space normal DDS.

Lightmaps and sky panoramas are ordinary texture references. The renderer
uploads decoded texture values and performs no game-asset file I/O.

## Skeletons and animation

Skeleton bones are ordered parent-before-child and carry a row-major
`armature_from_bone` matrix. Animation clips reference their skeleton and store
ordered scalar key tracks with interpolation ids. Mesh skin streams use the
same bone order.

Compression, root motion, retargeting, animation events, and morph tracks remain
future schema additions.

## Physics

Physics assets store a flat parent-before-child node list. Runtime construction
turns that generated data into a `PhysicsShape` tree and validates
shape-specific requirements. Supported nodes are box, sphere, capsule,
cylinder, plane, convex hull, triangle mesh, heightfield, and compound.

The format is backend-neutral and never serializes Jolt objects.

## Compositions and scenes

`.casset` is a reusable collection such as a character or prop. It contains
named object hierarchy rows, transforms, and complete component references.
Heavy payloads remain separate assets.

`.cscene` is a complete map. Its generated record contains:

- scene settings;
- complete asset references;
- entity declarations;
- per-class entity indices and generated property bytes;
- auxiliary asset indices;
- entity event/action connections.

Entity property layouts are generated from the same game schema. All current
entity class versions are `1`. Runtime scene code only resolves dependencies
and instantiates entities from the decoded values.

## Particles and audio

The Blender add-on exports a simplified standard particle vocabulary from
particle systems or `ce_particle` objects. Optional `ce_particle_*` properties
override texture, emission rate, lifetime, speed, spread, gravity, start/end
size, start/end color, blend mode, loop, and world-space behavior.

Audio stays in WAV, FLAC, MP3, or Ogg/Vorbis rather than acquiring an engine
container before loop, attenuation, subtitle, dialogue, or streaming metadata
is actually required. The runtime can either cache decoded PCM for repeated
effects or stream-decode the retained compressed bytes for music and ambience.

## Sponza

The checked-in Sponza scene, materials, and meshes are cooked in the current
generated version-one formats. Its 4096-square mipmapped RGBExp32 lightmap at
`assets/compiled/sponza/lightmaps/Sponza_0.dds` combines separate
visibility-aware direct and indirect Cycles passes.
Because the original source scene is not checked in,
`tools/ceasset/cook_sponza_collision.py` builds one static collision-only asset
from the structural LOD-zero meshes, binds it through a `collider` entity, and
places the viewer spawn origin on the floor. The viewer entity applies its
camera eye-height offset at runtime.
When the external source scene is available,
`tools/ceasset/author_sponza_collision.py` creates the matching aggressively
decimated, collision-only Blender object while preserving the floor polygons.

## Reference formats

- [glTF 2.0](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html)
- [DDS programming guide](https://learn.microsoft.com/en-us/windows/win32/direct3ddds/dx-graphics-dds-pguide)
- [Ogg](https://www.xiph.org/ogg/)
