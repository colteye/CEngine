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
    MATERIAL_TEXTURE,
    material_output_path,
    material_payload,
    material_texture_bindings,
    object_materials,
    write_material_asset,
)


ASSET_HEADER = struct.Struct("<4sHHI16sQ16sQQQ")


class FakeImage:
    def __init__(self, path: Path) -> None:
        self.path = path


class FakeNode:
    def __init__(self, node_type: str, image: FakeImage | None = None) -> None:
        self.type = node_type
        self.image = image


class FakeSocket:
    def __init__(self, name: str) -> None:
        self.name = name


class FakeLink:
    def __init__(self, image: FakeImage, socket: str, to_node_type: str = "BSDF_PRINCIPLED") -> None:
        self.from_node = FakeNode("TEX_IMAGE", image)
        self.to_node = FakeNode(to_node_type)
        self.to_socket = FakeSocket(socket)


class FakeNodeTree:
    def __init__(self, links: list[FakeLink]) -> None:
        self.links = links


class FakeMaterial:
    def __init__(self, name: str, links: list[FakeLink]) -> None:
        self.name = name
        self.node_tree = FakeNodeTree(links)

    def get(self, _key: str, default: object = None) -> object:
        return default


class FakeSlot:
    def __init__(self, material: FakeMaterial | None) -> None:
        self.material = material


class FakeObject:
    def __init__(self, slots: list[FakeSlot]) -> None:
        self.material_slots = slots


class BlenderMaterialsTests(unittest.TestCase):
    def test_material_output_path_is_sanitized_cmat(self) -> None:
        output = material_output_path(Path("hero.blend"), Path("compiled"), "Hero Skin")

        self.assertEqual(output, Path("compiled/hero/materials/Hero_Skin.cmat"))

    def test_object_materials_are_unique_and_sorted(self) -> None:
        body = FakeMaterial("Body", [])
        eyes = FakeMaterial("Eyes", [])

        materials = object_materials([FakeObject([FakeSlot(eyes), FakeSlot(body)]), FakeObject([FakeSlot(body)])])

        self.assertEqual([material.name for material in materials], ["Body", "Eyes"])

    def test_material_texture_bindings_follow_principled_links(self) -> None:
        image = FakeImage(Path("albedo.png"))
        normal = FakeImage(Path("normal.png"))
        material = FakeMaterial(
            "HeroSkin",
            [FakeLink(image, "Base Color"), FakeLink(normal, "Color", "NORMAL_MAP")],
        )

        bindings = material_texture_bindings(
            material,
            lambda fake_image: fake_image.path,
            lambda source: source.with_suffix(".dds"),
        )

        self.assertEqual([(binding.slot, binding.output.name) for binding in bindings], [("base_color", "albedo.dds"), ("normal", "normal.dds")])

    def test_material_payload_records_texture_slots_and_paths(self) -> None:
        material = FakeMaterial("HeroSkin", [])
        payload = material_payload(
            Path("hero.blend"),
            material,
            [type("Binding", (), {"slot": "base_color", "output": Path("compiled/hero/textures/albedo.dds")})()],
        )

        header = MATERIAL_HEADER.unpack_from(payload)
        self.assertEqual(header[0], MATERIAL_MAGIC)
        self.assertEqual(header[4], 1)
        texture = MATERIAL_TEXTURE.unpack_from(payload, header[5])
        strings = payload[header[6] : header[6] + header[7]]
        self.assertEqual(texture[0], 1)
        self.assertEqual(strings[texture[1] : texture[1] + texture[2]], b"compiled/hero/textures/albedo.dds")

    def test_material_payload_uses_missing_texture_fallback_when_untextured(self) -> None:
        material = FakeMaterial("Untextured", [])
        payload = material_payload(
            Path("hero.blend"),
            material,
            [],
            fallback_texture=Path("assets/demo/missing/missing.DDS"),
        )

        header = MATERIAL_HEADER.unpack_from(payload)
        self.assertEqual(header[4], 1)
        texture = MATERIAL_TEXTURE.unpack_from(payload, header[5])
        strings = payload[header[6] : header[6] + header[7]]
        self.assertEqual(texture[0], 1)
        self.assertEqual(strings[texture[1] : texture[1] + texture[2]], b"assets/demo/missing/missing.DDS")

    def test_write_material_asset_writes_common_cmat(self) -> None:
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
            texture = MATERIAL_TEXTURE.unpack_from(payload, material_header[5])
            strings = payload[material_header[6] : material_header[6] + material_header[7]]
            self.assertEqual(strings[material_header[8] : material_header[8] + material_header[9]], b"HeroSkin")
            self.assertEqual(texture[0], 1)


if __name__ == "__main__":
    unittest.main()
