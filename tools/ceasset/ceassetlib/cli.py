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

import argparse
import subprocess
import sys
from pathlib import Path

from .formats import SOURCE_TEXTURE_EXTENSIONS
from .paths import make_project_paths
from .pipeline import (
    BuildOptions,
    build,
    convert_source,
    convert_texture_source,
    import_asset,
    watch,
)
from .scene_inspect import inspect_scene, print_scene


def parse_args(argv: list[str]) -> argparse.Namespace:
    """TODO: Describe `parse_args`.

    Args:
        argv: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    parser = argparse.ArgumentParser(prog="ceasset")
    parser.add_argument("--project", type=Path, default=Path.cwd())

    subparsers = parser.add_subparsers(dest="command", required=True)

    import_parser = subparsers.add_parser("import")
    import_parser.add_argument("source", type=Path)

    convert_parser = subparsers.add_parser("convert")
    convert_parser.add_argument("source", type=Path)
    convert_parser.add_argument("--dds-format", default="DXT5")
    build_parser = subparsers.add_parser("build")
    build_parser.add_argument("--force", action="store_true")
    build_parser.add_argument("--dds-format", default="DXT5")

    watch_parser = subparsers.add_parser("watch")
    watch_parser.add_argument("--force", action="store_true")
    watch_parser.add_argument("--interval-ms", type=int, default=1000)
    watch_parser.add_argument("--dds-format", default="DXT5")

    inspect_parser = subparsers.add_parser("inspect")
    inspect_parser.add_argument("asset", type=Path)
    validate_parser = subparsers.add_parser("validate")
    validate_parser.add_argument("asset", type=Path)

    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    """TODO: Describe `main`.

    Args:
        argv: TODO: Describe this parameter.

    Returns:
        TODO: Describe the produced value.
    """
    try:
        args = parse_args(argv)
        paths = make_project_paths(args.project)

        if args.command == "import":
            return import_asset(paths, args.source)
        if args.command == "convert":
            if args.source.suffix.lower() in SOURCE_TEXTURE_EXTENSIONS:
                return convert_texture_source(paths, args.source, args.dds_format)
            return convert_source(paths, args.source)
        if args.command == "build":
            return build(
                paths,
                BuildOptions(
                    force=args.force,
                    dds_format=args.dds_format,
                ),
            )
        if args.command == "watch":
            return watch(
                paths,
                BuildOptions(
                    force=args.force,
                    dds_format=args.dds_format,
                ),
                max(args.interval_ms, 100),
            )
        if args.command in ("inspect", "validate"):
            asset = args.asset if args.asset.is_absolute() else paths.root / args.asset
            inspection = inspect_scene(asset, paths.root, validate_assets=args.command == "validate")
            if args.command == "inspect":
                print_scene(inspection)
            else:
                print(f"valid: {asset}")
            return 0
    except (OSError, RuntimeError, ValueError, subprocess.SubprocessError) as error:
        print(f"ceasset: {error}", file=sys.stderr)
        return 1

    return 1
