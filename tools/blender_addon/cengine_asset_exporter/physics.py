#   _____ ______             _
#  / ____|  ____|           (_)
# | |    | |__   _ __   __ _ _ _ __   ___
# | |    |  __| | '_ \ / _` | | '_ \ / _ \
# | |____| |____| | | | (_| | | | | |  __/
#  \_____|______|_| |_|\__, |_|_| |_|\___|
#                       __/ |
#                      |___/

"""TODO: Briefly describe this module.

Author:
    Erik Coltey
"""

from __future__ import annotations

import math
import types
from dataclasses import dataclass, field
from enum import IntEnum
from pathlib import Path
from typing import Callable, Iterable

from ceassetlib.assetfile import AssetWriteDesc, write_binary_asset
from ceassetlib.collection_export import clean_asset_name
from ceassetlib.blender_scene import object_transform
from ceassetlib.formats import AssetType
from ceassetlib.game_schema import load_bundled_game
from ceassetlib.ids import guid_from_stable_name
from ceassetlib.paths import generic_path, output_dir_for_source
from ceassetlib.wire import pack_record

from .meshes import blender_to_engine_vector


GAME_SCHEMA = load_bundled_game()


class ShapeType(IntEnum):
    """TODO: Describe `ShapeType`."""

    BOX = 0
    SPHERE = 1
    CAPSULE = 2
    CYLINDER = 3
    CONVEX_HULL = 4
    TRIANGLE_MESH = 5
    HEIGHT_FIELD = 6
    COMPOUND = 7
    PLANE = 8


@dataclass
class CollisionShape:
    """TODO: Describe `CollisionShape`."""

    shape_type: ShapeType = ShapeType.BOX
    local_position: tuple[float, float, float] = (0.0, 0.0, 0.0)
    local_rotation: tuple[float, float, float, float] = (0.0, 0.0, 0.0, 1.0)
    half_extents: tuple[float, float, float] = (0.5, 0.5, 0.5)
    radius: float = 0.5
    half_height: float = 0.5
    vertices: list[tuple[float, float, float]] = field(default_factory=list)
    indices: list[int] = field(default_factory=list)
    heights: list[float] = field(default_factory=list)
    samples_per_side: int = 0
    height_field_offset: tuple[float, float, float] = (0.0, 0.0, 0.0)
    height_field_scale: tuple[float, float, float] = (1.0, 1.0, 1.0)
    children: list[CollisionShape] = field(default_factory=list)


@dataclass(frozen=True)
class PhysicsExport:
    """TODO: Describe `PhysicsExport`."""

    source: object
    output: Path


def _finite(values: Iterable[float]) -> bool:
    """TODO: Describe `_finite`.

    Args:
        values: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    return all(math.isfinite(float(value)) for value in values)


def validate_shape(shape: CollisionShape, depth: int = 0) -> None:
    """TODO: Describe `validate_shape`.

    Args:
        shape: TODO: Describe this parameter.
        depth: TODO: Describe this parameter.
    """
    if depth > 8:
        raise ValueError("collision compound nesting exceeds 8 levels")
    if not _finite((*shape.local_position, *shape.local_rotation)):
        raise ValueError("collision shape transform must be finite")
    no_geometry = not shape.vertices and not shape.indices and \
        not shape.heights and shape.samples_per_side == 0
    if shape.shape_type == ShapeType.BOX:
        if not no_geometry or shape.children or \
                not _finite(shape.half_extents) or \
                any(value <= 0.0 for value in shape.half_extents):
            raise ValueError("box half extents must be positive")
    elif shape.shape_type == ShapeType.SPHERE:
        if not no_geometry or shape.children or \
                not math.isfinite(shape.radius) or shape.radius <= 0.0:
            raise ValueError("sphere radius must be positive")
    elif shape.shape_type in (ShapeType.CAPSULE, ShapeType.CYLINDER):
        if not no_geometry or shape.children or \
                not math.isfinite(shape.radius) or shape.radius <= 0.0 or \
                not math.isfinite(shape.half_height) or shape.half_height <= 0.0:
            raise ValueError("capsule/cylinder radius and half height must be positive")
    elif shape.shape_type == ShapeType.PLANE:
        if not no_geometry or shape.children or \
                not _finite(shape.half_extents) or \
                shape.half_extents[0] <= 0.0 or \
                shape.half_extents[1] <= 0.0:
            raise ValueError("plane preview extents must be positive")
    elif shape.shape_type == ShapeType.CONVEX_HULL:
        if len(shape.vertices) < 4 or shape.indices or shape.heights or \
                shape.samples_per_side or shape.children or \
                not all(_finite(vertex) for vertex in shape.vertices):
            raise ValueError("convex hull needs at least four finite points")
    elif shape.shape_type == ShapeType.TRIANGLE_MESH:
        if len(shape.vertices) < 3 or not shape.indices or len(shape.indices) % 3:
            raise ValueError("triangle collision needs indexed triangles")
        if shape.heights or shape.samples_per_side or shape.children or \
                not all(_finite(vertex) for vertex in shape.vertices) or \
                any(index < 0 or index >= len(shape.vertices) for index in shape.indices):
            raise ValueError("triangle collision geometry is invalid")
    elif shape.shape_type == ShapeType.HEIGHT_FIELD:
        expected = shape.samples_per_side * shape.samples_per_side
        if shape.samples_per_side < 2 or len(shape.heights) != expected or \
                not _finite(shape.heights) or not _finite(shape.height_field_offset) or \
                not _finite(shape.height_field_scale) or \
                any(value <= 0.0 for value in shape.height_field_scale) or \
                shape.vertices or shape.indices or shape.children:
            raise ValueError("height field samples or scale are invalid")
    elif shape.shape_type == ShapeType.COMPOUND:
        if not no_geometry or not shape.children:
            raise ValueError("compound collision needs at least one child")
        for child in shape.children:
            validate_shape(child, depth + 1)
    else:
        raise ValueError("collision shape type is unsupported")


def _movable(shape: CollisionShape) -> bool:
    """TODO: Describe `_movable`.

    Args:
        shape: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    return shape.shape_type not in {
        ShapeType.TRIANGLE_MESH, ShapeType.HEIGHT_FIELD, ShapeType.PLANE
    } and all(_movable(child) for child in shape.children)


def physics_payload(root: CollisionShape) -> bytes:
    """TODO: Describe `physics_payload`.

    Args:
        root: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    validate_shape(root)
    flattened: list[tuple[CollisionShape, int]] = []

    def visit(shape: CollisionShape, parent: int) -> None:
        """TODO: Describe `visit`.

        Args:
            shape: TODO: Describe this parameter.
            parent: TODO: Describe this parameter.
        """
        index = len(flattened)
        flattened.append((shape, parent))
        for child in shape.children:
            visit(child, index)

    visit(root, -1)
    records: list[dict[str, object]] = []
    for shape, parent in flattened:
        records.append({
            "type": int(shape.shape_type),
            "parent": parent,
            "position": shape.local_position,
            "rotation": shape.local_rotation,
            "half_extents": shape.half_extents,
            "radius": shape.radius,
            "half_height": shape.half_height,
            "vertices": [
                component for vertex in shape.vertices for component in vertex
            ],
            "indices": shape.indices,
            "heights": shape.heights,
            "samples_per_side": shape.samples_per_side,
            "height_offset": shape.height_field_offset,
            "height_scale": shape.height_field_scale,
        })
    return pack_record(GAME_SCHEMA, "physics", {"nodes": records})


def physics_output_path(
    blend_source: Path, output_root: Path, object_name: str
) -> Path:
    """TODO: Describe `physics_output_path`.

    Args:
        blend_source: TODO: Describe this parameter.
        output_root: TODO: Describe this parameter.
        object_name: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    return output_dir_for_source(
        blend_source, output_root
    ) / "physics" / f"{clean_asset_name(object_name)}.cphys"


def _property(obj: object, name: str, default: object) -> object:
    """TODO: Describe `_property`.

    Args:
        obj: TODO: Describe this parameter.
        name: TODO: Describe this parameter.
        default: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    getter = getattr(obj, "get", None)
    return getter(name, default) if callable(getter) else default


def _object_scale(obj: object) -> tuple[float, float, float]:
    """TODO: Describe `_object_scale`.

    Args:
        obj: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    scale = tuple(float(value) for value in getattr(obj, "scale", (1.0, 1.0, 1.0)))
    if len(scale) != 3 or not _finite(scale) or any(value <= 0.0 for value in scale):
        raise ValueError(f"collision object scale must be positive: {getattr(obj, 'name', '<unnamed>')}")
    # Blender X/Y map to engine -Y/+X.
    return (scale[1], scale[0], scale[2])


def _mesh_geometry(obj: object) -> tuple[
    list[tuple[float, float, float]], list[int]
]:
    """TODO: Describe `_mesh_geometry`.

    Args:
        obj: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    mesh = getattr(obj, "data", None)
    source_vertices = list(getattr(mesh, "vertices", ()))
    scale = _object_scale(obj)
    vertices = [
        tuple(component * scale[axis] for axis, component in enumerate(
            blender_to_engine_vector(vertex.co)))
        for vertex in source_vertices
    ]
    indices: list[int] = []
    for polygon in getattr(mesh, "polygons", ()):
        polygon_vertices = [
            int(value) for value in getattr(polygon, "vertices", ())
        ]
        if len(polygon_vertices) < 3:
            continue
        for index in range(1, len(polygon_vertices) - 1):
            # Axis conversion changes handedness, so reverse the source winding.
            indices.extend((
                polygon_vertices[0],
                polygon_vertices[index + 1],
                polygon_vertices[index],
            ))
    return vertices, indices


def _height_field(obj: object) -> CollisionShape:
    """TODO: Describe `_height_field`.

    Args:
        obj: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    vertices, _ = _mesh_geometry(obj)
    side = math.isqrt(len(vertices))
    if side < 2 or side * side != len(vertices):
        raise ValueError(
            f"height-field mesh must have a square vertex grid: {obj.name}")
    xs = sorted({round(vertex[0], 6) for vertex in vertices})
    ys = sorted({round(vertex[1], 6) for vertex in vertices})
    if len(xs) != side or len(ys) != side:
        raise ValueError(
            f"height-field rows and columns are ambiguous: {obj.name}")
    x_step = (xs[-1] - xs[0]) / (side - 1)
    y_step = (ys[-1] - ys[0]) / (side - 1)
    if x_step <= 0.0 or y_step <= 0.0 or any(
            not math.isclose(xs[index], xs[0] + index * x_step,
                rel_tol=1.0e-4, abs_tol=1.0e-5)
            for index in range(side)) or any(
            not math.isclose(ys[index], ys[0] + index * y_step,
                rel_tol=1.0e-4, abs_tol=1.0e-5)
            for index in range(side)):
        raise ValueError(
            f"height-field grid spacing must be uniform: {obj.name}")
    samples: dict[tuple[float, float], float] = {}
    for x, y, z in vertices:
        key = (round(x, 6), round(y, 6))
        if key in samples:
            raise ValueError(
                f"height-field grid contains duplicate samples: {obj.name}")
        samples[key] = z
    return CollisionShape(
        shape_type=ShapeType.HEIGHT_FIELD,
        heights=[samples[(x, y)] for y in ys for x in xs],
        samples_per_side=side,
        height_field_offset=(xs[0], ys[0], 0.0),
        height_field_scale=(x_step, y_step, 1.0),
    )


def _compound_shape(obj: object) -> CollisionShape:
    """TODO: Describe `_compound_shape`.

    Args:
        obj: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    children = []
    for child in sorted(
            getattr(obj, "children", ()),
            key=lambda value: str(getattr(value, "name", ""))):
        if getattr(child, "type", "") != "MESH":
            continue
        child_shape = collision_shape_for_object(child)
        local_matrix = getattr(child, "matrix_local", None)
        local = object_transform(types.SimpleNamespace(
            name=getattr(child, "name", ""), matrix_world=local_matrix))
        child_shape.local_position = local.position
        child_shape.local_rotation = local.rotation
        children.append(child_shape)
    if not children:
        raise ValueError(
            f"compound collider needs mesh children: {obj.name}")
    return CollisionShape(
        shape_type=ShapeType.COMPOUND, children=children)


def collision_shape_for_object(obj: object) -> CollisionShape:
    """TODO: Describe `collision_shape_for_object`.

    Args:
        obj: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    collider_name = str(_property(obj, "ce_collider", "box")).lower()
    try:
        shape_type = {
            "box": ShapeType.BOX,
            "sphere": ShapeType.SPHERE,
            "capsule": ShapeType.CAPSULE,
            "cylinder": ShapeType.CYLINDER,
            "convex_hull": ShapeType.CONVEX_HULL,
            "triangle_mesh": ShapeType.TRIANGLE_MESH,
            "height_field": ShapeType.HEIGHT_FIELD,
            "compound": ShapeType.COMPOUND,
            "plane": ShapeType.PLANE,
        }[collider_name]
    except KeyError as error:
        raise ValueError(
            f"unsupported collider '{collider_name}': {getattr(obj, 'name', '<unnamed>')}"
        ) from error

    if shape_type == ShapeType.HEIGHT_FIELD:
        shape = _height_field(obj)
        validate_shape(shape)
        return shape
    if shape_type == ShapeType.COMPOUND:
        shape = _compound_shape(obj)
        validate_shape(shape)
        return shape

    _object_scale(obj)
    dimensions = tuple(
        abs(float(value)) for value in getattr(obj, "dimensions", (1.0, 1.0, 1.0))
    )
    engine_dimensions = (
        dimensions[1], dimensions[0], dimensions[2]
    )
    shape = CollisionShape(shape_type=shape_type)
    if shape_type == ShapeType.BOX:
        shape.half_extents = tuple(value * 0.5 for value in engine_dimensions)
    elif shape_type == ShapeType.SPHERE:
        shape.radius = max(engine_dimensions) * 0.5
    elif shape_type in (ShapeType.CAPSULE, ShapeType.CYLINDER):
        shape.radius = max(engine_dimensions[0], engine_dimensions[1]) * 0.5
        shape.half_height = max(
            engine_dimensions[2] * 0.5 -
            (shape.radius if shape_type == ShapeType.CAPSULE else 0.0),
            1.0e-4,
        )
    elif shape_type == ShapeType.PLANE:
        shape.half_extents = (
            engine_dimensions[0] * 0.5,
            engine_dimensions[1] * 0.5,
            0.0,
        )
    else:
        shape.vertices, shape.indices = _mesh_geometry(obj)
        if shape_type == ShapeType.CONVEX_HULL:
            shape.indices.clear()
    validate_shape(shape)
    return shape


def physics_objects(objects: Iterable[object]) -> list[object]:
    """TODO: Describe `physics_objects`.

    Args:
        objects: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    return sorted((
        obj for obj in objects
        if getattr(obj, "type", "") == "MESH"
        and str(_property(
            obj, "ce_physics_motion", "None")).lower() != "none"
    ), key=lambda obj: str(getattr(obj, "name", "")))


def write_physics_asset(
    blend_source: Path,
    output_root: Path,
    obj: object,
    asset_path: Callable[[Path], str] = generic_path,
    source_hash: int = 0,
    collision_source: object | None = None,
) -> PhysicsExport:
    """TODO: Describe `write_physics_asset`.

    Args:
        blend_source: TODO: Describe this parameter.
        output_root: TODO: Describe this parameter.
        obj: TODO: Describe this parameter.
        asset_path: TODO: Describe this parameter.
        source_hash: TODO: Describe this parameter.
        collision_source: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    motion = str(_property(
        obj, "ce_physics_motion", "None")).lower()
    if motion not in ("static", "dynamic", "kinematic"):
        raise ValueError(
            f"physics motion must be Static, Dynamic, or Kinematic: {obj.name}")
    source = collision_source or obj
    if getattr(source, "type", "") != "MESH":
        raise ValueError(
            f"collision source must be a mesh: "
            f"{getattr(source, 'name', '<unnamed>')}")
    shape = collision_shape_for_object(source)
    if source is not obj:
        parent = getattr(source, "parent", None)
        if parent is not obj:
            raise ValueError(
                f"collision source must be parented to its physics prop: "
                f"{source.name} -> {obj.name}")
        local = object_transform(types.SimpleNamespace(
            name=source.name,
            matrix_world=getattr(source, "matrix_local", None)))
        shape.local_position = local.position
        shape.local_rotation = local.rotation
    if motion in ("dynamic", "kinematic") and not _movable(shape):
        raise ValueError(
            f"movable object cannot use triangle-mesh or height-field collision; "
            f"choose convex_hull or compound made only from convex shapes: {obj.name}"
        )
    output = physics_output_path(blend_source, output_root, str(obj.name))
    write_binary_asset(output, AssetWriteDesc(
        asset_type=AssetType.PHYSICS,
        guid=guid_from_stable_name(asset_path(output)),
        source_hash=source_hash,
        platform_target="generic",
        payload=physics_payload(shape),
    ))
    return PhysicsExport(obj, output)


def write_physics_assets(
    blend_source: Path,
    output_root: Path,
    objects: Iterable[object],
    asset_path: Callable[[Path], str] = generic_path,
    source_hash: int = 0,
) -> list[PhysicsExport]:
    """TODO: Describe `write_physics_assets`.

    Args:
        blend_source: TODO: Describe this parameter.
        output_root: TODO: Describe this parameter.
        objects: TODO: Describe this parameter.
        asset_path: TODO: Describe this parameter.
        source_hash: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    objects = tuple(objects)
    by_name = {
        str(getattr(obj, "name", "")): obj for obj in objects
    }
    exports: list[PhysicsExport] = []
    for obj in physics_objects(objects):
        collision_name = str(
            _property(obj, "ce_collision_object", "") or "")
        collision_source = by_name.get(collision_name) if collision_name else None
        if collision_name and collision_source is None:
            raise ValueError(
                f"collision source does not exist: {obj.name} -> "
                f"{collision_name}")
        exports.append(write_physics_asset(
            blend_source, output_root, obj, asset_path, source_hash,
            collision_source))
    return exports
