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

import math
from pathlib import Path

# Blender discovers RNA properties from evaluated class annotations.
try:
    import bpy
except ImportError:
    bpy = None

from ceassetlib.collection_export import collection_export_spec
from ceassetlib.formats import AssetType

from . import exporter
from .authoring import (
    CLASSNAME_PROPERTY,
    clear_lightmap_bindings,
    display_name,
    entity_classname,
    entity_schema,
    enum_items,
    field_by_property,
    initialize_entity_properties,
    native_object_type,
    property_name,
    store_lightmap_bindings,
    validate_entities,
)
from .lightmaps import (
    DEFAULT_DENOISE,
    DEFAULT_INCLUDE_DIRECT,
    DEFAULT_PADDING,
    DEFAULT_RESOLUTION,
    DEFAULT_SAMPLES,
    LightmapSettings,
    bake_scene_lightmaps,
    ensure_lightmap_uvs,
)

_ENUM_ITEM_CACHE: dict[tuple[str, str], tuple[tuple[str, str, str], ...]] = {}


def _settings(context):
    """TODO: Describe `_settings`.

    Args:
        context: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    return context.scene.cengine_settings


def _output_root(context) -> Path | None:
    """TODO: Describe `_output_root`.

    Args:
        context: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    value = str(_settings(context).output_root or "")
    if value:
        return Path(bpy.path.abspath(value)).absolute()
    return exporter.default_output_root_for_source(
        exporter.maybe_blend_source_path())


def _active_collection(context):
    """TODO: Describe `_active_collection`.

    Args:
        context: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    collection = getattr(context, "collection", None)
    if collection is None:
        raise RuntimeError("make an asset collection active first")
    return collection


def _scene_collection(context):
    """TODO: Describe `_scene_collection`.

    Args:
        context: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    collection = _active_collection(context)
    spec = collection_export_spec(collection, AssetType.PREFAB)
    if spec is None or spec.asset_type != AssetType.SCENE:
        raise RuntimeError(
            "mark the active collection as a CEngine Scene before using lightmaps")
    return collection, spec


def _lightmap_settings(context) -> LightmapSettings:
    """TODO: Describe `_lightmap_settings`.

    Args:
        context: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    settings = _settings(context)
    return LightmapSettings(
        resolution=settings.lightmap_resolution,
        padding=settings.lightmap_padding,
        samples=settings.lightmap_samples,
        denoise=settings.lightmap_denoise,
        include_direct=settings.lightmap_include_direct,
    )


def _select_only(context, obj) -> None:
    """TODO: Describe `_select_only`.

    Args:
        context: TODO: Describe this parameter.
        obj: TODO: Describe this parameter.
    """
    for selected in tuple(getattr(context, "selected_objects", ())):
        selected.select_set(False)
    obj.select_set(True)
    context.view_layer.objects.active = obj


def _new_entity_object(context, classname: str):
    """TODO: Describe `_new_entity_object`.

    Args:
        context: TODO: Describe this parameter.
        classname: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    object_type = native_object_type(classname)
    name = display_name(classname)
    if object_type == "MESH":
        bpy.ops.mesh.primitive_cube_add(
            size=1.0, location=context.scene.cursor.location)
        obj = context.active_object
    elif object_type == "LIGHT":
        data = bpy.data.lights.new(name, type="POINT")
        obj = bpy.data.objects.new(name, data)
        context.collection.objects.link(obj)
        obj.location = context.scene.cursor.location
        _select_only(context, obj)
    elif object_type == "CAMERA":
        data = bpy.data.cameras.new(name)
        obj = bpy.data.objects.new(name, data)
        context.collection.objects.link(obj)
        obj.location = context.scene.cursor.location
        _select_only(context, obj)
    else:
        obj = bpy.data.objects.new(name, None)
        context.collection.objects.link(obj)
        obj.location = context.scene.cursor.location
        obj.empty_display_type = {
            "skybox": "SPHERE",
            "exponential_height_fog": "CUBE",
            "post_process": "CIRCLE",
            "physics_constraint": "ARROWS",
        }.get(classname, "PLAIN_AXES")
        obj.empty_display_size = 1.0
        _select_only(context, obj)
    obj.name = name
    return obj


def _apply_native_defaults(obj, schema: dict[str, object]) -> None:
    """TODO: Describe `_apply_native_defaults`.

    Args:
        obj: TODO: Describe this parameter.
        schema: TODO: Describe this parameter.
    """
    classname = str(schema["classname"])
    fields = {str(field["name"]): field for field in schema["fields"]}
    if classname == "light" and obj.type == "LIGHT":
        data = obj.data
        data.color = tuple(fields["color"]["default"])
        data.energy = float(fields["intensity"]["default"])
        data.cutoff_distance = float(fields["range"]["default"])
        data.use_shadow = True
    elif classname == "player" and obj.type == "CAMERA":
        data = obj.data
        data.angle_y = float(fields["vertical_fov_radians"]["default"])
        data.clip_start = float(fields["near_clip"]["default"])
        data.clip_end = float(fields["far_clip"]["default"])


def _draw_id_property(layout, obj, key: str, label: str) -> None:
    """TODO: Describe `_draw_id_property`.

    Args:
        layout: TODO: Describe this parameter.
        obj: TODO: Describe this parameter.
        key: TODO: Describe this parameter.
        label: TODO: Describe this parameter.
    """
    if key not in obj:
        row = layout.row()
        row.alert = True
        row.label(text=f"{label}: not initialized", icon="ERROR")
        return
    layout.prop(obj, f'["{key}"]', text=label)


def _draw_enum(layout, obj, field: dict[str, object]) -> None:
    """TODO: Describe `_draw_enum`.

    Args:
        layout: TODO: Describe this parameter.
        obj: TODO: Describe this parameter.
        field: TODO: Describe this parameter.
    """
    key = property_name(field)
    current = str(obj.get(key, ""))
    row = layout.row(align=True)
    row.label(text=display_name(str(field["name"])))
    row.label(text=display_name(current) if current else "Not set")
    operator = row.operator(
        CENGINE_OT_set_enum_value.bl_idname, text="", icon="DOWNARROW_HLT")
    operator.property_key = key


def _draw_asset(layout, obj, field: dict[str, object]) -> None:
    """TODO: Describe `_draw_asset`.

    Args:
        layout: TODO: Describe this parameter.
        obj: TODO: Describe this parameter.
        field: TODO: Describe this parameter.
    """
    key = property_name(field)
    row = layout.row(align=True)
    if key in obj:
        row.prop(obj, f'["{key}"]', text=display_name(str(field["name"])))
    else:
        row.label(text=f"{display_name(str(field['name']))}: not initialized")
    operator = row.operator(
        CENGINE_OT_choose_asset.bl_idname, text="", icon="FILE_FOLDER")
    operator.property_key = key
    operator.asset_type = str(field.get("asset_type", "asset"))


def _draw_object_reference(
    layout, obj, key: str, label: str
) -> None:
    """TODO: Describe `_draw_object_reference`.

    Args:
        layout: TODO: Describe this parameter.
        obj: TODO: Describe this parameter.
        key: TODO: Describe this parameter.
        label: TODO: Describe this parameter.
    """
    if key not in obj:
        row = layout.row()
        row.alert = True
        row.label(text=f"{label}: not initialized", icon="ERROR")
        return
    layout.prop_search(
        obj, f'["{key}"]', bpy.data, "objects",
        text=label)


def _draw_entity_fields(layout, obj, schema: dict[str, object]) -> None:
    """TODO: Describe `_draw_entity_fields`.

    Args:
        layout: TODO: Describe this parameter.
        obj: TODO: Describe this parameter.
        schema: TODO: Describe this parameter.
    """
    classname = str(schema["classname"])
    if classname == "prop":
        layout.prop(obj, "hide_render", text="Hidden in Engine")
        row = layout.row(align=True)
        row.label(text="Collider")
        collider = str(obj.get("ce_collider", "Box"))
        row.label(text=display_name(collider))
        operator = row.operator(
            CENGINE_OT_set_enum_value.bl_idname,
            text="", icon="DOWNARROW_HLT")
        operator.property_key = "ce_collider"
        _draw_object_reference(
            layout, obj, "ce_collision_object", "Collision Object")
        layout.operator(
            CENGINE_OT_preview_collider.bl_idname,
            text="Toggle Collider Preview", icon="SHADING_WIRE")
    elif classname == "light":
        layout.prop(obj.data, "type")
        layout.prop(obj.data, "color")
        layout.prop(obj.data, "energy", text="Intensity")
        if obj.data.type != "SUN":
            layout.prop(obj.data, "cutoff_distance", text="Range")
        if obj.data.type == "SPOT":
            layout.prop(obj.data, "spot_size", text="Outer Angle")
            layout.prop(obj.data, "spot_blend")
        if obj.data.type == "AREA":
            layout.prop(obj.data, "shape")
            layout.prop(obj.data, "size")
            if hasattr(obj.data, "size_y"):
                layout.prop(obj.data, "size_y")
        layout.prop(obj.data, "use_shadow", text="Cast Shadows")
        layout.prop(obj, "hide_render", text="Disabled")
    elif classname == "player" and obj.type == "CAMERA":
        layout.prop(obj.data, "angle_y", text="Vertical Field of View")
        layout.prop(obj.data, "clip_start", text="Near Clip")
        layout.prop(obj.data, "clip_end", text="Far Clip")

    hidden_native_fields = {
        "prop": {
            "mesh", "materials", "lightmap", "lightmap_scale",
            "lightmap_offset", "lightmap_rgbm_range",
        },
        "light": {
            "type", "color", "intensity", "range", "inner_angle_radians",
            "outer_angle_radians", "area_size",
        },
        "player": {"vertical_fov_radians", "near_clip", "far_clip"},
    }.get(classname, set())
    hidden_native_flags = {
        "prop": {"visible"},
        "light": {"enabled", "casts_shadows"},
    }.get(classname, set())
    for field in schema["fields"]:
        field_type = str(field["type"])
        if field_type == "transform" or str(field["name"]) in hidden_native_fields:
            continue
        if field_type == "flags":
            for member in field["members"]:
                if str(member["name"]) in hidden_native_flags:
                    continue
                _draw_id_property(
                    layout,
                    obj,
                    property_name(member),
                    display_name(str(member["name"])),
                )
        elif field_type == "enum":
            _draw_enum(layout, obj, field)
        elif field_type in ("asset", "asset_list"):
            _draw_asset(layout, obj, field)
        elif field_type == "entity":
            _draw_object_reference(
                layout, obj, property_name(field),
                display_name(str(field["name"])))
        else:
            _draw_id_property(
                layout, obj, property_name(field),
                display_name(str(field["name"])))


def _enum_value_items(operator, context):
    """TODO: Describe `_enum_value_items`.

    Args:
        operator: TODO: Describe this parameter.
        context: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    if operator.property_key == "ce_collider":
        return tuple(
            (value, display_name(value), f"Cook a {display_name(value)} collider")
            for value in (
                "Box", "Sphere", "Capsule", "Cylinder",
                "Convex_Hull", "Triangle_Mesh", "Height_Field",
                "Compound", "Plane",
            )
        )
    obj = getattr(context, "active_object", None)
    if obj is None:
        return ()
    schema = entity_schema(exporter.GAME_SCHEMA, obj)
    if schema is None:
        return ()
    descriptor = field_by_property(schema, operator.property_key)
    if descriptor is None:
        return ()
    key = (str(schema["classname"]), operator.property_key)
    return _ENUM_ITEM_CACHE.setdefault(key, enum_items(descriptor[0]))


if bpy is not None:

    class CENGINE_PG_scene_settings(bpy.types.PropertyGroup):
        """TODO: Describe `CENGINE_PG_scene_settings`."""

        output_root: bpy.props.StringProperty(  # type: ignore[valid-type]
            name="Output Root",
            description=(
                "Folder that receives cooked assets; saved files under "
                "assets/source default to assets/compiled"
            ),
            subtype="DIR_PATH",
            default="",
        )
        dds_format: bpy.props.EnumProperty(  # type: ignore[valid-type]
            name="Texture Format",
            items=(
                ("DXT5", "DXT5", "RGBA textures"),
                ("DXT1", "DXT1", "Opaque color textures"),
                ("BC5", "BC5", "Two-channel normal maps"),
            ),
            default="DXT5",
        )
        lightmap_resolution: bpy.props.IntProperty(  # type: ignore[valid-type]
            name="Resolution", default=DEFAULT_RESOLUTION, min=128, max=16384)
        lightmap_padding: bpy.props.IntProperty(  # type: ignore[valid-type]
            name="Padding", default=DEFAULT_PADDING, min=1, max=128)
        lightmap_samples: bpy.props.IntProperty(  # type: ignore[valid-type]
            name="Samples", default=DEFAULT_SAMPLES, min=1, max=16384)
        lightmap_denoise: bpy.props.BoolProperty(  # type: ignore[valid-type]
            name="Denoise", default=DEFAULT_DENOISE)
        lightmap_include_direct: bpy.props.BoolProperty(  # type: ignore[valid-type]
            name="Include Direct Lighting", default=DEFAULT_INCLUDE_DIRECT)


    class CENGINE_OT_export_assets(bpy.types.Operator):
        """TODO: Describe `CENGINE_OT_export_assets`."""

        bl_idname = "cengine.export_assets"
        bl_label = "Export CEngine Asset"
        bl_description = "Cook the active asset collection without baking lightmaps"
        bl_options = {"REGISTER"}

        collection_only: bpy.props.BoolProperty(  # type: ignore[valid-type]
            name="Collection Only", default=False)

        def invoke(self, context, event):
            """TODO: Describe `invoke`.

            Args:
                context: TODO: Describe this parameter.
                event: TODO: Describe this parameter.

            Returns:
                TODO: Describe the produced value.
            """
            del event
            settings = _settings(context)
            if not settings.output_root:
                default_root = _output_root(context)
                if default_root is not None:
                    settings.output_root = str(default_root)
            return self.execute(context)

        def execute(self, context):
            """TODO: Describe `execute`.

            Args:
                context: TODO: Describe this parameter.

            Returns:
                TODO: Describe the produced value.
            """
            root = _output_root(context)
            try:
                result = exporter.run_export(
                    root, _settings(context).dds_format, self.collection_only)
            except (OSError, RuntimeError, ValueError) as error:
                self.report({"ERROR"}, str(error))
                return {"CANCELLED"}
            self.report({"INFO"}, exporter.export_summary(result, root))
            return {"FINISHED"}


    class CENGINE_OT_set_collection_type(bpy.types.Operator):
        """TODO: Describe `CENGINE_OT_set_collection_type`."""

        bl_idname = "cengine.set_collection_type"
        bl_label = "Set CEngine Asset Type"
        bl_options = {"REGISTER", "UNDO"}

        asset_type: bpy.props.EnumProperty(  # type: ignore[valid-type]
            name="Asset Type",
            items=(
                ("scene", "Scene", "Cook a complete .cscene"),
                ("prefab", "Model / Prefab", "Cook a reusable .casset"),
            ),
            default="prefab",
        )

        def execute(self, context):
            """TODO: Describe `execute`.

            Args:
                context: TODO: Describe this parameter.

            Returns:
                TODO: Describe the produced value.
            """
            collection = _active_collection(context)
            collection["ce_asset_type"] = self.asset_type
            if "ce_asset_name" not in collection:
                collection["ce_asset_name"] = collection.name
            return {"FINISHED"}


    class CENGINE_OT_add_entity(bpy.types.Operator):
        """TODO: Describe `CENGINE_OT_add_entity`."""

        bl_idname = "cengine.add_entity"
        bl_label = "Add CEngine Entity"
        bl_options = {"REGISTER", "UNDO"}

        entity_type: bpy.props.EnumProperty(  # type: ignore[valid-type]
            name="Entity",
            items=tuple(
                (
                    str(schema["classname"]),
                    display_name(str(schema["classname"])),
                    f"Add a {display_name(str(schema['classname']))} entity",
                )
                for schema in exporter.GAME_SCHEMA.entities
            ),
        )

        def execute(self, context):
            """TODO: Describe `execute`.

            Args:
                context: TODO: Describe this parameter.

            Returns:
                TODO: Describe the produced value.
            """
            schema = exporter.GAME_SCHEMA.entity(self.entity_type)
            if schema is None:
                self.report({"ERROR"}, f"Unknown entity type: {self.entity_type}")
                return {"CANCELLED"}
            obj = _new_entity_object(context, self.entity_type)
            initialize_entity_properties(obj, schema)
            _apply_native_defaults(obj, schema)
            self.report({"INFO"}, f"Added {display_name(self.entity_type)}")
            return {"FINISHED"}


    class CENGINE_MT_add_entity(bpy.types.Menu):
        """TODO: Describe `CENGINE_MT_add_entity`."""

        bl_idname = "CENGINE_MT_add_entity"
        bl_label = "CEngine Entity"

        def draw(self, _context):
            """TODO: Describe `draw`.

            Args:
                _context: TODO: Describe this parameter.
            """
            layout = self.layout
            for schema in exporter.GAME_SCHEMA.entities:
                classname = str(schema["classname"])
                operator = layout.operator(
                    CENGINE_OT_add_entity.bl_idname,
                    text=display_name(classname),
                    icon={
                        "prop": "MESH_CUBE",
                        "light": "LIGHT",
                        "player": "CAMERA_DATA",
                    }.get(classname, "EMPTY_AXIS"),
                )
                operator.entity_type = classname


    class CENGINE_OT_initialize_entity(bpy.types.Operator):
        """TODO: Describe `CENGINE_OT_initialize_entity`."""

        bl_idname = "cengine.initialize_entity"
        bl_label = "Initialize Entity Settings"
        bl_options = {"REGISTER", "UNDO"}

        def execute(self, context):
            """TODO: Describe `execute`.

            Args:
                context: TODO: Describe this parameter.

            Returns:
                TODO: Describe the produced value.
            """
            obj = context.active_object
            if obj is None:
                return {"CANCELLED"}
            schema = entity_schema(exporter.GAME_SCHEMA, obj)
            if schema is None:
                self.report({"ERROR"}, "The active object is not a known CEngine entity")
                return {"CANCELLED"}
            initialize_entity_properties(obj, schema, overwrite_classname=False)
            return {"FINISHED"}


    class CENGINE_OT_initialize_material(bpy.types.Operator):
        """TODO: Describe `CENGINE_OT_initialize_material`."""

        bl_idname = "cengine.initialize_material"
        bl_label = "Initialize CEngine Material"
        bl_options = {"REGISTER", "UNDO"}

        def execute(self, context):
            """TODO: Describe `execute`.

            Args:
                context: TODO: Describe this parameter.

            Returns:
                TODO: Describe the produced value.
            """
            material = getattr(context, "material", None)
            if material is None:
                return {"CANCELLED"}
            material.use_nodes = True
            if "ce_shader" not in material:
                material["ce_shader"] = "pbr"
            if "ce_ao" not in material:
                material["ce_ao"] = 1.0
                try:
                    material.id_properties_ui("ce_ao").update(
                        min=0.0,
                        max=1.0,
                        description="Ambient occlusion multiplier",
                    )
                except (KeyError, TypeError, RuntimeError):
                    pass
            return {"FINISHED"}


    class CENGINE_OT_set_enum_value(bpy.types.Operator):
        """TODO: Describe `CENGINE_OT_set_enum_value`."""

        bl_idname = "cengine.set_enum_value"
        bl_label = "Choose Value"
        bl_options = {"REGISTER", "UNDO"}

        property_key: bpy.props.StringProperty()  # type: ignore[valid-type]
        value: bpy.props.EnumProperty(  # type: ignore[valid-type]
            name="Value",
            items=_enum_value_items,
        )

        def invoke(self, context, event):
            """TODO: Describe `invoke`.

            Args:
                context: TODO: Describe this parameter.
                event: TODO: Describe this parameter.

            Returns:
                TODO: Describe the produced value.
            """
            del event
            current = str(context.active_object.get(self.property_key, ""))
            if current:
                try:
                    self.value = current
                except TypeError:
                    pass
            return context.window_manager.invoke_props_dialog(self)

        def execute(self, context):
            """TODO: Describe `execute`.

            Args:
                context: TODO: Describe this parameter.

            Returns:
                TODO: Describe the produced value.
            """
            context.active_object[self.property_key] = self.value
            return {"FINISHED"}


    class CENGINE_OT_choose_asset(bpy.types.Operator):
        """TODO: Describe `CENGINE_OT_choose_asset`."""

        bl_idname = "cengine.choose_asset"
        bl_label = "Choose Asset"
        bl_options = {"REGISTER", "UNDO"}

        property_key: bpy.props.StringProperty()  # type: ignore[valid-type]
        asset_type: bpy.props.StringProperty()  # type: ignore[valid-type]
        filepath: bpy.props.StringProperty(subtype="FILE_PATH")  # type: ignore[valid-type]
        filter_glob: bpy.props.StringProperty(  # type: ignore[valid-type]
            default="*.hdr;*.exr;*.dds;*.cmat;*.cmesh;*.casset;*.cscene",
            options={"HIDDEN"},
        )

        def invoke(self, context, event):
            """TODO: Describe `invoke`.

            Args:
                context: TODO: Describe this parameter.
                event: TODO: Describe this parameter.

            Returns:
                TODO: Describe the produced value.
            """
            del event
            context.window_manager.fileselect_add(self)
            return {"RUNNING_MODAL"}

        def execute(self, context):
            """TODO: Describe `execute`.

            Args:
                context: TODO: Describe this parameter.

            Returns:
                TODO: Describe the produced value.
            """
            obj = context.active_object
            if obj is None:
                return {"CANCELLED"}
            obj[self.property_key] = bpy.path.relpath(self.filepath)
            return {"FINISHED"}


    class CENGINE_OT_prepare_lightmaps(bpy.types.Operator):
        """TODO: Describe `CENGINE_OT_prepare_lightmaps`."""

        bl_idname = "cengine.prepare_lightmaps"
        bl_label = "Prepare Lightmap UVs"
        bl_description = "Create or rebuild the Lightmap UV layer without rendering"
        bl_options = {"REGISTER", "UNDO"}

        def execute(self, context):
            """TODO: Describe `execute`.

            Args:
                context: TODO: Describe this parameter.

            Returns:
                TODO: Describe the produced value.
            """
            try:
                collection, _spec = _scene_collection(context)
                objects = exporter.exported_collection_objects([collection])
                settings = _lightmap_settings(context)
                ensure_lightmap_uvs(
                    bpy,
                    objects,
                    settings.resolution,
                    settings.padding,
                )
            except (OSError, RuntimeError, ValueError) as error:
                self.report({"ERROR"}, str(error))
                return {"CANCELLED"}
            self.report({"INFO"}, "Lightmap UVs are ready")
            return {"FINISHED"}


    class CENGINE_OT_bake_lightmaps(bpy.types.Operator):
        """TODO: Describe `CENGINE_OT_bake_lightmaps`."""

        bl_idname = "cengine.bake_lightmaps"
        bl_label = "Bake Lightmaps"
        bl_description = "Render and save lightmaps now; later exports reuse them"
        bl_options = {"REGISTER"}

        def execute(self, context):
            """TODO: Describe `execute`.

            Args:
                context: TODO: Describe this parameter.

            Returns:
                TODO: Describe the produced value.
            """
            source = exporter.maybe_blend_source_path()
            root = _output_root(context)
            if source is None or root is None:
                self.report(
                    {"ERROR"}, "Save the Blender file and choose an output root first")
                return {"CANCELLED"}
            try:
                collection, spec = _scene_collection(context)
                objects = exporter.exported_collection_objects([collection])
                placements, _prefabs, outputs = bake_scene_lightmaps(
                    bpy, source, root, objects, spec.asset_name, {},
                    exporter.log,
                    _lightmap_settings(context))
                if not outputs:
                    self.report(
                        {"WARNING"},
                        "Nothing to bake; add a baked or mixed light and a static mesh",
                    )
                    return {"CANCELLED"}
                store_lightmap_bindings(
                    objects, placements,
                    lambda path: bpy.path.relpath(str(path)))
                project_root = (
                    exporter.project_root_for_output_root(root)
                    or exporter.project_root_for(source)
                )
                exporter.register_outputs(
                    project_root, source, outputs, exporter.log,
                    exporter.source_fingerprint(source))
            except (OSError, RuntimeError, ValueError) as error:
                self.report({"ERROR"}, str(error))
                return {"CANCELLED"}
            self.report(
                {"INFO"},
                f"Baked {len(placements)} object binding(s) to {len(outputs)} atlas file(s)",
            )
            return {"FINISHED"}


    class CENGINE_OT_clear_lightmaps(bpy.types.Operator):
        """TODO: Describe `CENGINE_OT_clear_lightmaps`."""

        bl_idname = "cengine.clear_lightmaps"
        bl_label = "Clear Lightmap Bindings"
        bl_description = "Detach baked lightmaps without deleting files or UV layers"
        bl_options = {"REGISTER", "UNDO"}

        def execute(self, context):
            """TODO: Describe `execute`.

            Args:
                context: TODO: Describe this parameter.

            Returns:
                TODO: Describe the produced value.
            """
            try:
                collection, _spec = _scene_collection(context)
                objects = exporter.exported_collection_objects([collection])
                count = clear_lightmap_bindings(objects)
            except RuntimeError as error:
                self.report({"ERROR"}, str(error))
                return {"CANCELLED"}
            self.report({"INFO"}, f"Cleared {count} lightmap binding(s)")
            return {"FINISHED"}


    class CENGINE_OT_validate_asset(bpy.types.Operator):
        """TODO: Describe `CENGINE_OT_validate_asset`."""

        bl_idname = "cengine.validate_asset"
        bl_label = "Validate Asset"
        bl_options = {"REGISTER"}

        def execute(self, context):
            """TODO: Describe `execute`.

            Args:
                context: TODO: Describe this parameter.

            Returns:
                TODO: Describe the produced value.
            """
            try:
                collection = _active_collection(context)
                objects = exporter.exported_collection_objects([collection])
                issues = validate_entities(objects, exporter.GAME_SCHEMA)
            except (RuntimeError, ValueError) as error:
                self.report({"ERROR"}, str(error))
                return {"CANCELLED"}
            if issues:
                self.report(
                    {"ERROR"},
                    f"{len(issues)} issue(s); first: {issues[0]}")
                return {"CANCELLED"}
            self.report({"INFO"}, "Asset is ready to export")
            return {"FINISHED"}


    class CENGINE_OT_preview_collider(bpy.types.Operator):
        """TODO: Describe `CENGINE_OT_preview_collider`."""

        bl_idname = "cengine.preview_collider"
        bl_label = "Toggle Collider Preview"
        bl_options = {"REGISTER", "UNDO"}

        def execute(self, context):
            """TODO: Describe `execute`.

            Args:
                context: TODO: Describe this parameter.

            Returns:
                TODO: Describe the produced value.
            """
            obj = context.active_object
            if obj is None or entity_classname(obj) != "prop":
                return {"CANCELLED"}
            collision_name = str(
                obj.get("ce_collision_object", "") or "")
            preview = bpy.data.objects.get(collision_name) \
                if collision_name else obj
            if preview is None or preview.type != "MESH":
                self.report({"ERROR"}, "Collision Object must name a mesh")
                return {"CANCELLED"}
            enabled = not bool(getattr(preview, "show_wire", False))
            preview.show_wire = enabled
            preview.show_all_edges = enabled
            collider = str(obj.get("ce_collider", "Box")).lower()
            if collider in {"box", "sphere", "capsule", "cylinder"}:
                preview.show_bounds = enabled
                preview.display_bounds_type = {
                    "box": "BOX",
                    "sphere": "SPHERE",
                    "capsule": "CAPSULE",
                    "cylinder": "CYLINDER",
                }[collider]
            self.report(
                {"INFO"},
                "Collider preview enabled" if enabled
                else "Collider preview disabled")
            return {"FINISHED"}


    class CENGINE_OT_preview_entity(bpy.types.Operator):
        """TODO: Describe `CENGINE_OT_preview_entity`."""

        bl_idname = "cengine.preview_entity"
        bl_label = "Preview in Blender"
        bl_options = {"REGISTER", "UNDO"}

        def execute(self, context):
            """TODO: Describe `execute`.

            Args:
                context: TODO: Describe this parameter.

            Returns:
                TODO: Describe the produced value.
            """
            obj = context.active_object
            classname = entity_classname(obj) if obj is not None else ""
            if classname == "player" and obj.type == "CAMERA":
                context.scene.camera = obj
                self.report({"INFO"}, "The player camera is now the scene camera")
                return {"FINISHED"}
            if classname == "post_process":
                exposure = max(1.0e-6, float(obj.get("ce_exposure", 1.0)))
                context.scene.view_settings.exposure = math.log2(exposure)
                self.report({"INFO"}, "Applied post-process exposure to Blender")
                return {"FINISHED"}
            if classname == "skybox":
                path = str(obj.get("ce_panorama", "") or "")
                if not path:
                    self.report({"ERROR"}, "Choose a panorama first")
                    return {"CANCELLED"}
                source = Path(bpy.path.abspath(path)).absolute()
                if not source.is_file():
                    self.report({"ERROR"}, f"Panorama does not exist: {source}")
                    return {"CANCELLED"}
                world = context.scene.world or bpy.data.worlds.new("CEngine World")
                context.scene.world = world
                world.use_nodes = True
                nodes = world.node_tree.nodes
                background = next(
                    (node for node in nodes if node.type == "BACKGROUND"), None)
                output = next(
                    (node for node in nodes if node.type == "OUTPUT_WORLD"), None)
                if background is None:
                    background = nodes.new("ShaderNodeBackground")
                if output is None:
                    output = nodes.new("ShaderNodeOutputWorld")
                environment = nodes.get("CEngine Skybox Preview")
                if environment is None:
                    environment = nodes.new("ShaderNodeTexEnvironment")
                    environment.name = "CEngine Skybox Preview"
                environment.image = bpy.data.images.load(
                    str(source), check_existing=True)
                background.inputs["Strength"].default_value = float(
                    obj.get("ce_intensity", 1.0))
                links = world.node_tree.links
                links.new(environment.outputs["Color"], background.inputs["Color"])
                links.new(background.outputs["Background"], output.inputs["Surface"])
                self.report({"INFO"}, "Applied the skybox to Blender's world")
                return {"FINISHED"}
            self.report({"INFO"}, "This entity already uses Blender's native preview")
            return {"FINISHED"}


    class CENGINE_PT_asset_workspace(bpy.types.Panel):
        """TODO: Describe `CENGINE_PT_asset_workspace`."""

        bl_label = "CEngine Asset"
        bl_idname = "CENGINE_PT_asset_workspace"
        bl_space_type = "VIEW_3D"
        bl_region_type = "UI"
        bl_category = "CEngine"

        def draw(self, context):
            """TODO: Describe `draw`.

            Args:
                context: TODO: Describe this parameter.
            """
            layout = self.layout
            collection = getattr(context, "collection", None)
            if collection is None:
                layout.label(text="No active collection", icon="ERROR")
                return
            layout.label(text=f"Asset: {collection.name}", icon="OUTLINER_COLLECTION")
            asset_type = str(collection.get("ce_asset_type", "") or "")
            row = layout.row(align=True)
            scene = row.operator(
                CENGINE_OT_set_collection_type.bl_idname,
                text="Scene", depress=asset_type == "scene")
            scene.asset_type = "scene"
            prefab = row.operator(
                CENGINE_OT_set_collection_type.bl_idname,
                text="Model / Prefab", depress=asset_type == "prefab")
            prefab.asset_type = "prefab"
            if "ce_asset_name" in collection:
                layout.prop(collection, '["ce_asset_name"]', text="Asset Name")
            layout.menu(
                CENGINE_MT_add_entity.bl_idname,
                text="Add Entity",
                icon="ADD",
            )
            layout.prop(_settings(context), "output_root")
            layout.prop(_settings(context), "dds_format")
            row = layout.row(align=True)
            row.operator(CENGINE_OT_validate_asset.bl_idname, icon="CHECKMARK")
            row.operator(CENGINE_OT_export_assets.bl_idname, icon="EXPORT")


    class CENGINE_PT_lightmaps(bpy.types.Panel):
        """TODO: Describe `CENGINE_PT_lightmaps`."""

        bl_label = "Lightmaps"
        bl_idname = "CENGINE_PT_lightmaps"
        bl_space_type = "VIEW_3D"
        bl_region_type = "UI"
        bl_category = "CEngine"
        bl_parent_id = "CENGINE_PT_asset_workspace"

        @classmethod
        def poll(cls, context):
            """TODO: Describe `poll`.

            Args:
                context: TODO: Describe this parameter.

            Returns:
                TODO: Describe the produced value.
            """
            collection = getattr(context, "collection", None)
            if collection is None:
                return False
            try:
                spec = collection_export_spec(collection, AssetType.PREFAB)
            except RuntimeError:
                return False
            return spec is not None and spec.asset_type == AssetType.SCENE

        def draw(self, context):
            """TODO: Describe `draw`.

            Args:
                context: TODO: Describe this parameter.
            """
            layout = self.layout
            settings = _settings(context)
            layout.label(text="Bake once, export many times.")
            layout.prop(settings, "lightmap_resolution")
            layout.prop(settings, "lightmap_padding")
            layout.prop(settings, "lightmap_samples")
            layout.prop(settings, "lightmap_denoise")
            layout.prop(settings, "lightmap_include_direct")
            layout.operator(
                CENGINE_OT_prepare_lightmaps.bl_idname, icon="GROUP_UVS")
            layout.operator(
                CENGINE_OT_bake_lightmaps.bl_idname, icon="RENDER_STILL")
            layout.operator(
                CENGINE_OT_clear_lightmaps.bl_idname, icon="X")


    class CENGINE_PT_entity(bpy.types.Panel):
        """TODO: Describe `CENGINE_PT_entity`."""

        bl_label = "CEngine Entity"
        bl_idname = "CENGINE_PT_entity"
        bl_space_type = "PROPERTIES"
        bl_region_type = "WINDOW"
        bl_context = "object"

        @classmethod
        def poll(cls, context):
            """TODO: Describe `poll`.

            Args:
                context: TODO: Describe this parameter.

            Returns:
                TODO: Describe the produced value.
            """
            return context.active_object is not None

        def draw(self, context):
            """TODO: Describe `draw`.

            Args:
                context: TODO: Describe this parameter.
            """
            layout = self.layout
            obj = context.active_object
            schema = entity_schema(exporter.GAME_SCHEMA, obj)
            if schema is None:
                layout.label(text="Not a CEngine entity")
                layout.menu(
                    CENGINE_MT_add_entity.bl_idname, text="Add Entity",
                    icon="ADD")
                return
            classname = str(schema["classname"])
            layout.label(
                text=display_name(classname),
                icon={
                    "prop": "MESH_DATA",
                    "light": "LIGHT_DATA",
                    "player": "CAMERA_DATA",
                }.get(classname, "EMPTY_DATA"),
            )
            if CLASSNAME_PROPERTY not in obj:
                layout.label(
                    text="Using native Blender entity inference",
                    icon="INFO")
            layout.operator(
                CENGINE_OT_initialize_entity.bl_idname,
                text="Initialize Missing Settings",
                icon="FILE_REFRESH",
            )
            _draw_entity_fields(layout, obj, schema)
            if classname in {"player", "skybox", "post_process"}:
                layout.operator(
                    CENGINE_OT_preview_entity.bl_idname, icon="HIDE_OFF")


    class CENGINE_PT_material(bpy.types.Panel):
        """TODO: Describe `CENGINE_PT_material`."""

        bl_label = "CEngine Material"
        bl_idname = "CENGINE_PT_material"
        bl_space_type = "PROPERTIES"
        bl_region_type = "WINDOW"
        bl_context = "material"

        @classmethod
        def poll(cls, context):
            """TODO: Describe `poll`.

            Args:
                context: TODO: Describe this parameter.

            Returns:
                TODO: Describe the produced value.
            """
            return getattr(context, "material", None) is not None

        def draw(self, context):
            """TODO: Describe `draw`.

            Args:
                context: TODO: Describe this parameter.
            """
            layout = self.layout
            material = context.material
            layout.label(
                text="Author with Blender's Principled BSDF",
                icon="MATERIAL",
            )
            layout.operator(
                CENGINE_OT_initialize_material.bl_idname,
                text="Initialize Engine Settings",
                icon="FILE_REFRESH",
            )
            if "ce_shader" in material:
                layout.prop(material, '["ce_shader"]', text="Shader")
            if "ce_ao" in material:
                layout.prop(material, '["ce_ao"]', text="AO Multiplier")


    def draw_add_menu(self, _context):
        """TODO: Describe `draw_add_menu`.

        Args:
            _context: TODO: Describe this parameter.
        """
        self.layout.menu(
            CENGINE_MT_add_entity.bl_idname,
            text="CEngine Entity",
            icon="PLUGIN",
        )


    def draw_export_menu(self, _context):
        """TODO: Describe `draw_export_menu`.

        Args:
            _context: TODO: Describe this parameter.
        """
        self.layout.operator(
            CENGINE_OT_export_assets.bl_idname,
            text="CEngine Asset",
        )


    CLASSES = (
        CENGINE_PG_scene_settings,
        CENGINE_OT_export_assets,
        CENGINE_OT_set_collection_type,
        CENGINE_OT_add_entity,
        CENGINE_MT_add_entity,
        CENGINE_OT_initialize_entity,
        CENGINE_OT_initialize_material,
        CENGINE_OT_set_enum_value,
        CENGINE_OT_choose_asset,
        CENGINE_OT_prepare_lightmaps,
        CENGINE_OT_bake_lightmaps,
        CENGINE_OT_clear_lightmaps,
        CENGINE_OT_validate_asset,
        CENGINE_OT_preview_collider,
        CENGINE_OT_preview_entity,
        CENGINE_PT_asset_workspace,
        CENGINE_PT_lightmaps,
        CENGINE_PT_entity,
        CENGINE_PT_material,
    )
else:
    CLASSES = ()


def register() -> None:
    """TODO: Describe `register`."""
    if bpy is None:
        raise RuntimeError("CEngine Asset Exporter must run inside Blender")
    for cls in CLASSES:
        bpy.utils.register_class(cls)
    bpy.types.Scene.cengine_settings = bpy.props.PointerProperty(
        type=CENGINE_PG_scene_settings)
    bpy.types.VIEW3D_MT_add.append(draw_add_menu)
    bpy.types.TOPBAR_MT_file_export.append(draw_export_menu)


def unregister() -> None:
    """TODO: Describe `unregister`."""
    if bpy is None:
        raise RuntimeError("CEngine Asset Exporter must run inside Blender")
    bpy.types.TOPBAR_MT_file_export.remove(draw_export_menu)
    bpy.types.VIEW3D_MT_add.remove(draw_add_menu)
    del bpy.types.Scene.cengine_settings
    for cls in reversed(CLASSES):
        bpy.utils.unregister_class(cls)
