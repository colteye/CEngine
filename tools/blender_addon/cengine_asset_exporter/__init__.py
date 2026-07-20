from __future__ import annotations

import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Callable


bl_info = {
    "name": "CEngine Asset Exporter",
    "author": "CEngine",
    "version": (0, 1, 1),
    "blender": (4, 5, 0),
    "location": "File > Export > CEngine Assets",
    "description": "Export selected Blender collections and textures to CEngine target assets.",
    "category": "Import-Export",
}

ADDON_DIR = Path(__file__).resolve().parent
if str(ADDON_DIR) not in sys.path:
    sys.path.insert(0, str(ADDON_DIR))

WHEELS_DIR = ADDON_DIR / "wheels"
VENDOR_DIR = ADDON_DIR / "vendor"
if VENDOR_DIR.exists() and str(VENDOR_DIR) not in sys.path:
    sys.path.insert(0, str(VENDOR_DIR))


def forget_bundled_modules(package_names: set[str]) -> None:
    addon_path = str(ADDON_DIR).lower()
    for module_name, module in list(sys.modules.items()):
        if module_name.split(".", 1)[0] not in package_names:
            continue
        module_file = str(getattr(module, "__file__", "") or "").lower()
        if addon_path in module_file:
            del sys.modules[module_name]


forget_bundled_modules({"ceassetlib", "PIL"})

try:
    import bpy
except ImportError:
    bpy = None

from ceassetlib.assetfile import make_asset_desc, write_binary_asset
from ceassetlib.collection_export import (
    RUNTIME_EXTENSIONS,
    collection_export_spec,
    collection_payload,
    write_collection_asset,
)
from ceassetlib.formats import AssetType, asset_type_for_extension
from ceassetlib.ids import fnv1a
from ceassetlib.paths import generic_path, make_project_paths, output_dir_for_source, stored_path
from ceassetlib.pipeline import register_conversion
from ceassetlib.state import load_registry, save_cache
from ceassetlib.texture import convert_texture_to_dds

from .animations import write_animation_assets
from .materials import write_material_assets
from .meshes import write_mesh_assets
from .skeletons import write_skeleton_assets


SOURCE_TEXTURE_EXTENSIONS = {".png", ".tga"}
DEFAULT_DDS_FORMAT = "DXT5"


@dataclass(frozen=True)
class ExportResult:
    collections: int
    textures: int
    materials: int
    skeletons: int
    meshes: int
    animations: int


@dataclass(frozen=True)
class TextureExport:
    source: Path
    output: Path


def require_bpy():
    if bpy is None:
        raise RuntimeError("CEngine Asset Exporter must run inside Blender")
    return bpy


def asset_type_name(output: Path) -> str:
    asset_type = asset_type_for_extension(output.suffix)
    if asset_type == AssetType.UNKNOWN:
        raise RuntimeError(f"unsupported CEngine output extension: {output.suffix}")
    return asset_type.name.lower()


def blend_source_path() -> Path:
    blender = require_bpy()
    filepath = str(getattr(blender.data, "filepath", "") or "")
    if not filepath:
        raise RuntimeError("save the Blender file before exporting CEngine assets")

    source = Path(filepath).resolve()
    if not source.is_file():
        raise RuntimeError("save the Blender file before exporting CEngine assets")
    return source


def maybe_blend_source_path() -> Path | None:
    blender = require_bpy()
    filepath = str(getattr(blender.data, "filepath", "") or "")
    if not filepath:
        return None

    source = Path(filepath).resolve()
    return source if source.is_file() else None


def default_output_root_for_source(source: Path | None) -> Path | None:
    if source is None:
        return None
    return make_project_paths(project_root_for(source)).compiled_dir


def project_root_for(source: Path) -> Path:
    if source.parent.name == "source" and source.parent.parent.name == "assets":
        return source.parent.parent.parent
    for parent in source.parents:
        if parent.name == "source" and parent.parent.name == "assets":
            return parent.parent.parent
    return Path.cwd().resolve()


def project_root_for_output_root(output_root: Path) -> Path | None:
    root = output_root.resolve()
    if root.name == "compiled" and root.parent.name == "assets":
        return root.parent.parent
    return None


def image_source_path(image) -> Path | None:
    filepath = str(getattr(image, "filepath", "") or "")
    if not filepath:
        return None

    blender = require_bpy()
    path = Path(blender.path.abspath(filepath)).resolve()
    if path.suffix.lower() not in SOURCE_TEXTURE_EXTENSIONS or not path.exists():
        return None
    return path


def texture_output_path(blend_source: Path, output_root: Path, image_source: Path) -> Path:
    return output_dir_for_source(blend_source, output_root) / "textures" / image_source.with_suffix(".dds").name


def asset_path_for_project(project_root: Path, output: Path) -> str:
    return generic_path(stored_path(project_root, output))


def asset_path_relative_to(root: Path, output: Path) -> str:
    return generic_path(stored_path(root, output))


def material_images(objects: list[object]) -> list[object]:
    images: list[object] = []
    seen: set[int] = set()
    for obj in objects:
        for slot in getattr(obj, "material_slots", ()):
            material = getattr(slot, "material", None)
            node_tree = getattr(material, "node_tree", None) if material is not None else None
            for link in getattr(node_tree, "links", ()) if node_tree is not None else ():
                from_node = getattr(link, "from_node", None)
                if getattr(from_node, "type", "") != "TEX_IMAGE":
                    continue
                image = getattr(from_node, "image", None)
                if image is None:
                    continue
                key = id(image)
                if key in seen:
                    continue
                seen.add(key)
                images.append(image)
    return images


def export_textures(
    blend_source: Path,
    output_root: Path,
    dds_format: str,
    images: list[object] | None = None,
) -> list[TextureExport]:
    blender = require_bpy()
    outputs: list[TextureExport] = []
    seen: set[Path] = set()
    texture_images = images if images is not None else list(getattr(blender.data, "images", ()))
    for image in texture_images:
        source = image_source_path(image)
        if source is None or source in seen:
            continue
        seen.add(source)
        output = texture_output_path(blend_source, output_root, source)
        convert_texture_to_dds(source, output, dds_format)
        outputs.append(TextureExport(source, output))
    return outputs


def exported_collection_objects(collections) -> list[object]:
    objects: list[object] = []
    seen: set[int] = set()
    for collection in collections:
        collection_objects = getattr(collection, "all_objects", None)
        if collection_objects is None:
            collection_objects = getattr(collection, "objects", ())
        for obj in collection_objects:
            key = id(obj)
            if key in seen:
                continue
            seen.add(key)
            objects.append(obj)
    return sorted(objects, key=lambda obj: obj.name)


def selected_export_collection(include_plain: bool = False) -> object | None:
    blender = require_bpy()
    collection = getattr(getattr(blender, "context", None), "collection", None)
    default_type = AssetType.PREFAB if include_plain else AssetType.UNKNOWN
    if collection is None or collection_export_spec(collection, default_type) is None:
        return None
    return collection


def selected_export_collections(include_plain: bool = False) -> list[object]:
    collection = selected_export_collection(include_plain)
    return [collection] if collection is not None else []


def animation_fps() -> float:
    blender = require_bpy()
    scene = getattr(blender.context, "scene", None)
    render = getattr(scene, "render", None)
    fps = float(getattr(render, "fps", 24.0))
    fps_base = float(getattr(render, "fps_base", 1.0))
    return fps / fps_base if fps_base > 0.0 else fps


def object_material_names(obj: object) -> list[str]:
    names: list[str] = []
    for slot in getattr(obj, "material_slots", ()):
        material = getattr(slot, "material", None)
        if material is not None:
            names.append(material.name)
    return names


def object_component_assets(
    obj: object,
    mesh_outputs: dict[str, Path],
    material_outputs: dict[str, Path],
    skeleton_outputs: dict[str, Path],
    animation_outputs: dict[str, list[Path]],
    asset_path: Callable[[Path], str],
) -> dict[str, object]:
    assets: dict[str, object] = {}
    mesh = mesh_outputs.get(obj.name)
    if mesh is not None:
        assets["mesh"] = asset_path(mesh)

    materials = [
        asset_path(material_outputs[name])
        for name in object_material_names(obj)
        if name in material_outputs
    ]
    if materials:
        assets["materials"] = materials

    skeleton = skeleton_outputs.get(obj.name)
    if skeleton is not None:
        assets["skeleton"] = asset_path(skeleton)

    animations = [asset_path(output) for output in animation_outputs.get(obj.name, ())]
    if animations:
        assets["animations"] = animations
    return assets


def register_outputs(project_root: Path, source: Path, outputs: list[Path]) -> None:
    paths = make_project_paths(project_root)
    for output in outputs:
        asset_type = asset_type_for_extension(output.suffix)
        if asset_type == AssetType.UNKNOWN:
            raise RuntimeError(f"unsupported CEngine output extension: {output.suffix}")
        register_conversion(paths, source, asset_type, output)
    save_cache(paths, load_registry(paths))


def export_selected_collection_assets(output_root: Path, source: Path | None = None) -> ExportResult:
    collection = selected_export_collection(include_plain=True)
    if collection is None:
        raise RuntimeError("select a collection before exporting CEngine collection assets")

    root = output_root.resolve()
    outputs: list[Path] = []
    spec = collection_export_spec(collection, AssetType.PREFAB, source.stem if source is not None else None)
    if spec is None:
        raise RuntimeError("select a collection before exporting CEngine collection assets")
    output_dir = output_dir_for_source(source, root) if source is not None else root
    output = output_dir / f"{spec.asset_name}{RUNTIME_EXTENSIONS[spec.asset_type]}"
    payload_source = Path(source.name) if source is not None else Path(f"unsaved:{spec.collection_name}")
    payload = collection_payload(payload_source, spec, exported_collection_objects([collection]), bundle_dir=output.parent)
    stable_name = generic_path(output)
    desc = make_asset_desc(
        AssetType.ASSET,
        stable_name,
        fnv1a(generic_path(payload_source).encode("utf-8")),
        payload,
    )
    write_binary_asset(output, desc)
    outputs.append(output)

    return ExportResult(len(outputs), 0, 0, 0, 0, 0)


def export_current_file(output_root: Path | None = None, dds_format: str = "DXT5") -> ExportResult:
    blender = require_bpy()
    source = maybe_blend_source_path()
    if source is None:
        if output_root is None:
            raise RuntimeError("choose an Output Root or save the Blender file before exporting CEngine assets")
        return export_selected_collection_assets(output_root)

    root = output_root.resolve() if output_root is not None else default_output_root_for_source(source)
    if root is None:
        raise RuntimeError("choose an Output Root or save the Blender file before exporting CEngine assets")
    project_root = project_root_for_output_root(root) or project_root_for(source)
    asset_path = lambda output: asset_path_for_project(project_root, output)
    collection = selected_export_collection(include_plain=True)
    if collection is None:
        raise RuntimeError("select the top-level collection to export as a CEngine asset")
    objects = exported_collection_objects([collection])

    texture_outputs = export_textures(source, root, dds_format, material_images(objects))
    texture_outputs_by_source = {texture.source: texture.output for texture in texture_outputs}
    material_outputs = write_material_assets(
        source,
        root,
        objects,
        image_source_path,
        texture_outputs_by_source.get,
        asset_path,
    )
    material_outputs_by_name = {material.source.name: material.output for material in material_outputs}
    skeleton_outputs = write_skeleton_assets(source, root, objects, asset_path)
    skeleton_outputs_by_name = {skeleton.source.name: skeleton.output for skeleton in skeleton_outputs}
    animation_outputs = write_animation_assets(
        source,
        root,
        objects,
        animation_fps(),
        skeleton_outputs_by_name.get,
        asset_path,
    )
    mesh_outputs = write_mesh_assets(
        source,
        root,
        objects,
        material_outputs_by_name.get,
        skeleton_outputs_by_name.get,
        asset_path,
    )
    mesh_outputs_by_name = {mesh.source.name: mesh.output for mesh in mesh_outputs}
    animation_outputs_by_armature: dict[str, list[Path]] = {}
    for animation in animation_outputs:
        animation_outputs_by_armature.setdefault(animation.armature.name, []).append(animation.output)

    collection_outputs: list[Path] = []
    collection_output_dir = output_dir_for_source(source, root)
    casset_asset_path = lambda output: asset_path_relative_to(collection_output_dir, output)
    output = write_collection_asset(
        source,
        root,
        collection,
        lambda obj: object_component_assets(
            obj,
            mesh_outputs_by_name,
            material_outputs_by_name,
            skeleton_outputs_by_name,
            animation_outputs_by_armature,
            casset_asset_path,
        ),
        asset_path,
        AssetType.PREFAB,
        source.stem,
        Path(source.name),
    )
    if output is not None:
        collection_outputs.append(output)

    register_outputs(project_root, source, collection_outputs)
    register_outputs(project_root, source, [material.output for material in material_outputs])
    register_outputs(project_root, source, [mesh.output for mesh in mesh_outputs])
    register_outputs(project_root, source, [skeleton.output for skeleton in skeleton_outputs])
    register_outputs(project_root, source, [animation.output for animation in animation_outputs])
    for texture_output in texture_outputs:
        register_outputs(project_root, texture_output.source, [texture_output.output])

    return ExportResult(
        len(collection_outputs),
        len(texture_outputs),
        len(material_outputs),
        len(skeleton_outputs),
        len(mesh_outputs),
        len(animation_outputs),
    )


def run_export(output_root: Path | None, dds_format: str = "DXT5", collection_only: bool = False) -> ExportResult:
    if not collection_only:
        return export_current_file(output_root, dds_format)

    root = output_root
    source = maybe_blend_source_path()
    if root is None:
        if source is None:
            raise RuntimeError("choose an Output Root before exporting collection assets")
        root = make_project_paths(project_root_for(source)).compiled_dir
    return export_selected_collection_assets(root, source)


def export_summary(result: ExportResult, output_root: Path | None) -> str:
    destination = f" to {output_root}" if output_root is not None else ""
    return (
        f"Exported {result.collections} .casset, {result.meshes} .cmesh, "
        f"{result.materials} .cmat, {result.skeletons} .cskel, "
        f"{result.animations} .canim, and {result.textures} .dds assets{destination}"
    )


if bpy is not None:

    class CENGINE_OT_export_assets(bpy.types.Operator):
        bl_idname = "cengine.export_assets"
        bl_label = "Export CEngine Assets"
        bl_options = {"REGISTER"}

        output_root: bpy.props.StringProperty(  # type: ignore[valid-type]
            name="Output Root",
            description="Folder that receives CEngine target assets. Saved files under assets/source default to assets/compiled.",
            subtype="DIR_PATH",
            default="",
        )
        dds_format: bpy.props.EnumProperty(  # type: ignore[valid-type]
            name="DDS Format",
            items=(
                ("DXT5", "DXT5", "RGBA textures"),
                ("DXT1", "DXT1", "Opaque/color textures"),
                ("BC5", "BC5", "Normal map style two-channel textures"),
            ),
            default=DEFAULT_DDS_FORMAT,
        )
        collection_only: bpy.props.BoolProperty(  # type: ignore[valid-type]
            name="Collection Only",
            description="Export selected PREFAB_ or SCENE_ collections as .casset files only",
            default=False,
        )

        def invoke(self, context, event):
            del event
            if not self.output_root:
                default_root = default_output_root_for_source(maybe_blend_source_path())
                if default_root is not None:
                    self.output_root = str(default_root)
            return context.window_manager.invoke_props_dialog(self)

        def draw(self, _context):
            layout = self.layout
            layout.use_property_split = True
            layout.use_property_decorate = False
            layout.prop(self, "output_root")
            layout.prop(self, "dds_format")
            layout.prop(self, "collection_only")

        def execute(self, _context):
            root = Path(self.output_root).resolve() if self.output_root else None
            try:
                result = run_export(root, self.dds_format, self.collection_only)
            except RuntimeError as error:
                self.report({"ERROR"}, str(error))
                return {"CANCELLED"}
            self.report({"INFO"}, export_summary(result, root))
            return {"FINISHED"}

    def menu_export(self, _context):
        self.layout.operator(CENGINE_OT_export_assets.bl_idname, text="CEngine Assets")


CLASSES = (CENGINE_OT_export_assets,) if bpy is not None else ()


def register():
    blender = require_bpy()
    for cls in CLASSES:
        blender.utils.register_class(cls)
    blender.types.TOPBAR_MT_file_export.append(menu_export)


def unregister():
    blender = require_bpy()
    blender.types.TOPBAR_MT_file_export.remove(menu_export)
    for cls in reversed(CLASSES):
        blender.utils.unregister_class(cls)
