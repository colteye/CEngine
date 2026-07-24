"""Author and bake the minimal dynamic-lighting probes in external Sponza.

Run with Blender:
    blender -b /path/to/sponza.blend \
        --python tools/ceasset/author_sponza_environment_probes.py
"""

from __future__ import annotations

import sys
from pathlib import Path

import bpy


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "ceasset"))
sys.path.insert(0, str(ROOT / "tools" / "blender_addon"))
for module_name in tuple(sys.modules):
    if module_name == "ceassetlib" or \
            module_name.startswith("ceassetlib.") or \
            module_name == "cengine_asset_exporter" or \
            module_name.startswith("cengine_asset_exporter."):
        del sys.modules[module_name]

from cengine_asset_exporter.environment_probes import (  # noqa: E402
    EnvironmentProbeBakeSettings,
    bake_environment_probes,
)


SOURCE_NAME = "sponza"
IRRADIANCE_VOLUME_NAME = "CE_DynamicIrradianceVolume"
PROBE_LAYOUT = (
    ("CE_EnvironmentProbe_West", (-9.0, 0.35, 3.0), (6.0, 6.8, 5.0)),
    ("CE_EnvironmentProbe_Center", (-0.5, 0.35, 3.0), (5.5, 6.8, 5.0)),
    ("CE_EnvironmentProbe_East", (8.5, 0.35, 3.0), (6.0, 6.8, 5.0)),
)
BAKE_SETTINGS = EnvironmentProbeBakeSettings(resolution=256, samples=64)


def _remove_previous() -> None:
    """Remove only probes owned by this Sponza authoring script."""
    names = {IRRADIANCE_VOLUME_NAME}
    names.update(name for name, _location, _scale in PROBE_LAYOUT)
    for name in names:
        previous = bpy.data.objects.get(name)
        if previous is None:
            continue
        data = previous.data
        bpy.data.objects.remove(previous, do_unlink=True)
        if data is not None and data.users == 0:
            bpy.data.lightprobes.remove(data)


def _probe_collection() -> bpy.types.Collection:
    source = bpy.data.objects.get(SOURCE_NAME)
    if source is None or source.type != "MESH":
        raise RuntimeError("sponza.blend has no mesh object named sponza")
    if not source.users_collection:
        raise RuntimeError("the Sponza mesh is not linked to a collection")
    return source.users_collection[0]


def _add_irradiance_volume(
    collection: bpy.types.Collection,
) -> bpy.types.Object:
    data = bpy.data.lightprobes.new(IRRADIANCE_VOLUME_NAME, "VOLUME")
    data.resolution_x = 6
    data.resolution_y = 3
    data.resolution_z = 3
    data.bake_samples = 32
    data.capture_indirect = True
    data.capture_world = True
    data.capture_emission = True
    probe = bpy.data.objects.new(IRRADIANCE_VOLUME_NAME, data)
    collection.objects.link(probe)
    probe.location = (-0.5, 0.35, 5.0)
    probe.scale = (14.5, 6.8, 7.0)
    probe.show_in_front = True
    return probe


def _add_environment_probe(
    collection: bpy.types.Collection,
    name: str,
    location: tuple[float, float, float],
    scale: tuple[float, float, float],
) -> bpy.types.Object:
    data = bpy.data.lightprobes.new(name, "SPHERE")
    data.influence_type = "BOX"
    data.influence_distance = 1.0
    data.falloff = 0.18
    data.parallax_type = "BOX"
    data.use_custom_parallax = False
    data.clip_start = 0.05
    data.clip_end = 50.0
    probe = bpy.data.objects.new(name, data)
    collection.objects.link(probe)
    probe.location = location
    probe.scale = scale
    probe.show_in_front = True
    probe["ce_classname"] = "environment_probe"
    probe["ce_intensity"] = 1.0
    probe["ce_enabled"] = True
    return probe


def main() -> None:
    """Create, bake, and save Sponza's dynamic-lighting probes."""
    if not bpy.data.filepath:
        raise RuntimeError("save the Sponza Blender file before authoring probes")
    collection = _probe_collection()
    _remove_previous()
    _add_irradiance_volume(collection)
    probes = [
        _add_environment_probe(collection, name, location, scale)
        for name, location, scale in PROBE_LAYOUT
    ]
    bpy.context.view_layer.update()

    source = Path(bpy.data.filepath)
    outputs = bake_environment_probes(
        bpy,
        source,
        ROOT / "assets" / "compiled",
        tuple(collection.all_objects),
        BAKE_SETTINGS,
        print,
    )
    if len(outputs) != len(probes):
        raise RuntimeError(
            f"baked {len(outputs)} of {len(probes)} Sponza probes")
    bpy.ops.wm.save_as_mainfile(filepath=bpy.data.filepath)
    print(
        f"Authored and baked {len(outputs)} Sponza environment probes: "
        + ", ".join(output.name for output in outputs))


if __name__ == "__main__":
    main()
