"""Repository layout: the root, the build directories, the manifest.

Everything derived from ``CMakeLists.txt`` + ``build.sh``/``build_msvc.ps1``
naming lives here, so a directory rename happens in one file.
"""

from __future__ import annotations

import json
import os
import sys
from pathlib import Path
from typing import Any

from .errors import BuildError
from .variants import arch_suffix

CMAKE_FILE = "CMakeLists.txt"
MANIFEST_FILE = "manifest.json"
LINUX_BINARY = "git_wrapper"
WINDOWS_BINARY = "git_wrapper.exe"

_BUILD_ROOT = "build"


def current_target() -> str:
    """The platform being targeted: ``windows`` or ``linux``."""
    if os.name == "nt":
        return "windows"
    if sys.platform == "darwin":
        raise BuildError(
            "macOS is not supported: git_wrapper ships for Windows and Linux only"
        )
    return "linux"


def binary_name(target: str) -> str:
    """``git_wrapper.exe`` on Windows, ``git_wrapper`` elsewhere."""
    return WINDOWS_BINARY if target == "windows" else LINUX_BINARY


def _default_bases() -> list[Path]:
    """Where to start looking for the checkout: the cwd, then the launcher.

    The launcher matters for the ``.pyz``: run as ``scripts/git_wrapper_builder.pyz``
    from an unrelated directory, its own location is what finds the root.
    """
    bases = [Path.cwd().resolve()]
    argv0 = sys.argv[0] if sys.argv and sys.argv[0] else ""
    if argv0:
        try:
            bases.append(Path(argv0).resolve().parent)
        except OSError:
            pass
    return bases


def find_repo_root(start: Path | None = None) -> Path:
    """Walk up to the directory holding both ``CMakeLists.txt`` and ``manifest.json``."""
    bases = [Path(start).resolve()] if start is not None else _default_bases()
    for base in bases:
        for parent in (base, *base.parents):
            if (parent / CMAKE_FILE).is_file() and (parent / MANIFEST_FILE).is_file():
                return parent
    raise BuildError(
        f"not inside a git_wrapper checkout: no directory with both {CMAKE_FILE} "
        f"and {MANIFEST_FILE} above {', '.join(str(b) for b in bases)}"
    )


def single_build_dir(
    root: Path, target: str, *, arch: str = "", debug: bool = False
) -> Path:
    """Build directory for one variant: ``build/linux``, ``build/linux_custom-v3``..."""
    if target == "windows":
        name = "windows_debug" if debug else "windows"
        return root / _BUILD_ROOT / name
    return root / _BUILD_ROOT / f"linux{arch_suffix(arch)}{'_debug' if debug else ''}"


def share_root(root: Path, *, debug: bool = False) -> Path:
    """Root of the multi-variant builds: ``build/linux_share``."""
    return root / _BUILD_ROOT / ("linux_share_debug" if debug else "linux_share")


def dist_dir(root: Path) -> Path:
    """Where packaged tarballs are written."""
    return root / "dist" / "linux"


def binary_path(build_dir: Path, target: str) -> Path:
    """The executable a build directory is expected to contain."""
    return build_dir / binary_name(target)


def display(root: Path, path: Path) -> str:
    """*path* relative to *root* when possible, for shorter console output."""
    try:
        return str(path.relative_to(root))
    except ValueError:
        return str(path)


def load_manifest(root: Path) -> dict[str, Any]:
    """Parse ``manifest.json`` — the single source of truth for the version."""
    try:
        return json.loads((root / MANIFEST_FILE).read_text(encoding="utf-8"))
    except OSError as exc:
        raise BuildError(f"cannot read {MANIFEST_FILE}: {exc}") from exc
    except ValueError as exc:
        raise BuildError(f"{MANIFEST_FILE} is not valid JSON: {exc}") from exc


def version(root: Path) -> str:
    """The tool's version string, straight from ``manifest.json``."""
    return str(load_manifest(root).get("version", "unknown"))
