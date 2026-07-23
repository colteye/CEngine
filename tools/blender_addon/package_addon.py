#!/usr/bin/env python3
from __future__ import annotations

import argparse
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
ADDON_DIR = ROOT / "tools" / "blender_addon" / "cengine_asset_exporter"
CEASSETLIB_DIR = ROOT / "tools" / "ceasset" / "ceassetlib"
DEFAULT_OUTPUT = ROOT / "build" / "blender_addon" / "cengine_asset_exporter-0.2.0.zip"
PILLOW_WHEELS = {
    "windows-x64": ADDON_DIR / "wheels" / "pillow-12.3.0-cp311-cp311-win_amd64.whl",
    "macos-arm64": ADDON_DIR / "wheels" / "pillow-12.3.0-cp313-cp313-macosx_11_0_arm64.whl",
}
MANIFEST = ADDON_DIR / "blender_manifest.toml"


def iter_package_files(root: Path):
    for path in sorted(root.rglob("*")):
        if not path.is_file():
            continue
        if "__pycache__" in path.parts or path.suffix == ".pyc":
            continue
        yield path


def write_wheel_contents(archive: zipfile.ZipFile, wheel: Path, vendor_root: Path) -> None:
    with zipfile.ZipFile(wheel) as wheel_archive:
        for member in wheel_archive.infolist():
            if member.is_dir():
                continue
            archive.writestr((vendor_root / member.filename).as_posix(), wheel_archive.read(member))


def platform_manifest(platform: str) -> str:
    lines = MANIFEST.read_text(encoding="utf-8").splitlines()
    return "\n".join(
        f'platforms = ["{platform}"]' if line.startswith("platforms = ") else line
        for line in lines) + "\n"


def write_addon_zip(
    output: Path = DEFAULT_OUTPUT,
    pillow_wheel: Path | None = None,
    platform: str = "windows-x64",
    game_file: Path | None = None,
) -> Path:
    if pillow_wheel is None:
        pillow_wheel = PILLOW_WHEELS.get(platform)
        if pillow_wheel is None:
            raise ValueError(f"no bundled Pillow wheel is configured for {platform}")
    if not pillow_wheel.is_file():
        raise FileNotFoundError(f"Pillow wheel does not exist: {pillow_wheel}")
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_suffix(output.suffix + ".tmp")
    with zipfile.ZipFile(temporary, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for path in iter_package_files(ADDON_DIR):
            relative = path.relative_to(ADDON_DIR)
            if "wheels" in relative.parts or relative == Path("blender_manifest.toml"):
                continue
            archive.write(path, relative)
        archive.writestr("blender_manifest.toml", platform_manifest(platform))
        for path in iter_package_files(CEASSETLIB_DIR):
            archive.write(path, Path("ceassetlib") / path.relative_to(CEASSETLIB_DIR))
        if game_file is not None:
            if not game_file.is_file():
                raise FileNotFoundError(f"game manifest does not exist: {game_file}")
            archive.write(game_file, "ceassetlib/game.json")
        write_wheel_contents(archive, pillow_wheel, Path("vendor"))
    temporary.replace(output)
    return output


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Package the CEngine Blender add-on.")
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--pillow-wheel", type=Path)
    parser.add_argument("--platform", default="windows-x64")
    parser.add_argument("--game", type=Path)
    args = parser.parse_args(argv)
    output = write_addon_zip(
        args.output, args.pillow_wheel, args.platform, args.game)
    print(output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
