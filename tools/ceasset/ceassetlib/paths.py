from __future__ import annotations

import shutil
from dataclasses import dataclass
from pathlib import Path


@dataclass
class ProjectPaths:
    root: Path
    assets_dir: Path
    state_dir: Path
    compiled_dir: Path
    registry_path: Path
    cache_path: Path


def make_project_paths(root: Path) -> ProjectPaths:
    project_root = root.resolve()
    assets_dir = project_root / "assets"
    state_dir = assets_dir / ".ceasset"
    return ProjectPaths(
        root=project_root,
        assets_dir=assets_dir,
        state_dir=state_dir,
        compiled_dir=assets_dir / "compiled",
        registry_path=state_dir / "registry.bin",
        cache_path=state_dir / "build-cache.bin",
    )


def generic_path(path: Path) -> str:
    return path.as_posix()


def stored_path(root: Path, path: Path) -> Path:
    absolute = path.resolve()
    try:
        return absolute.relative_to(root.resolve())
    except ValueError:
        return absolute


def project_path(paths: ProjectPaths, path: Path) -> Path:
    if path.is_absolute():
        return path
    return paths.root / path


def output_for_source(paths: ProjectPaths, source: Path, target_extension: str | None = None) -> Path:
    try:
        relative = source.resolve().relative_to((paths.assets_dir / "source").resolve())
    except ValueError:
        relative = Path(source.name)
    extension = target_extension if target_extension is not None else source.suffix.lower()
    return paths.compiled_dir / relative.with_suffix(extension.lower())


def output_dir_for_source(source: Path, output_root: Path) -> Path:
    try:
        relative = source.resolve().relative_to((output_root.parent / "source").resolve())
    except ValueError:
        return output_root / source.stem
    if relative.parent == Path("."):
        return output_root / source.stem
    if relative.parent.name.lower() == source.stem.lower():
        return output_root / relative.parent
    return output_root / relative.with_suffix("")


def atomic_write_bytes(path: Path, data: bytes | bytearray) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_bytes(data)
    temporary.replace(path)


def atomic_copyfile(source: Path, output: Path) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_name(output.name + ".tmp")
    shutil.copyfile(source, temporary)
    temporary.replace(output)
