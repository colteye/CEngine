# Blender asset authoring

The CEngine Blender add-on makes a `.blend` file the source of truth for a
model/prefab or scene. It exports/processes native Blender meshes, Principled materials,
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

The export command is an asset exporter/processor, not a render command. It does not
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
| `player_spawn` | empty | team, spawn group, priority, clearance radius, transform |
| `skybox` | empty | HDR panorama plus engine settings |
| `fog` | empty | schema-defined fog settings |
| `post_process` | empty | schema-defined presentation settings |
| `physics_constraint` | empty | two physics-enabled prop references, anchors, limits, springs, and optional motor |
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

## Physics

A `prop` may own a rigid body independently of its visible mesh. Choose a
primitive collider, static plane, convex hull, triangle mesh, heightfield, or
compound and author motion type, mass, material response, damping, gravity,
axis locks, sensor/CCD/sleep settings, and collision layer in the entity panel.

The visible object is the default collision source. For a deliberately simpler
or invisible collision mesh, assign a mesh object parented to the prop as its
collision source. The helper is omitted from render export, and its transform is
cooked relative to the body. **Toggle Collider Preview** shows the authored
collider without changing the exported asset.

**Generate Physics Mesh** creates that helper directly from the selected mesh:

- Leave **Concave Mesh** disabled for one enclosing Blender convex hull.
- Enable it to split the source into a compound of convex hulls. **Accuracy**
  controls how aggressively concave regions are refined, while **Max Hulls**
  is a hard performance and file-size limit.
- Generated `COL_*` objects use shaded solid display with a wire edge overlay
  and remain editable. Running the command again replaces only the helper
  previously generated for that object.

Generation evaluates modifiers and triangulates n-gons without modifying the
render mesh. Large sources use a single connectivity pass, split oversized
triangle clusters before hull construction, and select at most 512
representative key vertices per hull. Exported hulls are capped at 256 vertices
for the physics backend. Source import and connectivity memory remain linear in
mesh size, while the expensive repeated convex-hull work stays bounded even
when the render mesh is much larger than a sensible collider.

Constraints are ordinary schema-defined scene entities. Their object-reference
fields select two physics-enabled props; export resolves those object names to
stable scene entity indices. The add-on rejects missing bodies, self-links,
invalid motor combinations, and invalid limits before writing the scene.

## Lightmaps

Lightmap work is a separate, persistent workflow:

1. Set baked or mixed mode on at least one Blender light.
2. Use **Prepare Lightmap UVs** to create/rebuild the `Lightmap` UV layer
   without rendering.
3. Adjust resolution, padding, samples, and denoising.
4. Use **Bake Lightmaps** when lighting or static geometry changes.
5. Save the `.blend` file.
6. Export as often as needed. Export reuses the stored atlas path and each
   mesh's stored scale, offset, and decode range.

The bake command writes the DDS atlas to the compiled asset tree, registers it,
and stores portable `//`-relative bindings on receiving mesh objects. Export
fails with a direct object-specific error if a bound atlas disappeared or its
`Lightmap` UV layer was removed. **Clear Lightmap Bindings** detaches the
metadata without deleting the atlas or UV layers.

Each command renders two scene-linear Cycles images and adds them before
denoising and encoding the single final atlas. The indirect pass includes
Blender's World plus baked and mixed lights. The direct pass keeps the World
and baked lights but hides mixed and realtime lights. Consequently, the
lightmap stores geometry-occluded environment diffuse, all baked-light
transport, and mixed-light bounce lighting; mixed direct lighting and shadows
remain runtime work.

This separation keeps ordinary iteration fast: material, entity, transform, or
gameplay-property changes do not force a lighting render. Rebake only after a
change that affects static lighting.

## Adding a game entity

Add the entity to the game's JSON schema and rebuild/package the add-on. Its
creation menu entry and property UI are generated from that schema. A new
entity uses an empty unless it is one of the native mappings above. Scalar,
boolean, enum, vector, flag, asset, asset-list, and scene-entity-reference
fields are supported by the authoring and `.cscene` paths.

Runtime behavior remains game code. The Blender add-on authors the generated
property record and never needs to import a game C++ class.
