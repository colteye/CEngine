# Game files and generated schemas

Each game has one portable JSON game file. The viewer sample is
`samples/viewer/game.json`; it imports the engine definitions in
`schemas/engine.game.json`.

The file contains data only:

- game/module identity and version;
- source and compiled content roots;
- available target asset types and extensions;
- entity classnames, versions, fields, defaults, and validation bounds.

It contains no C++ class names, include paths, factory callbacks, GLM types, or
Blender/Python implementation names.

## Build outputs

`tools/schema/generate.py` reads a game file and can produce two outputs:

1. a C++ header for the records owned by that file;
2. a flattened game file with imported engine definitions included.

The generated header is standalone. It includes only `<cstddef>`, `<cstdint>`,
and `<cstring>`. It defines plain property values, serialized data values, and
inline `Read(...)` overloads. Readers consume an exact byte range, decode every
integer and float as little-endian, validate it, and never reinterpret a disk
structure as a host structure.

There is no generated `.cpp`, reflection runtime, descriptor registry, binary
reader object, engine context, asset manager, entity factory, or logging API.

CMake generates the engine header before `CEngineScene` compiles. The viewer
build generates its game header, links the engine library, flattens the complete
viewer game file, and packages that same flattened file into the Blender add-on.
The add-on therefore discovers custom game entities without a hand-maintained
Python layout table. It also builds Blender's entity creation menu and settings
UI from the same flattened schema. See [Blender asset
authoring](blender_addon.md).

All viewer-specific build logic lives beside the game in
`samples/viewer/CMakeLists.txt`: its schema output, custom entity sources,
executable, tests, and add-on package. The root build only exposes engine
libraries and opts into that sample directory.

The extension gate creates a temporary game with a new entity and asset type,
runs the generator from outside that game directory, compiles and executes the
standalone header, and loads the same flattened file through the Python schema
API.

## Runtime boundary

An entity inherits only its generated public properties:

```cpp
class PlayerEntity final
    : public CEngine::Scene::Entity,
      public Viewer::Generated::PlayerProperties {
    // Game behavior.
};
```

The game registers the binary type in one line:

```cpp
factory.Register<PlayerEntity, Generated::Player>("player", actions);
```

The factory constructs the entity, calls the generated `Read(...)`, copies the
decoded public properties, and applies the decoded transform to the engine
entity. It does not know individual fields. Asset fields remain stable
scene-local indices in generated data; entity lifecycle code resolves them
through the scene and asset APIs when it needs to create subsystem resources.

Programmatic gameplay spawning does not serialize or invoke the factory:

```cpp
auto& player = scene.CreateEntity<PlayerEntity>("Player", actions);
```

## Wire field types

All numeric values are little-endian. Records have no padding.

| Type | Bytes | Generated value |
| --- | ---: | --- |
| `f32` | 4 | `float` |
| `u32` | 4 | `std::uint32_t` |
| `bool` | 4 | validated zero or one |
| `enum` | 4 | generated `enum class` |
| `vec2` | 8 | two floats |
| `vec3` | 12 | three floats |
| `transform` | 40 | position, XYZW rotation, scale |
| `asset` | 4 | scene asset index |
| `asset_list` | 8 | first/count into class auxiliary asset indices |
| `flags` | 4 | validated mask expanded into named booleans |

Every entity schema currently has one `transform` field. The transform is
decoded separately from generated properties so runtime entities have only one
authoritative transform.

## Adding a game entity

1. Add the entity schema to the game's JSON file.
2. Add its entity class under the game's `entity/` folder and inherit the
   generated `...Properties` type.
3. Put behavior in its lifecycle methods and call engine subsystem APIs there.
4. Register the generated data type with the game's `EntityFactory`.
5. Rebuild. CMake regenerates C++, the flattened game file, and the add-on.

The schema field vocabulary currently drives `.cscene` entity records. The game
file also owns the target asset-type catalog. Existing non-scene asset payloads
remain on their tested fixed formats until each is deliberately migrated; they
must not be represented by placeholder schemas.
