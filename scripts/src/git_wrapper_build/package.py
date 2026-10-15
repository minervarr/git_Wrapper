"""Build every microarch variant and package the Release ones as tarballs.

Replaces ``build.sh --share``. Debug variants are built but deliberately not
packaged: a debug binary is not the kind of thing you hand someone.
"""

from __future__ import annotations

import tarfile
from collections.abc import Mapping, Sequence
from pathlib import Path

from . import builder, paths
from .errors import BuildError
from .toolchain import Toolchain
from .ui import note, ok, step
from .variants import PACKAGE_VARIANTS, Variant


def tarball_path(root: Path, variant: Variant) -> Path:
    """Where one variant's tarball belongs: ``dist/linux/git_wrapper-linux-v3.tar.gz``."""
    return paths.dist_dir(root) / f"git_wrapper-linux-{variant.name}.tar.gz"


def write_tarball(binary: Path, dest: Path, variant: Variant) -> Path:
    """Archive *binary* as ``git_wrapper-linux-<name>/git_wrapper`` inside *dest*.

    The arcname matches the old ``cp`` + ``tar -C dist`` staging, without the
    staging directory or the ``cp``.
    """
    if not binary.is_file():
        raise BuildError(f"nothing to package: {binary} was not produced")
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.unlink(missing_ok=True)
    with tarfile.open(dest, "w:gz") as archive:
        archive.add(binary, arcname=f"git_wrapper-linux-{variant.name}/{binary.name}")
    return dest


def build_all(
    root: Path,
    *,
    toolchain: Toolchain,
    target: str,
    build_type: str,
    jobs: int,
    env: Mapping[str, str] | None = None,
    extra: Sequence[str] = (),
    dry_run: bool = False,
    packaged: bool = True,
) -> list[Path]:
    """Build the four variants; return the tarballs written (empty when unpackaged).

    ``packaged=False`` is the ``all`` command: every variant, no tarballs.
    """
    if target != "linux":
        raise BuildError(
            "microarchitecture variants are a GCC/Clang feature — MSVC ignores "
            "GIT_WRAPPER_ARCH_LEVEL — so multi-variant builds are Linux-only"
        )

    debug = build_type == "Debug"
    root_dir = paths.share_root(root, debug=debug)
    pack = packaged and not debug
    tarballs: list[Path] = []

    for variant in PACKAGE_VARIANTS:
        variant_dir = root_dir / variant.name
        step(
            f"==> '{variant.name}' ({variant.description}) -> {paths.display(root, variant_dir)}"
        )
        binary = builder.build_variant(
            root,
            variant_dir,
            toolchain=toolchain,
            target=target,
            build_type=build_type,
            jobs=jobs,
            arch=variant.arch,
            extra=(*variant.cmake_args, *extra),
            env=env,
            dry_run=dry_run,
        )
        if pack:
            dest = write_tarball(binary, tarball_path(root, variant), variant)
            ok(f"    packaged {paths.display(root, dest)}")
        else:
            reason = "Debug, not packaged" if debug else "not packaged"
            note(f"    built {paths.display(root, binary)} ({reason})")

    return tarballs
