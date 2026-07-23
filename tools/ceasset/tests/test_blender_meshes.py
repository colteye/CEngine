from __future__ import annotations

import struct
import sys
import tempfile
import unittest
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "blender_addon"))

from cengine_asset_exporter.meshes import (  # noqa: E402
    MESH_FLAG_SKINNED,
    MESH_METADATA,
    MESH_METADATA_MAGIC,
    SKINNED_VERTEX,
    VERTEX,
    blender_to_engine_vector,
    mesh_buffers,
    mesh_metadata_payload,
    mesh_objects,
    mesh_output_path,
    polygon_triangles,
    write_mesh_asset,
    write_mesh_assets,
)


ASSET_HEADER = struct.Struct("<4sHHI16sQ16sQQQ")


class FakeVertex:
    def __init__(self, co: tuple[float, float, float], groups: list[object] | None = None) -> None:
        self.co = co
        self.groups = groups or []


class FakeGroupWeight:
    def __init__(self, group: int, weight: float) -> None:
        self.group = group
        self.weight = weight


class FakeLoop:
    def __init__(self, vertex_index: int, normal: tuple[float, float, float]) -> None:
        self.vertex_index = vertex_index
        self.normal = normal


class FakePolygon:
    def __init__(self, loop_indices: list[int], material_index: int = 0) -> None:
        self.loop_indices = loop_indices
        self.material_index = material_index


class FakeUv:
    def __init__(self, uv: tuple[float, float]) -> None:
        self.uv = uv


class FakeUvLayer:
    def __init__(self, data: list[FakeUv], name: str = "UVMap", active_render: bool = False) -> None:
        self.data = data
        self.name = name
        self.active_render = active_render


class FakeUvLayers:
    def __init__(self, data: list[FakeUv], lightmap: list[FakeUv] | None = None) -> None:
        self.active = FakeUvLayer(data, active_render=True)
        self.layers = [self.active]
        if lightmap is not None:
            layer = FakeUvLayer(lightmap, "CEngineLightmap")
            self.layers.append(layer)

    def get(self, name: str):
        return next((layer for layer in self.layers if getattr(layer, "name", "") == name), None)

    def __len__(self) -> int:
        return len(self.layers)

    def __getitem__(self, index: int):
        return self.layers[index]

    def __iter__(self):
        return iter(self.layers)


class FakeMesh:
    def __init__(
        self,
        polygons: list[FakePolygon] | None = None,
        skinned: bool = False,
        include_quad_vertex: bool = False,
    ) -> None:
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
    def __init__(self, name: str) -> None:
        self.name = name


class FakeSlot:
    def __init__(self, material: FakeMaterial) -> None:
        self.material = material


class FakeVertexGroup:
    def __init__(self, name: str) -> None:
        self.name = name


class FakeBone:
    def __init__(self, name: str) -> None:
        self.name = name


class FakeArmatureData:
    def __init__(self) -> None:
        self.bones = [FakeBone("root"), FakeBone("spine")]


class FakeArmature:
    def __init__(self, name: str = "ARM_Hero") -> None:
        self.name = name
        self.type = "ARMATURE"
        self.data = FakeArmatureData()


class FakeModifier:
    def __init__(self, armature: FakeArmature) -> None:
        self.type = "ARMATURE"
        self.object = armature


class FakeObject:
    def __init__(
        self,
        name: str,
        obj_type: str,
        mesh: FakeMesh | None = None,
        armature: FakeArmature | None = None,
        material_names: list[str] | None = None,
    ) -> None:
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
    def test_mesh_output_path_is_sanitized_cmesh(self) -> None:
        output = mesh_output_path(Path("hero.blend"), Path("compiled"), "SM Hero")

        self.assertEqual(output, Path("compiled/hero/meshes/SM_Hero.cmesh"))

    def test_mesh_objects_are_unique_and_sorted(self) -> None:
        first = FakeObject("SM_B", "MESH")
        second = FakeObject("SM_A", "MESH")
        armature = FakeObject("ARM_Hero", "ARMATURE")

        meshes = mesh_objects([first, armature, second, first])

        self.assertEqual([obj.name for obj in meshes], ["SM_A", "SM_B"])

    def test_mesh_buffers_pack_triangle_vertices_and_indices(self) -> None:
        buffers = mesh_buffers(FakeMesh())

        self.assertEqual(buffers.vertex_count, 3)
        self.assertEqual(buffers.index_count, 3)
        self.assertEqual(buffers.bounds_min, (0.0, -1.0, 0.0))
        self.assertEqual(buffers.bounds_max, (2.0, -0.0, 0.0))
        self.assertEqual(buffers.vertex_stride, VERTEX.size)
        self.assertFalse(buffers.skinned)
        self.assertEqual(len(buffers.data), 3 * VERTEX.size + 3 * 4)
        self.assertEqual(VERTEX.unpack_from(buffers.data, 0),
            (0.0, -0.0, 0.0, 0.0, -0.0, 1.0, 0.0, 0.0, 0.0, 0.0))

    def test_blender_to_engine_vector_maps_to_x_forward_y_left_z_up(self) -> None:
        self.assertEqual(blender_to_engine_vector((1.0, 2.0, 3.0)), (2.0, -1.0, 3.0))

    def test_named_lightmap_uv_layer_is_packed_as_uv1(self) -> None:
        mesh = FakeMesh()
        mesh.uv_layers = FakeUvLayers(mesh.uv_layers.active.data, [
            FakeUv((0.25, 0.5)), FakeUv((0.75, 0.5)), FakeUv((0.25, 1.0))])

        buffers = mesh_buffers(mesh)

        self.assertTrue(buffers.lightmap_uv)
        self.assertEqual(VERTEX.unpack_from(buffers.data, 0)[8:10], (0.25, 0.5))

    def test_lightmap_active_layer_is_never_exported_as_material_uv0(self) -> None:
        mesh = FakeMesh()
        material_uvs = mesh.uv_layers.active.data
        lightmap_uvs = [
            FakeUv((0.25, 0.5)), FakeUv((0.75, 0.5)), FakeUv((0.25, 1.0))]
        mesh.uv_layers = FakeUvLayers(material_uvs, lightmap_uvs)
        mesh.uv_layers.active = mesh.uv_layers.get("CEngineLightmap")

        buffers = mesh_buffers(mesh)

        first = VERTEX.unpack_from(buffers.data, 0)
        second = VERTEX.unpack_from(buffers.data, VERTEX.size)
        self.assertEqual(first[6:8], (0.0, 0.0))
        self.assertEqual(second[6:8], (1.0, 0.0))
        self.assertEqual(first[8:10], (0.25, 0.5))
        self.assertEqual(second[8:10], (0.75, 0.5))

    def test_render_active_uv_wins_over_differently_named_edit_active_lightmap(self) -> None:
        mesh = FakeMesh()
        material_uvs = mesh.uv_layers.active.data
        old_lightmap_uvs = [
            FakeUv((0.2, 0.3)), FakeUv((0.4, 0.5)), FakeUv((0.6, 0.7))]
        old_lightmap = FakeUvLayer(old_lightmap_uvs, "Lightmap")
        mesh.uv_layers.layers.append(old_lightmap)
        mesh.uv_layers.active = old_lightmap

        buffers = mesh_buffers(mesh)

        first = VERTEX.unpack_from(buffers.data, 0)
        second = VERTEX.unpack_from(buffers.data, VERTEX.size)
        self.assertEqual(first[6:8], tuple(material_uvs[0].uv))
        self.assertEqual(second[6:8], tuple(material_uvs[1].uv))
        for actual, expected in zip(first[8:10], old_lightmap_uvs[0].uv):
            self.assertAlmostEqual(actual, expected)
        for actual, expected in zip(second[8:10], old_lightmap_uvs[1].uv):
            self.assertAlmostEqual(actual, expected)

    def test_mesh_buffers_pack_skin_indices_and_weights(self) -> None:
        armature = FakeArmature()
        obj = FakeObject("SK_Body", "MESH", FakeMesh(skinned=True), armature)

        buffers = mesh_buffers(obj.data, obj, armature)

        self.assertTrue(buffers.skinned)
        self.assertEqual(buffers.skeleton, "ARM_Hero")
        self.assertEqual(buffers.vertex_stride, SKINNED_VERTEX.size)
        first = SKINNED_VERTEX.unpack_from(buffers.data, 0)
        self.assertEqual(first[10:14], (0, 1, 0, 0))
        self.assertEqual(sum(first[14:18]), 65535)

    def test_polygon_triangles_fans_quads_and_ngons(self) -> None:
        self.assertEqual(polygon_triangles(FakePolygon([0, 1, 2, 3])), [(0, 1, 2), (0, 2, 3)])
        self.assertEqual(
            polygon_triangles(FakePolygon([0, 1, 2, 3, 4])),
            [(0, 1, 2), (0, 2, 3), (0, 3, 4)],
        )

    def test_mesh_buffers_triangulates_quads(self) -> None:
        buffers = mesh_buffers(FakeMesh([FakePolygon([0, 1, 3, 2])], include_quad_vertex=True))

        self.assertEqual(buffers.vertex_count, 4)
        self.assertEqual(buffers.index_count, 6)
        self.assertEqual(
            struct.unpack_from(
                "<6I", buffers.data, buffers.vertex_count * VERTEX.size),
            (0, 1, 2, 0, 2, 3))
        self.assertEqual(buffers.bounds_min, (0.0, -1.0, 0.0))
        self.assertEqual(buffers.bounds_max, (2.0, -0.0, 0.0))

    def test_mesh_metadata_payload_is_binary_and_dependency_indexed(self) -> None:
        buffers = mesh_buffers(FakeMesh())

        payload = mesh_metadata_payload(buffers, 2, MESH_METADATA.size)

        header = MESH_METADATA.unpack_from(payload)
        self.assertEqual(header[0], MESH_METADATA_MAGIC)
        self.assertEqual(header[4], 3)
        self.assertEqual(header[5], 3)
        self.assertEqual(header[6], VERTEX.size)
        self.assertEqual(header[14], 2)
        self.assertEqual(header[15], MESH_METADATA.size)

    def test_write_mesh_asset_writes_binary_payload(self) -> None:
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
            payload_offset = header[7]
            payload_size = header[8]

            metadata = MESH_METADATA.unpack_from(data, payload_offset)
            self.assertEqual(metadata[0], MESH_METADATA_MAGIC)
            self.assertEqual(metadata[3], 0)
            self.assertEqual(metadata[6], VERTEX.size)
            self.assertEqual(metadata[14], 1)
            self.assertEqual(metadata[15], MESH_METADATA.size)
            self.assertEqual(payload_size, MESH_METADATA.size + 3 * VERTEX.size + 3 * 4)
            geometry_offset = payload_offset + metadata[15]
            self.assertEqual(data[geometry_offset : geometry_offset + 4], struct.pack("<f", 0.0))

    def test_write_mesh_asset_generates_default_material_binding_for_empty_slots(self) -> None:
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
                metadata = MESH_METADATA.unpack_from(data, header[7])
                self.assertEqual(metadata[4], 3)
                self.assertEqual(metadata[14], 1)

    def test_write_skinned_mesh_asset_depends_on_skeleton(self) -> None:
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
            metadata = MESH_METADATA.unpack_from(data, header[7])
            self.assertEqual(metadata[3], MESH_FLAG_SKINNED)
            self.assertEqual(metadata[6], SKINNED_VERTEX.size)
            self.assertEqual(metadata[14], 1)
            self.assertEqual(metadata[15], MESH_METADATA.size)
            self.assertEqual(header[8], MESH_METADATA.size + 3 * SKINNED_VERTEX.size + 3 * 4)


if __name__ == "__main__":
    unittest.main()
