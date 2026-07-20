#!/usr/bin/env python3
"""CEngine asset converter command.

The runtime loads target assets only. This tool converts source assets to target
assets and tracks them with a compact binary registry/cache. Each source format
needs an explicit, tested conversion path before the tool accepts it.
"""

from __future__ import annotations

import sys

from ceassetlib.cli import main, parse_args
from ceassetlib.formats import AssetType
from ceassetlib.paths import make_project_paths
from ceassetlib.pipeline import (
    BuildOptions,
    build,
    convert_source,
    convert_texture_source,
    import_asset,
)
from ceassetlib.state import load_cache, load_registry


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
