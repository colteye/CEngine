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

import struct
import sys
import tempfile
import unittest
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "blender_addon"))

from cengine_asset_exporter.meshes import (  # noqa: E402
    MESH_FLAG_SKINNED,
    blender_to_engine_vector,
    mesh_buffers,
    mesh_payload,
    mesh_objects,
    mesh_output_path,
    polygon_triangles,
    write_mesh_asset,
    write_mesh_assets,
)
from ceassetlib.game_schema import load_bundled_game  # noqa: E402
from ceassetlib.wire import unpack_record  # noqa: E402


ASSET_HEADER = struct.Struct("<4sHHI16sQ16sQQQ")
GAME = load_bundled_game()


class FakeVertex:
    """TODO: Describe `FakeVertex`."""

    def __init__(self, co: tuple[float, float, float], groups: list[object] | None = None) -> None:
        """TODO: Describe `__init__`.

        Args:
            co: TODO: Describe this parameter.
            groups: TODO: Describe this parameter.
        """
        self.co = co
        self.groups = groups or []


class FakeGroupWeight:
    """TODO: Describe `FakeGroupWeight`."""

    def __init__(self, group: int, weight: float) -> None:
        """TODO: Describe `__init__`.

        Args:
            group: TODO: Describe this parameter.
            weight: TODO: Describe this parameter.
        """
        self.group = group
        self.weight = weight


class FakeLoop:
    """TODO: Describe `FakeLoop`."""

    def __init__(self, vertex_index: int, normal: tuple[float, float, float]) -> None:
        """TODO: Describe `__init__`.

        Args:
            vertex_index: TODO: Describe this parameter.
            normal: TODO: Describe this parameter.
        """
        self.vertex_index = vertex_index
        self.normal = normal


class FakePolygon:
    """TODO: Describe `FakePolygon`."""

    def __init__(self, loop_indices: list[int], material_index: int = 0) -> None:
        """TODO: Describe `__init__`.

        Args:
            loop_indices: TODO: Describe this parameter.
            material_index: TODO: Describe this parameter.
        """
        self.loop_indices = loop_indices
        self.material_index = material_index


class FakeUv:
    """TODO: Describe `FakeUv`."""

    def __init__(self, uv: tuple[float, float]) -> None:
        """TODO: Describe `__init__`.

        Args:
            uv: TODO: Describe this parameter.
        """
        self.uv = uv


class FakeUvLayer:
    """TODO: Describe `FakeUvLayer`."""

    def __init__(self, data: list[FakeUv], name: str = "UVMap", active_render: bool = False) -> None:
        """TODO: Describe `__init__`.

        Args:
            data: TODO: Describe this parameter.
            name: TODO: Describe this parameter.
            active_render: TODO: Describe this parameter.
        """
        self.data = data
        self.name = name
        self.active_render = active_render


class FakeUvLayers:
    """TODO: Describe `FakeUvLayers`."""

    def __init__(self, data: list[FakeUv], lightmap: list[FakeUv] | None = None) -> None:
        """TODO: Describe `__init__`.

        Args:
            data: TODO: Describe this parameter.
            lightmap: TODO: Describe this parameter.
        """
        self.active = FakeUvLayer(data, active_render=True)
        self.layers = [self.active]
        if lightmap is not None:
            layer = FakeUvLayer(lightmap, "CEngineLightmap")
            self.layers.append(layer)

    def get(self, name: str):
        """TODO: Describe `get`.

        Args:
            name: TODO: Describe this parameter.

        Returns:
            TODO: Describe the produced value.
        """
        return next((layer for layer in self.layers if getattr(layer, "name", "") == name), None)

    def __len__(self) -> int:
        """TODO: Describe `__len__`.

        Returns:
            TODO: Describe the produced value.
        """
        return len(self.layers)

    def __getitem__(self, index: int):
        """TODO: Describe `__getitem__`.

        Args:
            index: TODO: Describe this parameter.

        Returns:
            TODO: Describe the produced value.
        """
        return self.layers[index]

    def __iter__(self):
        """TODO: Describe `__iter__`.

        Returns:
            TODO: Describe the produced value.
        """
        return iter(self.layers)


class FakeMesh:
    """TODO: Describe `FakeMesh`."""

    def __init__(
        self,
        polygons: list[FakePolygon] | None = None,
        skinned: bool = False,
        include_quad_vertex: bool = False,
    ) -> None:
        """TODO: Describe `__init__`.

        Args:
            polygons: TODO: Describe this parameter.
            skinned: TODO: Describe this parameter.
            include_quad_vertex: TODO: Describe this parameter.
        """
        self.vertices = [
            FakeVertex((0.0, 0.0, 0.0), [FakeGroupWeight(0, 0.75), FakeGroupWeight(1, 0.25)] if skinned else []),
            FakeVertex((1.0, 0.0, 0.0), [FakeGroupWeight(1, 1.0)] if skinned else []),
            FakeVertex((0.0, 2.0, 0.0), [FakeGroupWeight(0, 1.0)] if skinned else []),
        ]
        if include_quad_vertex:
            self.vertices.append(FakeVertex((1.0, 2.0, 0.0), [FakeGroupWeight(1, 1.0)] if skinned else []))
        self.loops = [
            FakeLoop(0, (0.0, 0.0, 1.0)),
            FakeLoop(1, (0.0, 0.0, 1.0)),
            FakeLoop(2, (0.0, 0.0, 1.0)),
        ]
        if include_quad_vertex:
            self.loops.append(FakeLoop(3, (0.0, 0.0, 1.0)))
        self.polygons = polygons if polygons is not None else [FakePolygon([0, 1, 2])]
        uvs = [FakeUv((0.0, 0.0)), FakeUv((1.0, 0.0)), FakeUv((0.0, 1.0))]
        if include_quad_vertex:
            uvs.append(FakeUv((1.0, 1.0)))
        self.uv_layers = FakeUvLayers(uvs)


class FakeMaterial:
    """TODO: Describe `FakeMaterial`."""

    def __init__(self, name: str) -> None:
        """TODO: Describe `__init__`.

        Args:
            name: TODO: Describe this parameter.
        """
        self.name = name


class FakeSlot:
    """TODO: Describe `FakeSlot`."""

    def __init__(self, material: FakeMaterial) -> None:
        """TODO: Describe `__init__`.

        Args:
            material: TODO: Describe this parameter.
        """
        self.material = material


class FakeVertexGroup:
    """TODO: Describe `FakeVertexGroup`."""

    def __init__(self, name: str) -> None:
        """TODO: Describe `__init__`.

        Args:
            name: TODO: Describe this parameter.
        """
        self.name = name


class FakeBone:
    """TODO: Describe `FakeBone`."""

    def __init__(self, name: str) -> None:
        """TODO: Describe `__init__`.

        Args:
            name: TODO: Describe this parameter.
        """
        self.name = name


class FakeArmatureData:
    """TODO: Describe `FakeArmatureData`."""

    def __init__(self) -> None:
        """TODO: Describe `__init__`."""
        self.bones = [FakeBone("root"), FakeBone("spine")]


class FakeArmature:
    """TODO: Describe `FakeArmature`."""

    def __init__(self, name: str = "ARM_Hero") -> None:
        """TODO: Describe `__init__`.

        Args:
            name: TODO: Describe this parameter.
        """
        self.name = name
        self.type = "ARMATURE"
        self.data = FakeArmatureData()


class FakeModifier:
    """TODO: Describe `FakeModifier`."""

    def __init__(self, armature: FakeArmature) -> None:
        """TODO: Describe `__init__`.

        Args:
            armature: TODO: Describe this parameter.
        """
        self.type = "ARMATURE"
        self.object = armature


class FakeObject:
    """TODO: Describe `FakeObject`."""

    def __init__(
        self,
        name: str,
        obj_type: str,
        mesh: FakeMesh | None = None,
        armature: FakeArmature | None = None,
        material_names: list[str] | None = None,
    ) -> None:
        """TODO: Describe `__init__`.

        Args:
            name: TODO: Describe this parameter.
            obj_type: TODO: Describe this parameter.
            mesh: TODO: Describe this parameter.
            armature: TODO: Describe this parameter.
            material_names: TODO: Describe this parameter.
        """
        self.name = name
        self.type = obj_type
        self.data = mesh or FakeMesh()
        if material_names is None:
            material_names = ["HeroSkin"]
        self.material_slots = [FakeSlot(FakeMaterial(name)) for name in material_names]
        self.vertex_groups = [FakeVertexGroup("root"), FakeVertexGroup("spine")]
        self.modifiers = [FakeModifier(armature)] if armature is not None else []
        self.parent = None


class BlenderMeshesTests(unittest.TestCase):
    """TODO: Describe `BlenderMeshesTests`."""

    def test_mesh_output_path_is_sanitized_cmesh(self) -> None:
        """TODO: Describe `test_mesh_output_path_is_sanitized_cmesh`."""
        output = mesh_output_path(Path("hero.blend"), Path("compiled"), "SM Hero")

        self.assertEqual(output, Path("compiled/hero/meshes/SM_Hero.cmesh"))

    def test_mesh_objects_are_unique_and_sorted(self) -> None:
        """TODO: Describe `test_mesh_objects_are_unique_and_sorted`."""
        first = FakeObject("SM_B", "MESH")
        second = FakeObject("SM_A", "MESH")
        armature = FakeObject("ARM_Hero", "ARMATURE")

        meshes = mesh_objects([first, armature, second, first])

        self.assertEqual([obj.name for obj in meshes], ["SM_A", "SM_B"])

    def test_mesh_buffers_pack_triangle_vertices_and_indices(self) -> None:
        """TODO: Describe `test_mesh_buffers_pack_triangle_vertices_and_indices`."""
        buffers = mesh_buffers(FakeMesh())

        self.assertEqual(buffers.vertex_count, 3)
        self.assertEqual(buffers.index_count, 3)
        self.assertEqual(buffers.bounds_min, (0.0, -1.0, 0.0))
        self.assertEqual(buffers.bounds_max, (2.0, -0.0, 0.0))
        self.assertFalse(buffers.skinned)
        self.assertEqual(buffers.vertices[0]["position"], (0.0, -0.0, 0.0))
        self.assertEqual(buffers.vertices[0]["normal"], (0.0, -0.0, 1.0))
        self.assertEqual(buffers.indices, (0, 1, 2))

    def test_blender_to_engine_vector_maps_to_x_forward_y_left_z_up(self) -> None:
        """TODO: Describe `test_blender_to_engine_vector_maps_to_x_forward_y_left_z_up`."""
        self.assertEqual(blender_to_engine_vector((1.0, 2.0, 3.0)), (2.0, -1.0, 3.0))

    def test_named_lightmap_uv_layer_is_packed_as_uv1(self) -> None:
        """TODO: Describe `test_named_lightmap_uv_layer_is_packed_as_uv1`."""
        mesh = FakeMesh()
        mesh.uv_layers = FakeUvLayers(mesh.uv_layers.active.data, [
            FakeUv((0.25, 0.5)), FakeUv((0.75, 0.5)), FakeUv((0.25, 1.0))])

        buffers = mesh_buffers(mesh)

        self.assertTrue(buffers.lightmap_uv)
        self.assertEqual(buffers.vertices[0]["lightmap_uv"], (0.25, 0.5))

    def test_lightmap_active_layer_is_never_exported_as_material_uv0(self) -> None:
        """TODO: Describe `test_lightmap_active_layer_is_never_exported_as_material_uv0`."""
        mesh = FakeMesh()
        material_uvs = mesh.uv_layers.active.data
        lightmap_uvs = [
            FakeUv((0.25, 0.5)), FakeUv((0.75, 0.5)), FakeUv((0.25, 1.0))]
        mesh.uv_layers = FakeUvLayers(material_uvs, lightmap_uvs)
        mesh.uv_layers.active = mesh.uv_layers.get("CEngineLightmap")

        buffers = mesh_buffers(mesh)

        first, second = buffers.vertices[:2]
        self.assertEqual(first["uv"], (0.0, 0.0))
        self.assertEqual(second["uv"], (1.0, 0.0))
        self.assertEqual(first["lightmap_uv"], (0.25, 0.5))
        self.assertEqual(second["lightmap_uv"], (0.75, 0.5))

    def test_render_active_uv_wins_over_differently_named_edit_active_lightmap(self) -> None:
        """TODO: Describe `test_render_active_uv_wins_over_differently_named_edit_active_lightmap`."""
        mesh = FakeMesh()
        material_uvs = mesh.uv_layers.active.data
        old_lightmap_uvs = [
            FakeUv((0.2, 0.3)), FakeUv((0.4, 0.5)), FakeUv((0.6, 0.7))]
        old_lightmap = FakeUvLayer(old_lightmap_uvs, "Lightmap")
        mesh.uv_layers.layers.append(old_lightmap)
        mesh.uv_layers.active = old_lightmap

        buffers = mesh_buffers(mesh)

        first, second = buffers.vertices[:2]
        self.assertEqual(first["uv"], tuple(material_uvs[0].uv))
        self.assertEqual(second["uv"], tuple(material_uvs[1].uv))
        for actual, expected in zip(first["lightmap_uv"], old_lightmap_uvs[0].uv):
            self.assertAlmostEqual(actual, expected)
        for actual, expected in zip(second["lightmap_uv"], old_lightmap_uvs[1].uv):
            self.assertAlmostEqual(actual, expected)

    def test_mesh_buffers_pack_skin_indices_and_weights(self) -> None:
        """TODO: Describe `test_mesh_buffers_pack_skin_indices_and_weights`."""
        armature = FakeArmature()
        obj = FakeObject("SK_Body", "MESH", FakeMesh(skinned=True), armature)

        buffers = mesh_buffers(obj.data, obj, armature)

        self.assertTrue(buffers.skinned)
        self.assertEqual(buffers.skeleton, "ARM_Hero")
        first = buffers.vertices[0]
        self.assertEqual(first["joints"], [0, 1, 0, 0])
        self.assertAlmostEqual(sum(first["weights"]), 1.0)

    def test_polygon_triangles_fans_quads_and_ngons(self) -> None:
        """TODO: Describe `test_polygon_triangles_fans_quads_and_ngons`."""
        self.assertEqual(polygon_triangles(FakePolygon([0, 1, 2, 3])), [(0, 1, 2), (0, 2, 3)])
        self.assertEqual(
            polygon_triangles(FakePolygon([0, 1, 2, 3, 4])),
            [(0, 1, 2), (0, 2, 3), (0, 3, 4)],
        )

    def test_mesh_buffers_triangulates_quads(self) -> None:
        """TODO: Describe `test_mesh_buffers_triangulates_quads`."""
        buffers = mesh_buffers(FakeMesh([FakePolygon([0, 1, 3, 2])], include_quad_vertex=True))

        self.assertEqual(buffers.vertex_count, 4)
        self.assertEqual(buffers.index_count, 6)
        self.assertEqual(buffers.indices, (0, 1, 2, 0, 2, 3))
        self.assertEqual(buffers.bounds_min, (0.0, -1.0, 0.0))
        self.assertEqual(buffers.bounds_max, (2.0, -0.0, 0.0))

    def test_mesh_metadata_payload_is_binary_and_dependency_indexed(self) -> None:
        """TODO: Describe `test_mesh_metadata_payload_is_binary_and_dependency_indexed`."""
        buffers = mesh_buffers(FakeMesh())

        decoded = unpack_record(GAME, "mesh", mesh_payload(buffers))
        self.assertEqual(len(decoded["lods"]), 1)
        self.assertEqual(len(decoded["lods"][0]["vertices"]), 3)
        self.assertEqual(decoded["lods"][0]["indices"], [0, 1, 2])

    def test_write_mesh_asset_writes_binary_payload(self) -> None:
        """TODO: Describe `test_write_mesh_asset_writes_binary_payload`."""
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            blend = root / "hero.blend"
            blend.write_bytes(b"blend")
            obj = FakeObject("SM_Body", "MESH")

            export = write_mesh_asset(
                blend,
                root / "compiled",
                obj,
                lambda name: root / "compiled" / "hero" / "materials" / f"{name}.cmat",
                lambda name: root / "compiled" / "hero" / "skeletons" / f"{name}.cskel",
            )

            data = export.output.read_bytes()
            header = ASSET_HEADER.unpack_from(data)
            self.assertEqual(header[0], b"CEAF")
            self.assertEqual(header[3], 3)
            decoded = unpack_record(
                GAME, "mesh", data[header[7]:header[7] + header[8]])
            self.assertEqual(decoded["flags"], 0)
            self.assertEqual(len(decoded["lods"]), 1)
            self.assertEqual(decoded["lods"][0]["vertices"][0]["position"][0], 0.0)

    def test_child_lods_are_written_in_descending_screen_order(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            blend = root / "hero.blend"
            blend.write_bytes(b"blend")
            base = FakeObject("SM_Body", "MESH")
            child = FakeObject("LOD1_SM_Body", "MESH")
            child.parent = base
            base.children = [child]
            export = write_mesh_asset(
                blend, root / "compiled", base,
                lambda name: root / f"{name}.cmat",
                lambda name: root / f"{name}.cskel",
            )
            data = export.output.read_bytes()
            header = ASSET_HEADER.unpack_from(data)
            decoded = unpack_record(
                GAME, "mesh", data[header[7]:header[7] + header[8]])
            self.assertEqual(
                [lod["screen_size"] for lod in decoded["lods"]], [1.0, 0.5])

    def test_write_mesh_asset_generates_default_material_binding_for_empty_slots(self) -> None:
        """TODO: Describe `test_write_mesh_asset_generates_default_material_binding_for_empty_slots`."""
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            blend = root / "hero.blend"
            blend.write_bytes(b"blend")

            export = write_mesh_asset(
                blend,
                root / "compiled",
                FakeObject("SM_Empty", "MESH", material_names=[]),
                lambda name: root / "compiled" / "hero" / "materials" / f"{name}.cmat",
                lambda name: root / "compiled" / "hero" / "skeletons" / f"{name}.cskel",
            )

            self.assertEqual(export.material_name, "SM_Empty_DefaultMaterial")
            self.assertTrue(export.output.exists())

    def test_write_mesh_assets_splits_multi_material_meshes(self) -> None:
        """TODO: Describe `test_write_mesh_assets_splits_multi_material_meshes`."""
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            blend = root / "hero.blend"
            blend.write_bytes(b"blend")
            mesh = FakeMesh(
                [
                    FakePolygon([0, 1, 2], material_index=0),
                    FakePolygon([0, 1, 2], material_index=1),
                ]
            )
            obj = FakeObject("SM_Multi", "MESH", mesh, material_names=["A", "B"])

            exports = write_mesh_assets(
                blend,
                root / "compiled",
                [obj],
                lambda name: root / "compiled" / "hero" / "materials" / f"{name}.cmat",
                lambda name: root / "compiled" / "hero" / "skeletons" / f"{name}.cskel",
            )

            self.assertEqual([export.material_name for export in exports], ["A", "B"])
            self.assertEqual([export.output.name for export in exports], ["SM_Multi__A.cmesh", "SM_Multi__B.cmesh"])
            for export in exports:
                data = export.output.read_bytes()
                header = ASSET_HEADER.unpack_from(data)
                decoded = unpack_record(
                    GAME, "mesh", data[header[7]:header[7] + header[8]])
                self.assertEqual(len(decoded["lods"][0]["vertices"]), 3)
                self.assertEqual(len(decoded["lods"]), 1)

    def test_write_skinned_mesh_asset_depends_on_skeleton(self) -> None:
        """TODO: Describe `test_write_skinned_mesh_asset_depends_on_skeleton`."""
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            blend = root / "hero.blend"
            blend.write_bytes(b"blend")
            armature = FakeArmature()
            obj = FakeObject("SK_Body", "MESH", FakeMesh(skinned=True), armature)

            export = write_mesh_asset(
                blend,
                root / "compiled",
                obj,
                lambda name: root / "compiled" / "hero" / "materials" / f"{name}.cmat",
                lambda name: root / "compiled" / "hero" / "skeletons" / f"{name}.cskel",
            )

            data = export.output.read_bytes()
            header = ASSET_HEADER.unpack_from(data)
            decoded = unpack_record(
                GAME, "mesh", data[header[7]:header[7] + header[8]])
            self.assertEqual(decoded["flags"], MESH_FLAG_SKINNED)
            self.assertEqual(len(decoded["lods"]), 1)
            self.assertEqual(len(decoded["lods"][0]["vertices"]), 3)


if __name__ == "__main__":
    unittest.main()
