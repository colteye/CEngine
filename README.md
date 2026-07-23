# CEngine

Blender is the primary source-asset editor. See
[Blender asset authoring](docs/blender_addon.md) for the add-on workflow,
schema-driven entities, previews, and decoupled lightmap baking.

CEngine is now built with CMake instead of Visual Studio project files.

## Repository layout

- `src/` contains engine-owned runtime code.
- `samples/viewer/` contains the sample viewer executable.
- `third_party/` contains vendored source/header dependencies.
- `assets/engine/` contains engine runtime assets such as shaders and utility textures.
- `assets/demo/` contains sample runtime assets.
- `assets/source/` contains source/import-time art files that are not loaded directly by the viewer.
- `cmake/` contains project CMake helper modules.


## Build

On macOS, install the Xcode Command Line Tools, then build natively for the
current Mac (Apple Silicon or Intel):

```sh
xcode-select --install # only needed once
cmake --preset mac-debug
cmake --build --preset mac-debug
./build/mac-debug/CEngine
```

Use `mac-release` in place of `mac-debug` for an optimized build. CMake links
the system OpenGL framework and builds GLFW's Cocoa backend automatically; a
separate Vulkan SDK is not required.

From a Windows command prompt:

```bat
cmake --preset windows-debug
cmake --build --preset windows-debug
```

Windows release builds use:

```bat
cmake --preset windows-release
cmake --build --preset windows-release
```

The cross-platform presets are:

```sh
cmake --preset debug
cmake --build --preset debug
```

Release builds use:

```sh
cmake --preset release
cmake --build --preset release
```

Build products live under `build/`, which is ignored by git.

## Dependencies

- CMake 3.22 or newer
- A C++17 compiler
- OpenGL
- GLFW 3

By default, CMake downloads and builds an immutable revision of GLFW 3.4 with
`FetchContent`, so Visual Studio project files or prebuilt GLFW libraries are not
required. To use an installed GLFW package instead, configure with
`-DCENGINE_USE_SYSTEM_GLFW=ON`.

On Linux, fetched GLFW still needs the development packages for the selected
desktop backend. The default fetched backend is X11; use
`-DCENGINE_FETCH_GLFW_WAYLAND=ON -DCENGINE_FETCH_GLFW_X11=OFF` for Wayland, or
use the `debug-headless` preset for a compile/link check on minimal images.

The viewer opens as a maximized, bordered window and keeps its camera projection
and render targets matched to the framebuffer at any window resolution. With no arguments, it loads
the checked-in Sponza sample at
`assets/compiled/sponza/Sponza.cscene`. An alternate cooked `.cscene` path may
be supplied as the first argument. Target-asset references resolve against the
project containing the scene's `assets/` directory, while renderer shaders are
loaded beside the executable. There is one graphics backend per build; OpenGL
is enabled by the checked-in presets.

## Architecture and asset pipeline

Implementation work starts at
[`docs/arch/STATUS.md`](docs/arch/STATUS.md), followed by
[`docs/arch/IMPLEMENTATION.md`](docs/arch/IMPLEMENTATION.md). They identify the
next vertical step, small active architecture, milestone gates, stable extension
seams, and routing for agents. The remaining four consolidated references cover
[delivery and performance](docs/arch/DELIVERY.md),
[core runtime design](docs/arch/CORE.md),
[runtime systems](docs/arch/SYSTEMS.md), and
[networking](docs/arch/NETWORK.md).

The engine runtime should load target assets only. Source formats from tools such
as Blender and Photoshop are converted by Python tooling before they become
runtime files.

```sh
python3 tools/ceasset/ceasset.py inspect assets/compiled/sponza/Sponza.cscene
```

`ceasset` keeps a compact binary GUID registry and build cache in
`assets/.ceasset/`, hashes source files, skips unchanged builds, and writes target
outputs under `assets/compiled/`. Texture runtime payloads are `.dds`; the
pipeline deliberately does not wrap PNG/TGA/DDS bytes in a custom texture file.
Source inputs such as `.png`, `.tga`, and `.psd` must get explicit Python
conversion paths with tests before the tool accepts them. PNG/TGA texture
conversion uses Pillow to write DDS output directly.

The converter internals live under `tools/ceasset/ceassetlib/` so each path can
stay small: binary state, path handling, texture conversion, and command
orchestration are separate modules.

Blender source processing lives only in the Blender add-on at
`tools/blender_addon/cengine_asset_exporter/`. The add-on reads the open
`.blend` through `bpy`, converts tracked PNG/JPEG/TGA image datablocks to `.dds`,
exports material datablocks used by the selected collection to `.cmat`, exports
selected armature bone hierarchies to `.cskel`, exports selected armature actions
to `.canim`, triangulates selected mesh polygons into `.cmesh` triangle buffers
at export time, and writes one tagged collection target asset directly under
`assets/compiled/<blend-name>/`. Orphan datablocks and unrelated scene
collections are ignored.

PBR material export is deliberately limited to the inputs used by the current
shader: base color, normal, metalness, roughness, and ambient occlusion. Each
input may be a supported image or a constant. If any metalness, roughness, or AO
image is connected, the add-on packs the authored images into one MRA DDS (R/G/B)
and keeps constants for the missing channels. Constant-only materials do not get
an invented MRA texture. A grayscale image reaching Principled Normal through a
Blender Bump or Normal Map node is treated as height data, cooked into a
tangent-space normal DDS, and bound to the normal slot.
Unconnected images and unsupported Principled inputs are not exported. Runtime
fallbacks remain neutral white and flat normal, never a checkerboard.

- `PREFAB_Hero` writes `Hero.casset`.
- `SCENE_TestRoom` writes `TestRoom.cscene`.
- Collection custom properties can override names with
  `ce_asset_type = prefab|scene` and `ce_asset_name = desired_name`.

Select the top-level collection you want to export before running
`File > Export > CEngine Assets`. `PREFAB_` collections produce reusable
`.casset` roots. `SCENE_` collections keep collection instances as
`prefab_instance` references, generate their `.casset` files, and publish the
`.cscene` only after those dependencies exist. Scene loading realizes each
`.casset` mesh/material pair once as an ordinary `prop`; it does not create a
general mutable runtime prefab graph.

Scene exports also build baked lighting automatically. Static meshes receive a
fresh generated `CEngineLightmap` UV1 on every cook, placements are packed into
a deterministic padded atlas, and Cycles writes scene-linear
irradiance to RGBM BC3 DDS. The bake adds indirect light from `baked` and `mixed`
lights to direct light from `baked` lights; realtime direct lighting and mixed
direct lighting remain runtime work. Surface albedo is not stored in the
lightmap and is multiplied by the irradiance in the shader. `.cmesh` owns UV1;
`.cscene` stores the atlas DDS reference, scale/offset, and RGBM range for each
static placement. Collection-instance bindings are stored per casset object on
the `prefab_instance` and copied to every material-split prop during expansion;
the reusable `.casset` contains no scene lighting. Optional scene custom
properties `ce_lightmap_resolution`, `ce_lightmap_padding`, and
`ce_lightmap_rgbm_range` override the 1024, 4-pixel, and 8.0 defaults. Generated
UV1 uses connected angle-based charts, equalized texel density, and
resolution-aware padding without changing the authored material UV0. Blender's
OpenImageDenoise HDR pass is
enabled by default after the Cycles passes are combined; set the scene property
`ce_lightmap_denoise = false` only when a raw bake is explicitly required.
Lightmap cooks use 64 Cycles samples, an indirect firefly clamp of 2.0, and
disable reflective/refractive caustics independently of interactive render
settings. Scene properties `ce_lightmap_samples` and
`ce_lightmap_indirect_clamp` override those quality defaults.

Every exported mesh placement is a `prop` entity. The `ce_dynamic` object
property selects dynamic behavior; when absent or false the prop is static.
`ce_collision` enables its collision binding independently of visibility.
Scene exports also preserve the Blender World background as environment light
and import each supported light's complete world transform. Sun direction is
the transformed local negative-Z axis, matching Blender's light convention.
Blender light energy is passed to the PBR shader without a hidden renderer
multiplier, so a Sun energy of `1.0` remains scene-linear intensity `1.0`.
Blender's per-light shadow setting is cooked into `.cscene`; a shadow-casting
Sun drives the renderer's cascaded directional shadows.

The current Blender phase exports hierarchy/placement payloads, static and
skinned mesh geometry buffers, material payloads, skeleton metadata, animation
F-curve payloads, and textures.
Each exported object record contains its Blender name, Blender object type,
CEngine role, parent name when the parent is inside the exported collection, and
a row-major `world_from_local` transform. `.casset` object records also include
generated asset paths such as `.cmesh`, `.cmat`, `.cskel`, and
`.canim`. These references are project-relative target paths, for example
`assets/compiled/hero/meshes/SM_Body.cmesh`, so exported assets remain movable
with the project.

All `.c*` target assets use a tiny common binary header and one typed binary
payload. There is no common dependency table, payload table, JSON payload, or
runtime manifest. `.cmesh` stores its own mesh header plus geometry bytes;
`.cskel`, `.cmat`, `.canim`, and `.casset` each use fixed binary rows plus UTF-8
string tables where names or paths are needed. DDS texture files stay plain DDS.

Runtime-side asset support lives in `src/assets/`. `AssetFile` reads the common
asset header and exposes the single payload. `LoadMeshAsset` builds the
renderer's in-memory `Mesh` from `.cmesh` using explicit material slots supplied
by the caller. `SkeletonAsset` validates `.cskel` and exposes bone rows/names
without allocations. `.cscene` loading constructs the fixed entity classes,
including the unified static/dynamic `prop` class, and retains referenced
`.casset` and other asset handles transactionally.

Install the Python tool dependency with:

```sh
python3 -m pip install -r tools/ceasset/requirements.txt
```

Run the Python tool tests with:

```sh
python3 -m unittest discover tools/ceasset/tests
```

Build and run the C++ asset tests with:

```sh
cmake --build build --target CEngineAssetTests
./build/CEngineAssetTests
```

See `docs/asset_formats.md` for the current format decisions.

## Tooling

The repo includes cross-platform formatting and static analysis configuration:

```sh
cmake --build --preset debug --target format
cmake --build --preset debug --target format-check
cmake --build --preset debug --target tidy
cmake --build --preset debug --target cppcheck
cmake --build --preset debug --target lint
```

Install `clang-format`, `clang-tidy`, and optionally `cppcheck` to use those
targets.
