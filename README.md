# CEngine

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

By default, CMake downloads and builds GLFW 3.4 with `FetchContent`, so Visual
Studio project files or prebuilt GLFW libraries are not required. To use an
installed GLFW package instead, configure with `-DCENGINE_USE_SYSTEM_GLFW=ON`.

On Linux, fetched GLFW still needs the development packages for the selected
desktop backend. The default fetched backend is X11; use
`-DCENGINE_FETCH_GLFW_WAYLAND=ON -DCENGINE_FETCH_GLFW_X11=OFF` for Wayland, or
use the `debug-headless` preset for a compile/link check on minimal images.

The viewer copies the full `assets/` tree beside the executable after each build.

## Asset pipeline

The engine runtime should load target assets only. Source formats from tools such
as Blender and Photoshop are converted by Python tooling before they become
runtime files.

```sh
python3 tools/ceasset/ceasset.py convert assets/demo/barrel/results/barrel_albedo.DDS
python3 tools/ceasset/ceasset.py convert assets/source/barrel_test/barrel_low_barrel_BaseColor.png
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
`.blend` through `bpy`, converts tracked PNG/TGA image datablocks to `.dds`,
exports material datablocks used by the selected collection to `.cmat`, exports
selected armature bone hierarchies to `.cskel`, exports selected armature actions
to `.canim`, triangulates selected mesh polygons into `.cmesh` triangle buffers
at export time, and writes one tagged collection target asset directly under
`assets/compiled/<blend-name>/`. Orphan datablocks and unrelated scene
collections are ignored.

- `PREFAB_Hero` writes `Hero.casset`.
- `SCENE_TestRoom` writes `TestRoom.casset`.
- Collection custom properties can override names with
  `ce_asset_type = prefab|scene` and `ce_asset_name = desired_name`.

Select the top-level collection you want to become the asset root before running
`File > Export > CEngine Assets`. Nested child collections are walked through
that root, and the resulting component files are linked together by the single
`.casset` root.

The current Blender phase exports hierarchy/placement payloads, static and
skinned mesh geometry buffers, material payloads, skeleton metadata, animation
F-curve payloads, and textures.
Each exported object record contains its Blender name, Blender object type,
CEngine role, parent name when the parent is inside the exported collection, and
a row-major `world_from_local` transform. `.casset` object records also include
generated component asset paths such as `.cmesh`, `.cmat`, `.cskel`, and
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
without allocations. A future `.casset` runtime loader can instantiate component
graphs from the binary composition payload when the entity/component side is
ready.

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
