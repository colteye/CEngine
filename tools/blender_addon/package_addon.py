#!/usr/bin/env python3
from __future__ import annotations

import argparse
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
ADDON_DIR = ROOT / "tools" / "blender_addon" / "cengine_asset_exporter"
CEASSETLIB_DIR = ROOT / "tools" / "ceasset" / "ceassetlib"
DEFAULT_OUTPUT = ROOT / "build" / "blender_addon" / "cengine_asset_exporter-0.1.6.zip"
PILLOW_WHEEL = ADDON_DIR / "wheels" / "pillow-12.3.0-cp311-cp311-win_amd64.whl"


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


def write_addon_zip(output: Path = DEFAULT_OUTPUT) -> Path:
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_suffix(output.suffix + ".tmp")
    with zipfile.ZipFile(temporary, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for path in iter_package_files(ADDON_DIR):
            if "wheels" in path.relative_to(ADDON_DIR).parts:
                continue
            archive.write(path, path.relative_to(ADDON_DIR))
        for path in iter_package_files(CEASSETLIB_DIR):
            archive.write(path, Path("ceassetlib") / path.relative_to(CEASSETLIB_DIR))
        write_wheel_contents(archive, PILLOW_WHEEL, Path("vendor"))
    temporary.replace(output)
    return output


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Package the CEngine Blender add-on.")
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args(argv)
    output = write_addon_zip(args.output)
    print(output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
