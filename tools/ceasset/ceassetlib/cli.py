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


def parse_args(argv: list[str]) -> argparse.Namespace:
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

    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
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
    except (OSError, RuntimeError, ValueError, subprocess.SubprocessError) as error:
        print(f"ceasset: {error}", file=sys.stderr)
        return 1

    return 1
