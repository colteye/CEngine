# CEngine

CEngine is now built with CMake instead of Visual Studio project files.

## Build

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
