from __future__ import annotations

import time
from dataclasses import dataclass
from pathlib import Path
from typing import Callable


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
from ceassetlib.game_schema import load_bundled_game
from ceassetlib.blender_scene import blender_scene_settings, scene_description
from ceassetlib.scene_export import write_scene
from ceassetlib.ids import fnv1a, guid_from_stable_name, guid_to_string
from ceassetlib.paths import generic_path, make_project_paths, output_dir_for_source, stored_path
from ceassetlib.state import AssetRecord, load_registry, save_cache, save_registry
from ceassetlib.texture import convert_texture_to_dds, write_rgbexp32_dds

from .animations import write_animation_assets
from .materials import effective_material_names, supported_material_images, write_material_assets
from .authoring import load_lightmap_bindings
from .lightmaps import encode_rgbexp32
from .meshes import write_mesh_assets
from .skeletons import write_skeleton_assets


GAME_SCHEMA = load_bundled_game()

SOURCE_TEXTURE_EXTENSIONS = {".jpeg", ".jpg", ".png", ".tga"}
DEFAULT_DDS_FORMAT = "DXT5"
PACKED_IMAGE_EXTENSIONS = {
    "PNG": ".png",
    "TARGA": ".tga",
    "TGA": ".tga",
    "JPEG": ".jpg",
    "JPG": ".jpg",
}


@dataclass(frozen=True)
class ExportResult:
    collections: int
    textures: int
    materials: int
    skeletons: int
    meshes: int
    animations: int
    scenes: int = 0


@dataclass(frozen=True)
class TextureExport:
    source: Path
    output: Path


def log(message: str) -> None:
    print(f"[CEngine Asset Exporter] {message}", flush=True)


def elapsed(start: float) -> str:
    return f"{time.perf_counter() - start:.2f}s"


def source_fingerprint(path: Path) -> int:
    try:
        stat = path.stat()
        text = f"{generic_path(path.resolve())}|{stat.st_size}|{stat.st_mtime_ns}"
    except OSError:
        text = generic_path(path)
    return fnv1a(text.encode("utf-8"))


def require_bpy():
    if bpy is None:
        raise RuntimeError("CEngine Asset Exporter must run inside Blender")
    return bpy


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
    # Keep Blender's spelling of an absolute path.  Path.resolve() follows
    # macOS's /var -> /private/var symlink, which makes the same image acquire
    # two different path identities depending on which API supplied it.
    path = Path(blender.path.abspath(filepath)).absolute()
    if path.suffix.lower() not in SOURCE_TEXTURE_EXTENSIONS or not path.exists():
        return None
    return path


def packed_image_extension(image: object) -> str:
    filepath = str(getattr(image, "filepath", "") or "")
    suffix = Path(filepath).suffix.lower()
    if suffix in SOURCE_TEXTURE_EXTENSIONS:
        return suffix
    name_suffix = Path(str(getattr(image, "name", "") or "")).suffix.lower()
    if name_suffix in SOURCE_TEXTURE_EXTENSIONS:
        return name_suffix
    file_format = str(getattr(image, "file_format", "") or "").upper()
    return PACKED_IMAGE_EXTENSIONS.get(file_format, ".png")


def image_source_path_for_export(
    image: object,
    blend_source: Path,
    output_root: Path,
    packed_sources: dict[int, Path],
    logger: Callable[[str], None] | None = None,
) -> Path | None:
    source = image_source_path(image)
    if source is not None:
        return source

    packed_file = getattr(image, "packed_file", None)
    data = getattr(packed_file, "data", None)
    if data is None:
        return None

    key = id(image)
    if key in packed_sources:
        return packed_sources[key]

    image_name = clean_asset_filename(str(getattr(image, "name", "") or "packed_image"))
    source_dir = output_dir_for_source(blend_source, output_root) / "_packed_texture_sources"
    packed_source = source_dir / f"{image_name}{packed_image_extension(image)}"
    source_dir.mkdir(parents=True, exist_ok=True)
    payload = bytes(data)
    if not packed_source.exists() or packed_source.read_bytes() != payload:
        packed_source.write_bytes(payload)
    packed_sources[key] = packed_source
    if logger is not None:
        logger(f"Texture source: unpacked bundled Blender image {getattr(image, 'name', '<unnamed>')} -> {packed_source}")
    return packed_source


def clean_asset_filename(name: str) -> str:
    cleaned = "".join(character if character.isalnum() or character in "._-" else "_" for character in name)
    cleaned = cleaned.strip("._-")
    return cleaned or "asset"


def texture_output_path(blend_source: Path, output_root: Path, image_source: Path) -> Path:
    return output_dir_for_source(blend_source, output_root) / "textures" / image_source.with_suffix(".dds").name


def asset_path_for_project(project_root: Path, output: Path) -> str:
    return generic_path(stored_path(project_root, output))


def asset_path_relative_to(root: Path, output: Path) -> str:
    return generic_path(stored_path(root, output))


def material_images(objects: list[object], logger: Callable[[str], None] | None = None) -> list[object]:
    start = time.perf_counter()
    images: list[object] = []
    seen: set[int] = set()
    material_count = 0
    link_count = 0
    for obj in objects:
        for slot in getattr(obj, "material_slots", ()):
            material = getattr(slot, "material", None)
            if material is not None:
                material_count += 1
            for image in supported_material_images(material) if material is not None else ():
                link_count += 1
                key = id(image)
                if key in seen:
                    continue
                seen.add(key)
                images.append(image)
    if logger is not None:
        logger(
            f"Material image scan: {len(images)} image(s) from {material_count} material slot(s) "
            f"and {link_count} supported image input(s) in {elapsed(start)}"
        )
    return images


def export_textures(
    blend_source: Path,
    output_root: Path,
    dds_format: str,
    images: list[object] | None = None,
    logger: Callable[[str], None] | None = None,
    image_source_resolver: Callable[[object], Path | None] | None = None,
) -> list[TextureExport]:
    blender = require_bpy()
    start = time.perf_counter()
    outputs: list[TextureExport] = []
    seen: set[Path] = set()
    texture_images = images if images is not None else list(getattr(blender.data, "images", ()))
    if logger is not None:
        logger(f"Texture export: checking {len(texture_images)} candidate image datablock(s)")
    resolver = image_source_resolver or image_source_path
    for image in texture_images:
        image_name = str(getattr(image, "name", "") or getattr(image, "filepath", "") or "<unnamed image>")
        source = resolver(image)
        if source is None:
            if logger is not None:
                logger(f"Texture skipped: {image_name} has no supported source file")
            continue
        if source in seen:
            if logger is not None:
                logger(f"Texture skipped: {source.name} already converted")
            continue
        seen.add(source)
        output = texture_output_path(blend_source, output_root, source)
        texture_start = time.perf_counter()
        if logger is not None:
            logger(f"Texture {len(outputs) + 1}: converting {source} -> {output}")
        convert_texture_to_dds(source, output, dds_format)
        if logger is not None:
            logger(f"Texture {len(outputs) + 1}: wrote {output.name} in {elapsed(texture_start)}")
        outputs.append(TextureExport(source, output))
    if logger is not None:
        logger(f"Texture export complete: {len(outputs)} texture(s) in {elapsed(start)}")
    return outputs


def export_skybox_panoramas(
    blender: object,
    blend_source: Path,
    output_root: Path,
    objects: list[object],
    logger: Callable[[str], None] | None = None,
) -> tuple[dict[str, Path], list[TextureExport]]:
    by_entity: dict[str, Path] = {}
    outputs: list[TextureExport] = []
    by_source: dict[Path, Path] = {}
    for obj in objects:
        getter = getattr(obj, "get", None)
        if not callable(getter) or str(getter("ce_classname", "")) != "skybox":
            continue
        authored_path = str(getter("ce_panorama", "") or "")
        source = Path(blender.path.abspath(authored_path)).absolute()
        if source.suffix.lower() not in {".exr", ".hdr"} or not source.is_file():
            raise RuntimeError(f"skybox panorama must reference an existing .exr or .hdr: {obj.name}")
        output = by_source.get(source)
        if output is None:
            output = output_dir_for_source(blend_source, output_root) / "environments" / \
                f"{clean_asset_filename(source.stem)}.dds"
            image = blender.data.images.load(str(source), check_existing=True)
            width, height = int(image.size[0]), int(image.size[1])
            if width <= 0 or height <= 0 or width != height * 2:
                raise RuntimeError(f"skybox panorama must use a 2:1 equirectangular layout: {source}")
            pixels = tuple(float(value) for value in image.pixels[:])
            write_rgbexp32_dds(output, width, height, encode_rgbexp32(pixels))
            by_source[source] = output
            outputs.append(TextureExport(source, output))
            if logger is not None:
                logger(f"Skybox panorama: {source} -> {output} ({width}x{height} RGBE HDR)")
        by_entity[str(obj.name)] = output
    return by_entity, outputs


def bool_attr(value: object, name: str) -> bool:
    return bool(getattr(value, name, False))


def call_bool_method(value: object, name: str, default: bool = False) -> bool:
    method = getattr(value, name, None)
    if method is None:
        return default
    try:
        return bool(method())
    except TypeError:
        return default
    except RuntimeError:
        return default


def collection_is_exportable(collection: object) -> bool:
    return not (
        bool_attr(collection, "hide_render")
        or bool_attr(collection, "hide_viewport")
        or call_bool_method(collection, "hide_get")
    )


def object_is_exportable(obj: object) -> bool:
    return not (
        bool_attr(obj, "hide_render")
        or bool_attr(obj, "hide_viewport")
        or call_bool_method(obj, "hide_get")
    )


def object_instance_collection(obj: object) -> object | None:
    if str(getattr(obj, "instance_type", "") or "") == "COLLECTION":
        collection = getattr(obj, "instance_collection", None)
        if collection is not None:
            return collection
    return getattr(obj, "dupli_group", None)


def collect_object_tree(
    obj: object,
    objects: list[object],
    seen_objects: set[int],
    seen_collections: set[int],
    logger: Callable[[str], None] | None,
) -> None:
    key = id(obj)
    if key not in seen_objects:
        seen_objects.add(key)
        objects.append(obj)

    instance_collection = object_instance_collection(obj)
    if instance_collection is not None:
        if logger is not None:
            logger(
                f"Collection walk: expanding collection instance {getattr(obj, 'name', '<unnamed>')} "
                f"-> {getattr(instance_collection, 'name', '<unnamed collection>')}"
            )
        collect_collection_tree(instance_collection, objects, seen_objects, seen_collections, logger)

    for child in getattr(obj, "children", ()):
        if not object_is_exportable(child):
            continue
        collect_object_tree(child, objects, seen_objects, seen_collections, logger)


def collect_collection_tree(
    collection: object,
    objects: list[object],
    seen_objects: set[int],
    seen_collections: set[int],
    logger: Callable[[str], None] | None,
) -> None:
    key = id(collection)
    if key in seen_collections:
        return
    seen_collections.add(key)

    if not collection_is_exportable(collection):
        if logger is not None:
            logger(f"Collection walk: skipped hidden/unrendered collection {getattr(collection, 'name', '<unnamed>')}")
        return

    direct_objects = list(getattr(collection, "objects", ()))
    child_collections = list(getattr(collection, "children", ()))
    if logger is not None:
        logger(
            f"Collection walk: visiting {getattr(collection, 'name', '<unnamed>')} "
            f"with {len(direct_objects)} direct object(s), {len(child_collections)} child collection(s)"
        )

    for obj in direct_objects:
        if not object_is_exportable(obj):
            continue
        collect_object_tree(obj, objects, seen_objects, seen_collections, logger)

    for child in child_collections:
        collect_collection_tree(child, objects, seen_objects, seen_collections, logger)


def collection_export_objects(
    collection: object,
    seen_collections: set[int] | None = None,
    logger: Callable[[str], None] | None = None,
) -> list[object]:
    objects: list[object] = []
    collect_collection_tree(collection, objects, set(), seen_collections or set(), logger)
    return objects


def exported_collection_objects(collections, logger: Callable[[str], None] | None = None) -> list[object]:
    start = time.perf_counter()
    objects: list[object] = []
    seen: set[int] = set()
    collection_names: list[str] = []
    skipped_collections = 0
    skipped_objects = 0
    for collection in collections:
        collection_names.append(str(getattr(collection, "name", "<unnamed collection>")))
        if not collection_is_exportable(collection):
            skipped_collections += 1
            continue
        collection_objects = collection_export_objects(collection, logger=logger)
        for obj in collection_objects:
            if not object_is_exportable(obj):
                skipped_objects += 1
                continue
            key = id(obj)
            if key in seen:
                continue
            seen.add(key)
            objects.append(obj)
    result = sorted(objects, key=lambda obj: obj.name)
    if logger is not None:
        counts: dict[str, int] = {}
        for obj in result:
            obj_type = str(getattr(obj, "type", "UNKNOWN") or "UNKNOWN")
            counts[obj_type] = counts.get(obj_type, 0) + 1
        count_text = ", ".join(f"{key}={value}" for key, value in sorted(counts.items())) or "none"
        preview = ", ".join(
            f"{getattr(obj, 'name', '<unnamed>')}:{getattr(obj, 'type', 'UNKNOWN')}"
            for obj in result[:40]
        )
        logger(
            f"Collection walk: {collection_names} -> {len(result)} object(s) "
            f"({count_text}), skipped hidden/unrendered collections={skipped_collections}, "
            f"objects={skipped_objects} in {elapsed(start)}"
        )
        logger(f"Collection walk objects: {preview if preview else '<none>'}")
        if len(result) > 40:
            logger(f"Collection walk objects: ... {len(result) - 40} more object(s)")
    return result


def selected_export_collection(include_plain: bool = False) -> object | None:
    blender = require_bpy()
    collection = getattr(getattr(blender, "context", None), "collection", None)
    default_type = AssetType.PREFAB if include_plain else AssetType.UNKNOWN
    if collection is None or collection_export_spec(collection, default_type) is None:
        return None
    return collection


def animation_fps() -> float:
    blender = require_bpy()
    scene = getattr(blender.context, "scene", None)
    render = getattr(scene, "render", None)
    fps = float(getattr(render, "fps", 24.0))
    fps_base = float(getattr(render, "fps_base", 1.0))
    return fps / fps_base if fps_base > 0.0 else fps


def object_material_names(obj: object) -> list[str]:
    return effective_material_names(obj)


def object_component_assets(
    obj: object,
    mesh_outputs: dict[str, list[tuple[Path, str]]],
    material_outputs: dict[str, Path],
    skeleton_outputs: dict[str, Path],
    animation_outputs: dict[str, list[Path]],
    asset_path: Callable[[Path], str],
) -> dict[str, object]:
    assets: dict[str, object] = {}
    meshes = [output for output, _material in mesh_outputs.get(obj.name, [])]
    if meshes:
        assets["mesh"] = [asset_path(mesh) for mesh in meshes]

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


def register_outputs(
    project_root: Path,
    source: Path,
    outputs: list[Path],
    logger: Callable[[str], None] | None = None,
    source_hash: int | None = None,
) -> None:
    if not outputs:
        return
    start = time.perf_counter()
    paths = make_project_paths(project_root)
    records = load_registry(paths)
    record_source_hash = source_hash if source_hash is not None else source_fingerprint(source)
    for output in outputs:
        asset_type = asset_type_for_extension(output.suffix)
        if asset_type == AssetType.UNKNOWN:
            raise RuntimeError(f"unsupported CEngine output extension: {output.suffix}")
        stored_source = stored_path(paths.root, source)
        stored_output = stored_path(paths.root, output)
        guid = guid_from_stable_name(generic_path(stored_output))
        record = AssetRecord(guid, stored_source, stored_output, asset_type, record_source_hash)
        for index, existing in enumerate(records):
            if existing.guid == guid or existing.output == stored_output:
                records[index] = record
                break
        else:
            records.append(record)
        if logger is not None:
            logger(f"Registry queued: {generic_path(stored_output)} ({asset_type.name.lower()}) {guid_to_string(guid)}")
    save_registry(paths, records)
    save_cache(paths, records)
    if logger is not None:
        logger(f"Registry/cache updated for {len(outputs)} output(s) in {elapsed(start)}")


def export_selected_collection_assets(output_root: Path, source: Path | None = None) -> ExportResult:
    export_start = time.perf_counter()
    log("Collection-only export started")
    collection = selected_export_collection(include_plain=True)
    if collection is None:
        raise RuntimeError("select a collection before exporting CEngine collection assets")

    root = output_root.resolve()
    outputs: list[Path] = []
    spec = collection_export_spec(collection, AssetType.PREFAB, source.stem if source is not None else None)
    if spec is None:
        raise RuntimeError("select a collection before exporting CEngine collection assets")
    if spec.asset_type == AssetType.SCENE:
        raise RuntimeError("SCENE_ collections require a saved Blender file and full map export")
    output_dir = output_dir_for_source(source, root) if source is not None else root
    output = output_dir / f"{spec.asset_name}{RUNTIME_EXTENSIONS[spec.asset_type]}"
    payload_source = Path(source.name) if source is not None else Path(f"unsaved:{spec.collection_name}")
    log(f"Collection-only root: {collection.name} -> {output}")
    objects = exported_collection_objects([collection], log)
    payload_start = time.perf_counter()
    payload = collection_payload(payload_source, spec, objects, bundle_dir=output.parent)
    log(f"Collection-only payload built: {len(payload)} byte(s) in {elapsed(payload_start)}")
    stable_name = generic_path(output)
    desc = make_asset_desc(
        AssetType.ASSET,
        stable_name,
        fnv1a(generic_path(payload_source).encode("utf-8")),
        payload,
    )
    write_binary_asset(output, desc)
    outputs.append(output)

    log(f"Collection-only export complete: {len(outputs)} .casset in {elapsed(export_start)}")
    return ExportResult(len(outputs), 0, 0, 0, 0, 0)


def export_current_file(output_root: Path | None = None, dds_format: str = "DXT5") -> ExportResult:
    export_start = time.perf_counter()
    log("Export started")
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
    log(f"Source blend: {source}")
    log(f"Output root: {root}")
    log(f"Project root: {project_root}")
    source_hash = source_fingerprint(source)
    log(f"Source fingerprint: {source_hash:016x} (path/size/mtime, no full .blend hash)")
    collection = selected_export_collection(include_plain=True)
    if collection is None:
        raise RuntimeError("select the top-level collection to export as a CEngine asset")
    log(f"Selected asset root collection: {collection.name}")
    collection_spec = collection_export_spec(collection, AssetType.PREFAB, source.stem)
    if collection_spec is None:
        raise RuntimeError("selected collection has no CEngine export type")
    is_scene = collection_spec.asset_type == AssetType.SCENE
    objects = exported_collection_objects([collection], log)

    packed_sources: dict[int, Path] = {}
    image_resolver = lambda image: image_source_path_for_export(image, source, root, packed_sources, log)

    texture_outputs = export_textures(source, root, dds_format, material_images(objects, log), log, image_resolver)
    texture_outputs_by_source = {texture.source: texture.output for texture in texture_outputs}
    phase_start = time.perf_counter()
    log("Material export started")
    material_outputs = write_material_assets(
        source,
        root,
        objects,
        image_resolver,
        texture_outputs_by_source.get,
        asset_path,
        log,
        source_hash,
        dds_format,
    )
    log(f"Material export complete: {len(material_outputs)} .cmat in {elapsed(phase_start)}")
    material_outputs_by_name = {material.source.name: material.output for material in material_outputs}
    phase_start = time.perf_counter()
    log("Skeleton export started")
    skeleton_outputs = write_skeleton_assets(source, root, objects, asset_path, log, source_hash)
    log(f"Skeleton export complete: {len(skeleton_outputs)} .cskel in {elapsed(phase_start)}")
    skeleton_outputs_by_name = {skeleton.source.name: skeleton.output for skeleton in skeleton_outputs}
    phase_start = time.perf_counter()
    log("Animation export started")
    animation_outputs = write_animation_assets(
        source,
        root,
        objects,
        animation_fps(),
        skeleton_outputs_by_name.get,
        asset_path,
        log,
        source_hash,
    )
    log(f"Animation export complete: {len(animation_outputs)} .canim in {elapsed(phase_start)}")

    lightmap_placements = {}
    lightmap_outputs: list[Path] = []
    scene_objects = objects if is_scene else []
    skybox_outputs: dict[str, Path] = {}
    skybox_texture_outputs: list[TextureExport] = []
    if is_scene:
        skybox_outputs, skybox_texture_outputs = export_skybox_panoramas(
            blender, source, root, scene_objects, log)
    if is_scene:
        lightmap_placements, lightmap_outputs = load_lightmap_bindings(
            scene_objects,
            lambda value: Path(blender.path.abspath(value)).absolute(),
        )
        log(
            "Lightmap bindings: "
            f"{len(lightmap_placements)} object(s), {len(lightmap_outputs)} existing atlas file(s)"
        )

    phase_start = time.perf_counter()
    log("Mesh export started")
    mesh_outputs = write_mesh_assets(
        source,
        root,
        objects,
        material_outputs_by_name.get,
        skeleton_outputs_by_name.get,
        asset_path,
        log,
        source_hash,
    )
    log(f"Mesh export complete: {len(mesh_outputs)} .cmesh in {elapsed(phase_start)}")
    mesh_outputs_by_name: dict[str, list[tuple[Path, str]]] = {}
    for mesh in mesh_outputs:
        mesh_outputs_by_name.setdefault(mesh.source.name, []).append((mesh.output, mesh.material_name))
    animation_outputs_by_armature: dict[str, list[Path]] = {}
    for animation in animation_outputs:
        animation_outputs_by_armature.setdefault(animation.armature.name, []).append(animation.output)

    collection_outputs: list[Path] = []
    scene_outputs: list[Path] = []
    collection_output_dir = output_dir_for_source(source, root)
    casset_asset_path = lambda output: asset_path_relative_to(collection_output_dir, output)
    phase_start = time.perf_counter()
    object_assets = lambda obj: object_component_assets(
        obj, mesh_outputs_by_name, material_outputs_by_name,
        skeleton_outputs_by_name, animation_outputs_by_armature,
        casset_asset_path)
    if is_scene:
        scene_output = collection_output_dir / f"{collection_spec.asset_name}.cscene"
        description = scene_description(
            scene_objects, mesh_outputs_by_name,
            material_outputs_by_name, asset_path,
            lightmap_placements, skybox_outputs,
            resolve_asset_path=lambda value: Path(
                blender.path.abspath(value)).absolute())
        description = type(description)(description.entities, blender_scene_settings(blender),
                                        description.connections)
        write_scene(scene_output, description, asset_path(scene_output), source_hash)
        scene_outputs.append(scene_output)
        log(f"Scene export complete: {scene_output}")
    else:
        log("Root .casset export started")
        output = write_collection_asset(
            source, root, collection, object_assets, asset_path,
            AssetType.PREFAB, source.stem, Path(source.name), log, objects, source_hash)
        if output is not None:
            collection_outputs.append(output)
        log(f"Root .casset export complete: {len(collection_outputs)} file(s) in {elapsed(phase_start)}")

    register_outputs(
        project_root,
        source,
        collection_outputs
        + scene_outputs
        + [material.output for material in material_outputs]
        + [texture for material in material_outputs
           for texture in getattr(material, "generated_textures", ())]
        + [mesh.output for mesh in mesh_outputs]
        + [skeleton.output for skeleton in skeleton_outputs]
        + [animation.output for animation in animation_outputs]
		+ lightmap_outputs,
        log,
        source_hash,
    )
    for texture_output in texture_outputs:
        register_outputs(
            project_root,
            texture_output.source,
            [texture_output.output],
            log,
            source_fingerprint(texture_output.source),
        )
    for texture_output in skybox_texture_outputs:
        register_outputs(
            project_root,
            texture_output.source,
            [texture_output.output],
            log,
            source_fingerprint(texture_output.source),
        )

    result = ExportResult(
        len(collection_outputs),
        len(texture_outputs) + len(skybox_texture_outputs) + len(lightmap_outputs) +
        sum(len(getattr(material, "generated_textures", ())) for material in material_outputs),
        len(material_outputs),
        len(skeleton_outputs),
        len(mesh_outputs),
        len(animation_outputs),
        len(scene_outputs),
    )
    log(f"Export finished: {export_summary(result, root)} in {elapsed(export_start)}")
    return result


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
        f"Exported {result.scenes} .cscene, {result.collections} .casset, {result.meshes} .cmesh, "
        f"{result.materials} .cmat, {result.skeletons} .cskel, "
        f"{result.animations} .canim, and {result.textures} .dds assets{destination}"
    )
