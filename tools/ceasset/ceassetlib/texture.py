from __future__ import annotations

from pathlib import Path


def normalize_pillow_dds_format(dds_format: str) -> str:
    pillow_format = dds_format.upper()
    if pillow_format == "BC1":
        return "DXT1"
    if pillow_format == "BC2":
        return "DXT3"
    if pillow_format == "BC3":
        return "DXT5"
    if pillow_format in {"DXT1", "DXT3", "DXT5", "BC5"}:
        return pillow_format
    raise RuntimeError("Pillow DDS output supports DXT1, DXT3, DXT5, and BC5")


def convert_texture_to_dds(source: Path, output: Path, dds_format: str) -> None:
    try:
        from PIL import Image
    except ImportError as error:
        raise RuntimeError("Pillow is not installed") from error

    pixel_format = normalize_pillow_dds_format(dds_format)
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_name(output.name + ".tmp")
    with Image.open(source) as image:
        image.save(temporary, format="DDS", pixel_format=pixel_format)
    temporary.replace(output)
