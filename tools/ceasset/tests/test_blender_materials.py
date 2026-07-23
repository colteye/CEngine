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

from cengine_asset_exporter.materials import (  # noqa: E402
    MATERIAL_HEADER,
    MATERIAL_MAGIC,
    MATERIAL_VERSION,
    MATERIAL_TEXTURE,
    material_factors,
    material_output_path,
    material_payload,
    material_surface,
    material_texture_bindings,
    object_materials,
    supported_material_images,
    write_material_asset,
)


ASSET_HEADER = struct.Struct("<4sHHI16sQ16sQQQ")


class FakeImage:
    """TODO: Describe `FakeImage`."""

    def __init__(self, path: Path) -> None:
        """TODO: Describe `__init__`.

        Args:
            path: TODO: Describe this parameter.
        """
        self.path = path


class FakeNode:
    """TODO: Describe `FakeNode`."""

    def __init__(self, node_type: str, image: FakeImage | None = None,
                 inputs: dict[str, object] | None = None) -> None:
        """TODO: Describe `__init__`.

        Args:
            node_type: TODO: Describe this parameter.
            image: TODO: Describe this parameter.
            inputs: TODO: Describe this parameter.
        """
        self.type = node_type
        self.image = image
        self.inputs = inputs or {}


class FakeSocket:
    """TODO: Describe `FakeSocket`."""

    def __init__(self, name: str, default_value: object = None) -> None:
        """TODO: Describe `__init__`.

        Args:
            name: TODO: Describe this parameter.
            default_value: TODO: Describe this parameter.
        """
        self.name = name
        self.default_value = default_value


class FakeLink:
    """TODO: Describe `FakeLink`."""

    def __init__(self, image: FakeImage | None, socket: str,
                 to_node_type: str = "BSDF_PRINCIPLED",
                 from_node: FakeNode | None = None,
                 to_node: FakeNode | None = None) -> None:
        """TODO: Describe `__init__`.

        Args:
            image: TODO: Describe this parameter.
            socket: TODO: Describe this parameter.
            to_node_type: TODO: Describe this parameter.
            from_node: TODO: Describe this parameter.
            to_node: TODO: Describe this parameter.
        """
        self.from_node = from_node or FakeNode("TEX_IMAGE", image)
        self.to_node = to_node or FakeNode(to_node_type)
        self.to_socket = FakeSocket(socket)


class FakeNodeTree:
    """TODO: Describe `FakeNodeTree`."""

    def __init__(self, links: list[FakeLink], nodes: list[FakeNode] | None = None) -> None:
        """TODO: Describe `__init__`.

        Args:
            links: TODO: Describe this parameter.
            nodes: TODO: Describe this parameter.
        """
        self.links = links
        self.nodes = nodes or []


class FakeMaterial:
    """TODO: Describe `FakeMaterial`."""

    def __init__(self, name: str, links: list[FakeLink], values: dict[str, object] | None = None,
                 surface_render_method: str = "DITHERED", blend_method: str = "HASHED",
                 alpha_threshold: float = 0.5) -> None:
        """TODO: Describe `__init__`.

        Args:
            name: TODO: Describe this parameter.
            links: TODO: Describe this parameter.
            values: TODO: Describe this parameter.
            surface_render_method: TODO: Describe this parameter.
            blend_method: TODO: Describe this parameter.
            alpha_threshold: TODO: Describe this parameter.
        """
        self.name = name
        self.surface_render_method = surface_render_method
        self.blend_method = blend_method
        self.alpha_threshold = alpha_threshold
        principled = FakeNode("BSDF_PRINCIPLED", inputs={
            key: FakeSocket(key, value) for key, value in (values or {}).items()
        })
        self.node_tree = FakeNodeTree(links, [principled])

    def get(self, _key: str, default: object = None) -> object:
        """TODO: Describe `get`.

        Args:
            _key: TODO: Describe this parameter.
            default: TODO: Describe this parameter.

        Returns:
            TODO: Describe the produced value.
        """
        return default


class FakeSlot:
    """TODO: Describe `FakeSlot`."""

    def __init__(self, material: FakeMaterial | None) -> None:
        """TODO: Describe `__init__`.

        Args:
            material: TODO: Describe this parameter.
        """
        self.material = material


class FakeObject:
    """TODO: Describe `FakeObject`."""

    def __init__(self, slots: list[FakeSlot]) -> None:
        """TODO: Describe `__init__`.

        Args:
            slots: TODO: Describe this parameter.
        """
        self.material_slots = slots


class BlenderMaterialsTests(unittest.TestCase):
    """TODO: Describe `BlenderMaterialsTests`."""

    def test_material_output_path_is_sanitized_cmat(self) -> None:
        """TODO: Describe `test_material_output_path_is_sanitized_cmat`."""
        output = material_output_path(Path("hero.blend"), Path("compiled"), "Hero Skin")

        self.assertEqual(output, Path("compiled/hero/materials/Hero_Skin.cmat"))

    def test_object_materials_are_unique_and_sorted(self) -> None:
        """TODO: Describe `test_object_materials_are_unique_and_sorted`."""
        body = FakeMaterial("Body", [])
        eyes = FakeMaterial("Eyes", [])

        materials = object_materials([FakeObject([FakeSlot(eyes), FakeSlot(body)]), FakeObject([FakeSlot(body)])])

        self.assertEqual([material.name for material in materials], ["Body", "Eyes"])

    def test_material_texture_bindings_follow_principled_links(self) -> None:
        """TODO: Describe `test_material_texture_bindings_follow_principled_links`."""
        image = FakeImage(Path("albedo.png"))
        normal = FakeImage(Path("normal.png"))
        material = FakeMaterial(
            "HeroSkin",
            [FakeLink(image, "Base Color"), FakeLink(normal, "Normal")],
        )

        bindings = material_texture_bindings(
            material,
            lambda fake_image: fake_image.path,
            lambda source: source.with_suffix(".dds"),
        )

        self.assertEqual([(binding.slot, binding.output.name) for binding in bindings], [("base_color", "albedo.dds"), ("normal", "normal.dds")])

    def test_bump_height_link_becomes_a_generated_normal_source(self) -> None:
        """TODO: Describe `test_bump_height_link_becomes_a_generated_normal_source`."""
        bump_image = FakeImage(Path("stone_bump.png"))
        bump = FakeNode("BUMP", inputs={
            "Strength": FakeSocket("Strength", 0.6),
            "Distance": FakeSocket("Distance", 0.25),
        })
        principled = FakeNode("BSDF_PRINCIPLED")
        material = FakeMaterial("Stone", [
            FakeLink(bump_image, "Height", to_node=bump),
            FakeLink(None, "Normal", from_node=bump, to_node=principled),
        ])

        bindings = material_texture_bindings(
            material,
            lambda fake_image: fake_image.path,
            lambda source: source.with_suffix(".dds"),
        )

        self.assertEqual(len(bindings), 1)
        self.assertEqual(bindings[0].slot, "bump")
        self.assertEqual(bindings[0].output, Path("stone_bump.png"))
        self.assertAlmostEqual(bindings[0].bump_strength, 0.6)
        self.assertAlmostEqual(bindings[0].bump_distance, 0.25)

    def test_bump_named_image_through_normal_map_uses_unit_height_even_at_zero_strength(self) -> None:
        """TODO: Describe `test_bump_named_image_through_normal_map_uses_unit_height_even_at_zero_strength`."""
        bump_image = FakeImage(Path("stone_bump.png"))
        bump_image.name = "stone_bump.png"
        bump_image.filepath = "stone_bump.png"
        normal_map = FakeNode("NORMAL_MAP", inputs={
            "Strength": FakeSocket("Strength", 0.0),
        })
        principled = FakeNode("BSDF_PRINCIPLED")
        material = FakeMaterial("Stone", [
            FakeLink(bump_image, "Color", to_node=normal_map),
            FakeLink(None, "Normal", from_node=normal_map, to_node=principled),
        ])

        bindings = material_texture_bindings(
            material,
            lambda fake_image: fake_image.path,
            lambda source: source.with_suffix(".dds"),
        )

        self.assertEqual(len(bindings), 1)
        self.assertEqual(bindings[0].slot, "bump")
        self.assertAlmostEqual(bindings[0].bump_strength, 1.0)
        self.assertAlmostEqual(bindings[0].bump_distance, 1.0)
        self.assertEqual(supported_material_images(material), [])

    def test_grayscale_image_through_normal_map_is_assumed_to_be_bump(self) -> None:
        """TODO: Describe `test_grayscale_image_through_normal_map_is_assumed_to_be_bump`."""
        height = FakeImage(Path("stone_detail.png"))
        height.name = "stone_detail.png"
        height.filepath = "stone_detail.png"
        height.pixels = [0.1, 0.1, 0.1, 1.0, 0.8, 0.8, 0.8, 1.0]
        normal_map = FakeNode("NORMAL_MAP", inputs={})
        principled = FakeNode("BSDF_PRINCIPLED")
        material = FakeMaterial("Stone", [
            FakeLink(height, "Color", to_node=normal_map),
            FakeLink(None, "Normal", from_node=normal_map, to_node=principled),
        ])

        bindings = material_texture_bindings(
            material,
            lambda fake_image: fake_image.path,
            lambda source: source.with_suffix(".dds"),
        )

        self.assertEqual([binding.slot for binding in bindings], ["bump"])
        self.assertEqual(supported_material_images(material), [])

    def test_unsupported_principled_images_are_not_exported(self) -> None:
        """TODO: Describe `test_unsupported_principled_images_are_not_exported`."""
        base = FakeImage(Path("base.png"))
        specular = FakeImage(Path("specular.png"))
        alpha = FakeImage(Path("alpha.png"))
        material = FakeMaterial("Basic", [
            FakeLink(base, "Base Color"),
            FakeLink(specular, "Specular IOR Level"),
            FakeLink(alpha, "Alpha"),
        ])

        self.assertEqual(supported_material_images(material), [base])

    def test_principled_alpha_image_is_a_packing_input_not_a_standalone_texture(self) -> None:
        """TODO: Describe `test_principled_alpha_image_is_a_packing_input_not_a_standalone_texture`."""
        base = FakeImage(Path("leaves.png"))
        opacity = FakeImage(Path("leaves_mask.png"))
        material = FakeMaterial("Leaves", [
            FakeLink(base, "Base Color"),
            FakeLink(opacity, "Alpha"),
        ])

        bindings = material_texture_bindings(
            material,
            lambda fake_image: fake_image.path,
            lambda source: source.with_suffix(".dds"),
        )

        self.assertEqual([(binding.slot, binding.output.name) for binding in bindings], [
            ("alpha", "leaves_mask.png"),
            ("base_color", "leaves.dds"),
        ])
        self.assertEqual(supported_material_images(material), [base])

    def test_material_payload_records_texture_slots_and_paths(self) -> None:
        """TODO: Describe `test_material_payload_records_texture_slots_and_paths`."""
        material = FakeMaterial("HeroSkin", [])
        payload = material_payload(
            Path("hero.blend"),
            material,
            [type("Binding", (), {"slot": "base_color", "output": Path("compiled/hero/textures/albedo.dds")})()],
        )

        header = MATERIAL_HEADER.unpack_from(payload)
        self.assertEqual(header[0], MATERIAL_MAGIC)
        self.assertEqual(header[1], MATERIAL_VERSION)
        self.assertEqual(header[4], 0)
        self.assertEqual(header[5], 1)
        texture = MATERIAL_TEXTURE.unpack_from(payload, header[6])
        strings = payload[header[7] : header[7] + header[8]]
        self.assertEqual(texture[0], 1)
        self.assertEqual(strings[texture[1] : texture[1] + texture[2]], b"compiled/hero/textures/albedo.dds")

    def test_material_payload_does_not_invent_source_texture_bindings(self) -> None:
        """TODO: Describe `test_material_payload_does_not_invent_source_texture_bindings`."""
        material = FakeMaterial("Untextured", [], {
            "Base Color": (0.2, 0.4, 0.6, 1.0),
            "Metallic": 0.25,
            "Roughness": 0.75,
        })
        payload = material_payload(
            Path("hero.blend"),
            material,
            [],
        )

        header = MATERIAL_HEADER.unpack_from(payload)
        self.assertEqual(header[5], 0)
        for actual, expected in zip(header[11:15], (0.2, 0.4, 0.6, 1.0)):
            self.assertAlmostEqual(actual, expected)
        self.assertAlmostEqual(header[15], 0.25)
        self.assertAlmostEqual(header[16], 0.75)
        self.assertAlmostEqual(header[17], 1.0)

    def test_linked_principled_alpha_exports_alpha_hash_mode(self) -> None:
        """TODO: Describe `test_linked_principled_alpha_exports_alpha_hash_mode`."""
        image_node = FakeNode("TEX_IMAGE", FakeImage(Path("leaves.png")))
        principled = FakeNode("BSDF_PRINCIPLED", inputs={
            "Alpha": FakeSocket("Alpha", 1.0),
        })
        material = FakeMaterial("Leaves", [
            FakeLink(None, "Alpha", from_node=image_node, to_node=principled),
        ])
        material.node_tree.nodes = [principled]

        surface = material_surface(material, 1)
        payload = material_payload(Path("foliage.blend"), material, [])
        header = MATERIAL_HEADER.unpack_from(payload)

        self.assertEqual(surface.render_mode, 2)
        self.assertEqual(header[4], 2)
        self.assertAlmostEqual(header[18], 0.5)

    def test_textured_material_preserves_principled_alpha_factor(self) -> None:
        """TODO: Describe `test_textured_material_preserves_principled_alpha_factor`."""
        material = FakeMaterial("Glass", [], {"Alpha": 0.25})
        factors = material_factors(material, [
            type("Binding", (), {"slot": "base_color"})(),
        ])

        self.assertEqual(factors.base_color, (1.0, 1.0, 1.0, 0.25))

    def test_texture_inputs_replace_only_their_corresponding_constants(self) -> None:
        """TODO: Describe `test_texture_inputs_replace_only_their_corresponding_constants`."""
        material = FakeMaterial("Mixed", [
            FakeLink(FakeImage(Path("albedo.png")), "Base Color"),
            FakeLink(FakeImage(Path("roughness.png")), "Roughness"),
        ], {
            "Base Color": (0.2, 0.4, 0.6, 1.0),
            "Metallic": 0.25,
            "Roughness": 0.75,
        })
        bindings = material_texture_bindings(
            material,
            lambda fake_image: fake_image.path,
            lambda source: source.with_suffix(".dds"),
        )

        factors = material_factors(material, bindings)

        self.assertEqual(factors.base_color, (1.0, 1.0, 1.0, 1.0))
        self.assertEqual(factors.metallic, 0.25)
        self.assertEqual(factors.roughness, 1.0)
        self.assertEqual(factors.ao, 1.0)

    def test_write_material_asset_writes_common_cmat(self) -> None:
        """TODO: Describe `test_write_material_asset_writes_common_cmat`."""
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            blend = root / "hero.blend"
            image = root / "albedo.png"
            blend.write_bytes(b"blend")
            image.write_bytes(b"png")
            material = FakeMaterial("HeroSkin", [FakeLink(FakeImage(image), "Base Color")])

            export = write_material_asset(
                blend,
                root / "compiled",
                material,
                lambda fake_image: fake_image.path,
                lambda source: root / "compiled" / "hero" / "textures" / source.with_suffix(".dds").name,
            )

            data = export.output.read_bytes()
            header = ASSET_HEADER.unpack_from(data)
            self.assertEqual(header[0], b"CEAF")
            self.assertEqual(header[3], 2)

            payload = data[header[7] : header[7] + header[8]]
            material_header = MATERIAL_HEADER.unpack_from(payload)
            texture = MATERIAL_TEXTURE.unpack_from(payload, material_header[6])
            strings = payload[material_header[7] : material_header[7] + material_header[8]]
            self.assertEqual(strings[material_header[9] : material_header[9] + material_header[10]], b"HeroSkin")
            self.assertEqual(texture[0], 1)
            self.assertEqual(export.generated_textures, ())
            self.assertFalse((root / "compiled" / "hero" / "textures" / "HeroSkin_mra.dds").exists())


if __name__ == "__main__":
    unittest.main()
