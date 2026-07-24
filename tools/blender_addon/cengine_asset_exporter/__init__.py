#   _____ ______             _
#  / ____|  ____|           (_)
# | |    | |__   _ __   __ _ _ _ __   ___
# | |    |  __| | '_ \ / _` | | '_ \ / _ |
# | |____| |____| | | | (_| | | | | |  __/
#  \_____|______|_| |_|\__, |_|_| |_|\___|
#                       __/ |
#                      |___/

"""TODO: Briefly describe this module.

Author:
    Erik Coltey
"""

from __future__ import annotations

import sys
from pathlib import Path


bl_info = {
    "name": "CEngine Asset Authoring",
    "author": "CEngine",
    "version": (0, 2, 0),
    "blender": (4, 5, 0),
    "location": "3D View > CEngine",
    "description": "Author, preview, bake, and export CEngine assets.",
    "category": "Import-Export",
}

_PACKAGE_DIR = Path(__file__).resolve().parent
for _dependency_dir in (_PACKAGE_DIR, _PACKAGE_DIR / "vendor"):
    if _dependency_dir.is_dir() and str(_dependency_dir) not in sys.path:
        sys.path.insert(0, str(_dependency_dir))


def _forget_bundled_modules(package_names: set[str]) -> None:
    """TODO: Describe `_forget_bundled_modules`.

    Args:
        package_names: TODO: Describe this parameter.
    """
    package_path = str(_PACKAGE_DIR).lower()
    for name, module in tuple(sys.modules.items()):
        if name.split(".", 1)[0] not in package_names:
            continue
        if package_path in str(getattr(module, "__file__", "") or "").lower():
            del sys.modules[name]


_forget_bundled_modules({"ceassetlib", "PIL"})

from . import exporter
from .ui import register, unregister

__all__ = ["exporter", "register", "unregister"]
