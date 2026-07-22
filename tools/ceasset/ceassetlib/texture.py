from __future__ import annotations

import struct
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
        converted = image.convert("RGBA") if getattr(image, "mode", "RGBA") != "RGBA" else image
        try:
            converted.save(temporary, format="DDS", pixel_format=pixel_format)
        finally:
            if converted is not image:
                converted.close()
    temporary.replace(output)


def _rgb565(red: int, green: int, blue: int) -> int:
    return ((red * 31 + 127) // 255 << 11) | ((green * 63 + 127) // 255 << 5) | \
        ((blue * 31 + 127) // 255)


def _rgb888(value: int) -> tuple[int, int, int]:
    return (((value >> 11) & 31) * 255 // 31,
            ((value >> 5) & 63) * 255 // 63,
            (value & 31) * 255 // 31)


def _nearest(value: tuple[int, ...], palette: list[tuple[int, ...]]) -> int:
    return min(range(len(palette)), key=lambda index: sum(
        (value[channel] - palette[index][channel]) ** 2 for channel in range(len(value))))


def _dxt5_block(pixels: list[tuple[int, int, int, int]]) -> bytes:
    alpha0 = max(pixel[3] for pixel in pixels)
    alpha1 = min(pixel[3] for pixel in pixels)
    if alpha0 == alpha1:
        alpha_palette = [alpha0] * 8
    else:
        alpha_palette = [alpha0, alpha1] + [
            ((7 - index) * alpha0 + index * alpha1 + 3) // 7 for index in range(1, 7)]
    alpha_bits = 0
    for index, pixel in enumerate(pixels):
        alpha_index = min(range(8), key=lambda candidate: abs(pixel[3] - alpha_palette[candidate]))
        alpha_bits |= alpha_index << (index * 3)

    bright = max(pixels, key=lambda pixel: pixel[0] + pixel[1] + pixel[2])
    dark = min(pixels, key=lambda pixel: pixel[0] + pixel[1] + pixel[2])
    color0 = _rgb565(*bright[:3])
    color1 = _rgb565(*dark[:3])
    if color0 < color1:
        color0, color1 = color1, color0
    if color0 == color1:
        color1 = max(0, color0 - 1)
    first = _rgb888(color0)
    second = _rgb888(color1)
    palette = [first, second,
               tuple((2 * first[channel] + second[channel]) // 3 for channel in range(3)),
               tuple((first[channel] + 2 * second[channel]) // 3 for channel in range(3))]
    color_bits = 0
    for index, pixel in enumerate(pixels):
        color_bits |= _nearest(pixel[:3], palette) << (index * 2)
    return bytes((alpha0, alpha1)) + alpha_bits.to_bytes(6, "little") + \
        struct.pack("<HHI", color0, color1, color_bits)


def write_rgba_dxt5(output: Path, width: int, height: int, rgba: bytes) -> None:
    if width <= 0 or height <= 0 or width % 4 or height % 4:
        raise ValueError("DXT5 dimensions must be positive multiples of four")
    if len(rgba) != width * height * 4:
        raise ValueError("RGBA byte count does not match the texture dimensions")

    blocks = bytearray()
    for block_y in range(0, height, 4):
        for block_x in range(0, width, 4):
            pixels = []
            for y in range(block_y, block_y + 4):
                for x in range(block_x, block_x + 4):
                    offset = (y * width + x) * 4
                    pixels.append(tuple(rgba[offset:offset + 4]))
            blocks.extend(_dxt5_block(pixels))

    flags = 0x000A1007  # caps, height, width, pixel format, linear size
    pixel_format = struct.pack("<II4sIIIII", 32, 0x4, b"DXT5", 0, 0, 0, 0, 0)
    header = struct.pack("<IIIIIII11I", 124, flags, height, width, len(blocks), 0, 0, *([0] * 11))
    header += pixel_format
    header += struct.pack("<IIIII", 0x1000, 0, 0, 0, 0)
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_name(output.name + ".tmp")
    temporary.write_bytes(b"DDS " + header + blocks)
    temporary.replace(output)


def write_rgbexp32_dds(output: Path, width: int, height: int, rgba: bytes) -> None:
    if width <= 0 or height <= 0:
        raise ValueError("RGBExp32 dimensions must be positive")
    if len(rgba) != width * height * 4:
        raise ValueError("RGBA byte count does not match the texture dimensions")

    pitch = width * 4
    flags = 0x0000100F  # caps, height, width, pitch, pixel format
    pixel_format = struct.pack("<II4sIIIII", 32, 0x4, b"RGBE", 0, 0, 0, 0, 0)
    header = struct.pack("<IIIIIII11I", 124, flags, height, width, pitch, 0, 1,
                         *([0] * 11))
    header += pixel_format
    header += struct.pack("<IIIII", 0x1000, 0, 0, 0, 0)

    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_name(output.name + ".tmp")
    temporary.write_bytes(b"DDS " + header + rgba)
    temporary.replace(output)
