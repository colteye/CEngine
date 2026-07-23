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
import struct
from io import BytesIO
from pathlib import Path


DDS_HEADER_SIZE = 128
DDS_DX10_HEADER_SIZE = 148
DDSD_MIPMAPCOUNT = 0x00020000
DDSCAPS_COMPLEX = 0x00000008
DDSCAPS_TEXTURE = 0x00001000
DDSCAPS_MIPMAP = 0x00400000


def normalize_pillow_dds_format(dds_format: str) -> str:
    """TODO: Describe `normalize_pillow_dds_format`.

    Args:
        dds_format: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
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


def _apply_mipmap_header(data: bytearray, mip_count: int) -> None:
    """Declare a complete mip chain in an existing DDS header."""
    if mip_count <= 1:
        return
    flags = struct.unpack_from("<I", data, 8)[0] | DDSD_MIPMAPCOUNT
    caps = struct.unpack_from("<I", data, 108)[0] | \
        DDSCAPS_COMPLEX | DDSCAPS_TEXTURE | DDSCAPS_MIPMAP
    struct.pack_into("<I", data, 8, flags)
    struct.pack_into("<I", data, 28, mip_count)
    struct.pack_into("<I", data, 108, caps)


def _dds_payload_offset(data: bytes) -> int:
    """Return the payload offset for a legacy or DX10 DDS header."""
    if len(data) < DDS_HEADER_SIZE or data[:4] != b"DDS ":
        raise RuntimeError("Pillow produced an invalid DDS image")
    return DDS_DX10_HEADER_SIZE if data[84:88] == b"DX10" else DDS_HEADER_SIZE


def write_image_to_dds(image: object, output: Path, dds_format: str) -> None:
    """Write a Pillow image as DDS with every mip level down to 1x1."""
    try:
        from PIL import Image
    except ImportError as error:
        raise RuntimeError("Pillow is not installed") from error

    pixel_format = normalize_pillow_dds_format(dds_format)
    target_mode = "RGB" if pixel_format == "BC5" else "RGBA"
    converted = image.convert(target_mode) if getattr(image, "mode", target_mode) != target_mode else image
    levels: list[object] = [converted]
    try:
        while getattr(levels[-1], "size") != (1, 1):
            width, height = getattr(levels[-1], "size")
            levels.append(levels[-1].resize(
                (max(1, width // 2), max(1, height // 2)),
                Image.Resampling.LANCZOS,
            ))

        result = bytearray()
        payload_offset = 0
        for index, level in enumerate(levels):
            encoded = BytesIO()
            level.save(encoded, format="DDS", pixel_format=pixel_format)
            level_data = encoded.getvalue()
            level_payload_offset = _dds_payload_offset(level_data)
            if index == 0:
                result.extend(level_data[:level_payload_offset])
                payload_offset = level_payload_offset
            elif level_payload_offset != payload_offset:
                raise RuntimeError("Pillow changed DDS headers within one mip chain")
            result.extend(level_data[level_payload_offset:])
        _apply_mipmap_header(result, len(levels))

        output.parent.mkdir(parents=True, exist_ok=True)
        temporary = output.with_name(output.name + ".tmp")
        try:
            temporary.write_bytes(result)
            temporary.replace(output)
        finally:
            if temporary.exists():
                temporary.unlink()
    finally:
        for level in levels[1:]:
            level.close()
        if converted is not image:
            converted.close()


def convert_texture_to_dds(source: Path, output: Path, dds_format: str) -> None:
    """TODO: Describe `convert_texture_to_dds`.

    Args:
        source: TODO: Describe this parameter.
        output: TODO: Describe this parameter.
        dds_format: TODO: Describe this parameter.
    """
    try:
        from PIL import Image
    except ImportError as error:
        raise RuntimeError("Pillow is not installed") from error

    with Image.open(source) as image:
        write_image_to_dds(image, output, dds_format)


def _rgb565(red: int, green: int, blue: int) -> int:
    """TODO: Describe `_rgb565`.

    Args:
        red: TODO: Describe this parameter.
        green: TODO: Describe this parameter.
        blue: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    return ((red * 31 + 127) // 255 << 11) | ((green * 63 + 127) // 255 << 5) | \
        ((blue * 31 + 127) // 255)


def _rgb888(value: int) -> tuple[int, int, int]:
    """TODO: Describe `_rgb888`.

    Args:
        value: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    return (((value >> 11) & 31) * 255 // 31,
            ((value >> 5) & 63) * 255 // 63,
            (value & 31) * 255 // 31)


def _nearest(value: tuple[int, ...], palette: list[tuple[int, ...]]) -> int:
    """TODO: Describe `_nearest`.

    Args:
        value: TODO: Describe this parameter.
        palette: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    return min(range(len(palette)), key=lambda index: sum(
        (value[channel] - palette[index][channel]) ** 2 for channel in range(len(value))))


def _dxt5_block(pixels: list[tuple[int, int, int, int]]) -> bytes:
    """TODO: Describe `_dxt5_block`.

    Args:
        pixels: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
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


def _downsample_rgba8(width: int, height: int, rgba: bytes) -> tuple[int, int, bytes]:
    """Box-filter one RGBA8 mip level."""
    next_width = max(1, width // 2)
    next_height = max(1, height // 2)
    result = bytearray(next_width * next_height * 4)
    for target_y in range(next_height):
        first_y = target_y * height // next_height
        last_y = max(first_y + 1, (target_y + 1) * height // next_height)
        for target_x in range(next_width):
            first_x = target_x * width // next_width
            last_x = max(first_x + 1, (target_x + 1) * width // next_width)
            count = (last_x - first_x) * (last_y - first_y)
            target = (target_y * next_width + target_x) * 4
            for channel in range(4):
                total = sum(
                    rgba[(source_y * width + source_x) * 4 + channel]
                    for source_y in range(first_y, last_y)
                    for source_x in range(first_x, last_x)
                )
                result[target + channel] = (total + count // 2) // count
    return next_width, next_height, bytes(result)


def _dxt5_level(width: int, height: int, rgba: bytes) -> bytes:
    """Compress one possibly sub-block-sized RGBA8 mip level."""
    blocks = bytearray()
    for block_y in range(0, height, 4):
        for block_x in range(0, width, 4):
            pixels = []
            for local_y in range(4):
                source_y = min(height - 1, block_y + local_y)
                for local_x in range(4):
                    source_x = min(width - 1, block_x + local_x)
                    offset = (source_y * width + source_x) * 4
                    pixels.append(tuple(rgba[offset:offset + 4]))
            blocks.extend(_dxt5_block(pixels))
    return bytes(blocks)


def write_rgba_dxt5(output: Path, width: int, height: int, rgba: bytes) -> None:
    """TODO: Describe `write_rgba_dxt5`.

    Args:
        output: TODO: Describe this parameter.
        width: TODO: Describe this parameter.
        height: TODO: Describe this parameter.
        rgba: TODO: Describe this parameter.
    """
    if width <= 0 or height <= 0:
        raise ValueError("DXT5 dimensions must be positive")
    if len(rgba) != width * height * 4:
        raise ValueError("RGBA byte count does not match the texture dimensions")

    levels = [(width, height, rgba)]
    while levels[-1][0:2] != (1, 1):
        levels.append(_downsample_rgba8(*levels[-1]))
    compressed = [_dxt5_level(*level) for level in levels]

    flags = 0x000A1007 | (DDSD_MIPMAPCOUNT if len(levels) > 1 else 0)
    pixel_format = struct.pack("<II4sIIIII", 32, 0x4, b"DXT5", 0, 0, 0, 0, 0)
    header = struct.pack(
        "<IIIIIII11I", 124, flags, height, width, len(compressed[0]), 0, len(levels), *([0] * 11))
    header += pixel_format
    caps = DDSCAPS_TEXTURE
    if len(levels) > 1:
        caps |= DDSCAPS_COMPLEX | DDSCAPS_MIPMAP
    header += struct.pack("<IIIII", caps, 0, 0, 0, 0)
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_name(output.name + ".tmp")
    temporary.write_bytes(b"DDS " + header + b"".join(compressed))
    temporary.replace(output)


def _decode_rgbexp32_pixel(pixel: bytes) -> tuple[float, float, float]:
    """Decode one engine RGBExp32 pixel to linear RGB."""
    multiplier = math.ldexp(1.0, pixel[3] - 128) / 255.0
    return pixel[0] * multiplier, pixel[1] * multiplier, pixel[2] * multiplier


def _encode_rgbexp32_pixel(red: float, green: float, blue: float) -> bytes:
    """Encode one non-negative linear RGB value as engine RGBExp32."""
    maximum = max(red, green, blue)
    if maximum <= 0.0:
        return bytes((0, 0, 0, 0))
    exponent = max(-127, min(127, math.ceil(math.log2(maximum))))
    scale = 255.0 / math.ldexp(1.0, exponent)
    return bytes((
        round(min(255.0, red * scale)),
        round(min(255.0, green * scale)),
        round(min(255.0, blue * scale)),
        exponent + 128,
    ))


def _downsample_rgbexp32(width: int, height: int, rgba: bytes) -> tuple[int, int, bytes]:
    """Box-filter one RGBExp32 mip level in linear HDR space."""
    next_width = max(1, width // 2)
    next_height = max(1, height // 2)
    result = bytearray()
    for target_y in range(next_height):
        first_y = target_y * height // next_height
        last_y = max(first_y + 1, (target_y + 1) * height // next_height)
        for target_x in range(next_width):
            first_x = target_x * width // next_width
            last_x = max(first_x + 1, (target_x + 1) * width // next_width)
            samples = [
                _decode_rgbexp32_pixel(
                    rgba[(source_y * width + source_x) * 4:
                         (source_y * width + source_x + 1) * 4])
                for source_y in range(first_y, last_y)
                for source_x in range(first_x, last_x)
            ]
            count = len(samples)
            result.extend(_encode_rgbexp32_pixel(
                sum(sample[0] for sample in samples) / count,
                sum(sample[1] for sample in samples) / count,
                sum(sample[2] for sample in samples) / count,
            ))
    return next_width, next_height, bytes(result)


def write_rgbexp32_dds(output: Path, width: int, height: int, rgba: bytes) -> None:
    """TODO: Describe `write_rgbexp32_dds`.

    Args:
        output: TODO: Describe this parameter.
        width: TODO: Describe this parameter.
        height: TODO: Describe this parameter.
        rgba: TODO: Describe this parameter.
    """
    if width <= 0 or height <= 0:
        raise ValueError("RGBExp32 dimensions must be positive")
    if len(rgba) != width * height * 4:
        raise ValueError("RGBA byte count does not match the texture dimensions")

    levels = [(width, height, rgba)]
    while levels[-1][0:2] != (1, 1):
        levels.append(_downsample_rgbexp32(*levels[-1]))

    pitch = width * 4
    flags = 0x0000100F | (DDSD_MIPMAPCOUNT if len(levels) > 1 else 0)
    pixel_format = struct.pack("<II4sIIIII", 32, 0x4, b"RGBE", 0, 0, 0, 0, 0)
    header = struct.pack("<IIIIIII11I", 124, flags, height, width, pitch, 0, len(levels),
                         *([0] * 11))
    header += pixel_format
    caps = DDSCAPS_TEXTURE
    if len(levels) > 1:
        caps |= DDSCAPS_COMPLEX | DDSCAPS_MIPMAP
    header += struct.pack("<IIIII", caps, 0, 0, 0, 0)

    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_name(output.name + ".tmp")
    temporary.write_bytes(b"DDS " + header + b"".join(level[2] for level in levels))
    temporary.replace(output)
