#   _____ ______             _
#  / ____|  ____|           (_)
# | |    | |__   _ __   __ _ _ _ __   ___
# | |    |  __| | '_ \ / _` | | '_ \ / _ |
# | |____| |____| | | | (_| | | | | |  __/
#  \_____|______|_| |_|\__, |_|_| |_|\___|
#                       __/ |
#                      |___/

"""Bake native Blender sphere probes into engine-local HDR environments."""

from __future__ import annotations

import math
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Iterable

from ceassetlib.collection_export import clean_asset_name
from ceassetlib.paths import output_dir_for_source


DEFAULT_PROBE_RESOLUTION = 256
DEFAULT_PROBE_SAMPLES = 64


@dataclass(frozen=True)
class EnvironmentProbeBakeSettings:
    """Offline Eevee capture quality for engine environment probes."""

    resolution: int = DEFAULT_PROBE_RESOLUTION
    samples: int = DEFAULT_PROBE_SAMPLES


_FACE_BASES = (
    ((1.0, 0.0, 0.0), (0.0, 1.0, 0.0), (0.0, 0.0, 1.0)),
    ((-1.0, 0.0, 0.0), (0.0, -1.0, 0.0), (0.0, 0.0, 1.0)),
    ((0.0, 1.0, 0.0), (-1.0, 0.0, 0.0), (0.0, 0.0, 1.0)),
    ((0.0, -1.0, 0.0), (1.0, 0.0, 0.0), (0.0, 0.0, 1.0)),
    ((0.0, 0.0, 1.0), (1.0, 0.0, 0.0), (0.0, 1.0, 0.0)),
    ((0.0, 0.0, -1.0), (1.0, 0.0, 0.0), (0.0, -1.0, 0.0)),
)


def _dot(left: tuple[float, float, float],
         right: tuple[float, float, float]) -> float:
    return sum(a * b for a, b in zip(left, right))


def direction_to_face_uv(
    direction: tuple[float, float, float],
) -> tuple[int, float, float]:
    """Map a Blender-world direction to the matching captured cube face."""
    face = max(
        range(len(_FACE_BASES)),
        key=lambda index: _dot(direction, _FACE_BASES[index][0]))
    forward, right, up = _FACE_BASES[face]
    depth = max(_dot(direction, forward), 1.0e-8)
    u = 0.5 * (_dot(direction, right) / depth + 1.0)
    v = 0.5 * (_dot(direction, up) / depth + 1.0)
    return face, min(max(u, 0.0), 1.0), min(max(v, 0.0), 1.0)


def _sample_face(
    pixels: tuple[float, ...], resolution: int, u: float, v: float,
) -> tuple[float, float, float, float]:
    x = u * float(resolution - 1)
    y = v * float(resolution - 1)
    x0, y0 = int(math.floor(x)), int(math.floor(y))
    x1, y1 = min(x0 + 1, resolution - 1), min(y0 + 1, resolution - 1)
    tx, ty = x - x0, y - y0

    def pixel(px: int, py: int) -> tuple[float, float, float, float]:
        offset = (py * resolution + px) * 4
        return tuple(pixels[offset + channel] for channel in range(4))

    lower_left = pixel(x0, y0)
    lower_right = pixel(x1, y0)
    upper_left = pixel(x0, y1)
    upper_right = pixel(x1, y1)
    return tuple(
        (lower_left[channel] * (1.0 - tx) +
         lower_right[channel] * tx) * (1.0 - ty) +
        (upper_left[channel] * (1.0 - tx) +
         upper_right[channel] * tx) * ty
        for channel in range(4)
    )


def _engine_direction_to_blender(
    direction: tuple[float, float, float],
) -> tuple[float, float, float]:
    # Engine coordinates are (Blender Y, -Blender X, Blender Z).
    return -direction[1], direction[0], direction[2]


def _compose_panorama(
    faces: tuple[tuple[float, ...], ...], face_resolution: int,
) -> tuple[int, int, list[float]]:
    width = face_resolution * 4
    height = face_resolution * 2
    pixels: list[float] = []
    for y in range(height):
        latitude = ((float(y) + 0.5) / float(height) - 0.5) * math.pi
        cos_latitude = math.cos(latitude)
        for x in range(width):
            longitude = ((float(x) + 0.5) / float(width) - 0.5) * 2.0 * math.pi
            engine_direction = (
                cos_latitude * math.cos(longitude),
                cos_latitude * math.sin(longitude),
                math.sin(latitude),
            )
            blender_direction = _engine_direction_to_blender(engine_direction)
            face, u, v = direction_to_face_uv(blender_direction)
            pixels.extend(_sample_face(
                faces[face], face_resolution, u, v))
    return width, height, pixels


def _probe_objects(objects: Iterable[object]) -> tuple[object, ...]:
    return tuple(
        obj for obj in objects
        if str(getattr(obj, "type", "")) == "LIGHT_PROBE" and
        str(getattr(getattr(obj, "data", None), "type", "")) == "SPHERE" and
        str(getattr(obj, "get", lambda *_: "")(
            "ce_classname", "")) == "environment_probe"
    )


def _eevee_engine(scene: object) -> str:
    engines = {
        item.identifier
        for item in scene.render.bl_rna.properties["engine"].enum_items
    }
    for identifier in ("BLENDER_EEVEE_NEXT", "BLENDER_EEVEE"):
        if identifier in engines:
            return identifier
    raise RuntimeError("this Blender build has no Eevee render engine")


def bake_environment_probes(
    blender: object,
    source: Path,
    output_root: Path,
    objects: Iterable[object],
    settings: EnvironmentProbeBakeSettings,
    logger: Callable[[str], None] | None = None,
) -> tuple[Path, ...]:
    """Bake local GI/reflection captures using Eevee and native Light Probes."""
    probes = _probe_objects(objects)
    if not probes:
        return ()
    if settings.resolution < 16 or settings.resolution > 1024 or \
            settings.samples < 1:
        raise ValueError("environment probe bake settings are invalid")

    scene = blender.context.scene
    eevee_engine = _eevee_engine(scene)
    volumes = tuple(
        obj for obj in objects
        if str(getattr(obj, "type", "")) == "LIGHT_PROBE" and
        str(getattr(getattr(obj, "data", None), "type", "")) == "VOLUME")
    if volumes:
        previous_engine = scene.render.engine
        try:
            scene.render.engine = eevee_engine
            if logger is not None:
                logger(
                    f"Environment probes: baking {len(volumes)} native "
                    "Eevee irradiance volume(s)")
            blender.ops.object.lightprobe_cache_bake(subset="ALL")
        finally:
            scene.render.engine = previous_engine

    from mathutils import Matrix, Vector

    camera_data = blender.data.cameras.new("CEngineEnvironmentProbeBake")
    camera_data.type = "PERSP"
    camera_data.angle = math.pi * 0.5
    camera_data.lens = 16.0
    camera = blender.data.objects.new(
        "CEngineEnvironmentProbeBake", camera_data)
    scene.collection.objects.link(camera)

    previous = {
        "camera": scene.camera,
        "engine": scene.render.engine,
        "resolution_x": scene.render.resolution_x,
        "resolution_y": scene.render.resolution_y,
        "resolution_percentage": scene.render.resolution_percentage,
        "film_transparent": scene.render.film_transparent,
        "filepath": scene.render.filepath,
        "file_format": scene.render.image_settings.file_format,
        "color_mode": scene.render.image_settings.color_mode,
        "color_depth": scene.render.image_settings.color_depth,
        "taa_samples": getattr(scene.eevee, "taa_render_samples", None)
            if hasattr(scene, "eevee") else None,
    }
    outputs: list[Path] = []
    try:
        scene.camera = camera
        scene.render.engine = eevee_engine
        scene.render.resolution_x = settings.resolution
        scene.render.resolution_y = settings.resolution
        scene.render.resolution_percentage = 100
        scene.render.film_transparent = False
        scene.render.image_settings.file_format = "OPEN_EXR"
        scene.render.image_settings.color_mode = "RGBA"
        scene.render.image_settings.color_depth = "32"
        if hasattr(scene, "eevee") and \
                hasattr(scene.eevee, "taa_render_samples"):
            scene.eevee.taa_render_samples = settings.samples

        output_dir = output_dir_for_source(
            source, output_root) / "probe_bakes"
        output_dir.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(
                prefix="cengine-probe-faces-") as temporary_dir:
            for probe_index, probe in enumerate(probes, 1):
                camera.location = probe.matrix_world.translation
                faces: list[tuple[float, ...]] = []
                for face_index, (
                        forward_values, right_values,
                        up_values) in enumerate(_FACE_BASES):
                    forward = Vector(forward_values)
                    right = Vector(right_values)
                    up = Vector(up_values)
                    rotation = Matrix((
                        right, up, -forward)).transposed().to_quaternion()
                    camera.rotation_mode = "QUATERNION"
                    camera.rotation_quaternion = rotation
                    blender.context.view_layer.update()
                    face_output = Path(temporary_dir) / \
                        f"probe_{probe_index}_face_{face_index}.exr"
                    scene.render.filepath = str(face_output)
                    blender.ops.render.render(write_still=True)
                    result = blender.data.images.load(
                        str(face_output), check_existing=False)
                    try:
                        result_size = tuple(int(value) for value in result.size)
                        face_pixels = tuple(
                            float(value) for value in result.pixels[:])
                    finally:
                        blender.data.images.remove(result)
                    expected_values = \
                        settings.resolution * settings.resolution * 4
                    if result_size != (
                            settings.resolution, settings.resolution) or \
                            len(face_pixels) != expected_values:
                        raise RuntimeError(
                            "Eevee returned an unexpected environment-probe "
                            f"face buffer: size={result_size}, "
                            f"values={len(face_pixels)}, "
                            f"expected={settings.resolution}x"
                            f"{settings.resolution} RGBA")
                    faces.append(face_pixels)

                width, height, pixels = _compose_panorama(
                    tuple(faces), settings.resolution)
                image = blender.data.images.new(
                    f"CEngineProbe_{probe.name}",
                    width=width, height=height, alpha=True, float_buffer=True)
                image.pixels.foreach_set(pixels)
                output = output_dir / \
                    f"{clean_asset_name(str(probe.name))}.exr"
                image.file_format = "OPEN_EXR"
                image.filepath_raw = str(output)
                image.save()
                blender.data.images.remove(image)
                probe["ce_panorama"] = blender.path.relpath(str(output))
                outputs.append(output)
                if logger is not None:
                    logger(
                        f"Environment probe {probe_index}/{len(probes)}: "
                        f"{probe.name} -> {output.name} ({width}x{height})")
    finally:
        scene.camera = previous["camera"]
        scene.render.engine = previous["engine"]
        scene.render.resolution_x = previous["resolution_x"]
        scene.render.resolution_y = previous["resolution_y"]
        scene.render.resolution_percentage = previous[
            "resolution_percentage"]
        scene.render.film_transparent = previous["film_transparent"]
        scene.render.filepath = previous["filepath"]
        scene.render.image_settings.file_format = previous["file_format"]
        scene.render.image_settings.color_mode = previous["color_mode"]
        scene.render.image_settings.color_depth = previous["color_depth"]
        if previous["taa_samples"] is not None and hasattr(scene, "eevee"):
            scene.eevee.taa_render_samples = previous["taa_samples"]
        blender.data.objects.remove(camera, do_unlink=True)
        blender.data.cameras.remove(camera_data)
    return tuple(outputs)
