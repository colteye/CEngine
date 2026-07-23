# Blender asset authoring

The CEngine Blender add-on makes a `.blend` file the source of truth for a
model/prefab or scene. It cooks native Blender meshes, Principled materials,
images, armatures, animations, lights, cameras, and schema-defined engine
entities into the target asset formats.

## Workspace

The **CEngine** tab in the 3D View sidebar owns the asset workflow:

1. Make the top-level asset collection active.
2. Mark it as **Scene** or **Model / Prefab**.
3. Set an asset name if the collection name should not be used.
4. Author and preview the asset with normal Blender tools.
5. Run **Validate Asset**, then **Export CEngine Asset**.

Files saved under `assets/source` default to the sibling `assets/compiled`
tree. The output root can be overridden in the same panel. Export operates only
on the active collection and its visible descendants.

The export command is deliberately a cooker, not a render command. It does not
invoke Cycles or rebuild lightmap UVs.

## Entities

Use **Add > CEngine Entity** or **CEngine > Add Entity**. The list comes from
the flattened game schema packaged with the add-on, so game-defined entities
appear without handwritten add-on classes.

The add-on uses native Blender data whenever it represents the same concept:

| CEngine entity | Blender authoring object | Native data used |
| --- | --- | --- |
| `prop` | mesh | geometry, materials, transform, render visibility |
| `light` | light | type, color, energy, range, cone/area shape, shadows |
| `player` | camera | transform, vertical field of view, near/far clip |
| `skybox` | empty | HDR panorama plus engine settings |
| `exponential_height_fog` | empty | schema-defined fog settings |
| `post_process` | empty | schema-defined presentation settings |
| other game entity | empty | every schema field and transform |

Selecting an entity exposes its engine settings in **Object Properties >
CEngine Entity**. Mesh and material editing remains in Blender's standard
editors. Engine-only material values are in **Material Properties > CEngine
Material**; ordinary surface inputs continue to use the Principled BSDF graph.

Schema asset fields use Blender-relative file paths. Asset-list fields accept
one path per line or semicolon-separated paths. Visual entities can be inspected
directly in Material Preview or Rendered mode. **Preview in Blender** makes a
player camera active, applies a skybox panorama to the Blender world, or maps
post-process exposure onto Blender's view settings.

## Lightmaps

Lightmap work is a separate, persistent workflow:

1. Set baked or mixed mode on at least one Blender light.
2. Use **Prepare Lightmap UVs** to create/rebuild the `Lightmap` UV layer
   without rendering.
3. Adjust resolution, padding, samples, direct-light inclusion, and denoising.
4. Use **Bake Lightmaps** when lighting or static geometry changes.
5. Save the `.blend` file.
6. Export as often as needed. Export reuses the stored atlas path and each
   mesh's stored scale, offset, and decode range.

The bake command writes the DDS atlas to the compiled asset tree, registers it,
and stores portable `//`-relative bindings on receiving mesh objects. Export
fails with a direct object-specific error if a bound atlas disappeared or its
`Lightmap` UV layer was removed. **Clear Lightmap Bindings** detaches the
metadata without deleting the atlas or UV layers.

This separation keeps ordinary iteration fast: material, entity, transform, or
gameplay-property changes do not force a lighting render. Rebake only after a
change that affects static lighting.

## Adding a game entity

Add the entity to the game's JSON schema and rebuild/package the add-on. Its
creation menu entry and property UI are generated from that schema. A new
entity uses an empty unless it is one of the native mappings above. Scalar,
boolean, enum, vector, flag, asset, and asset-list fields are supported by the
authoring and `.cscene` paths.

Runtime behavior remains game code. The Blender add-on authors the generated
property record and never needs to import a game C++ class.
