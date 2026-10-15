"""Microarchitecture variants — the one place arch names live.

``GIT_WRAPPER_ARCH_LEVEL`` is consumed by the root ``CMakeLists.txt``, which
turns ``v2``/``v3``/``v4`` into ``-march=x86-64-vN`` and passes anything else
through to the compiler verbatim (``znver4``, ``native``, ...).
"""

from __future__ import annotations

import re
from dataclasses import dataclass

from .errors import BuildError

ARCH_LEVEL_FLAG = "GIT_WRAPPER_ARCH_LEVEL"
NATIVE = "native"
_ARCH_RE = re.compile(r"^[A-Za-z0-9_.+-]+$")


@dataclass(frozen=True)
class Variant:
    """One packaged build: a label, its ``-march`` value, and a description."""

    name: str
    arch: str
    description: str

    @property
    def cmake_args(self) -> tuple[str, ...]:
        """``-DGIT_WRAPPER_ARCH_LEVEL=...``, empty for the portable baseline."""
        if not self.arch:
            return ()
        return (f"-D{ARCH_LEVEL_FLAG}={self.arch}",)


UNIVERSAL = Variant("universal", "", "portable generic x86-64 baseline")
PACKAGE_VARIANTS: tuple[Variant, ...] = (
    UNIVERSAL,
    Variant("v3", "v3", "x86-64-v3 (AVX2/BMI2/FMA)"),
    Variant("v4", "v4", "x86-64-v4 (AVX-512)"),
    Variant("zen4", "znver4", "AMD Zen 4 tuning"),
)


def sanitize_arch(value: str) -> str:
    """Validate a user-supplied ``-march`` value.

    The value doubles as part of a directory name, so it is restricted to
    characters that are safe on both platforms — the shell version took
    whatever was typed and pasted it into a path.
    """
    arch = value.strip()
    if not arch:
        raise BuildError("--arch needs a value, e.g. v3, v4, znver4 or native")
    if not _ARCH_RE.match(arch) or ".." in arch:
        raise BuildError(
            f"refusing {value!r} as a -march value: it is also used as a directory "
            "name, so only letters, digits and . _ + - are allowed"
        )
    return arch


def arch_suffix(arch: str) -> str:
    """Build-directory suffix for a single-variant build.

    Mirrors the old ``build.sh``: nothing for the baseline, ``_native`` for
    ``-march=native``, ``_custom-<level>`` for anything else.
    """
    if not arch:
        return ""
    if arch == NATIVE:
        return "_native"
    return f"_custom-{arch}"
