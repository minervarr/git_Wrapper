"""The start menu: microarchitecture first, then build type.

A convenience over the flags, never a separate code path — the answers are the
same values the command line accepts.
"""

from __future__ import annotations

from .errors import BuildError
from .prompts import Reader, ask, choose
from .variants import sanitize_arch

UNIVERSAL_KEY = "universal"
NATIVE_KEY = "native"
CUSTOM_KEY = "custom"
ALL_KEY = "all"

ARCH_OPTIONS: tuple[tuple[str, str], ...] = (
    (UNIVERSAL_KEY, "portable generic x86-64 baseline"),
    (NATIVE_KEY, "tuned to this exact CPU (-march=native)"),
    (CUSTOM_KEY, "enter a specific -march value (v2/v3/v4/znver4/...)"),
    (ALL_KEY, "build universal/v3/v4/zen4 in one pass"),
)
BUILD_TYPE_OPTIONS: tuple[tuple[str, str], ...] = (
    ("Release", "optimised, the one you ship"),
    ("Debug", "asserts and symbols, unpackaged"),
)


def choose_arch(*, supported: bool = True, reader: Reader = input) -> tuple[str, bool]:
    """Return ``(arch, build_everything)``.

    *supported* is false under MSVC, where ``GIT_WRAPPER_ARCH_LEVEL`` is a
    GCC/Clang flag that CMake ignores — the menu says so instead of offering a
    choice that would silently do nothing.
    """
    if not supported:
        print(
            "\nmicroarchitecture targets are GCC/Clang-only: MSVC builds with its own defaults"
        )
        return "", False

    choice = choose(
        "Select microarchitecture target:",
        ARCH_OPTIONS,
        default=UNIVERSAL_KEY,
        reader=reader,
    )
    if choice == UNIVERSAL_KEY:
        return "", False
    if choice == ALL_KEY:
        return "", True
    if choice == NATIVE_KEY:
        return NATIVE_KEY, False

    while True:
        entered = ask("-march value (e.g. v3, v4, znver4)", reader=reader)
        if not entered:
            print("Please enter a -march value, or pick 1 for the portable build.")
            continue
        try:
            return sanitize_arch(entered), False
        except BuildError as exc:
            print(exc)


def choose_build_type(*, reader: Reader = input) -> str:
    """Return ``Release`` or ``Debug``."""
    return choose(
        "Select build type:", BUILD_TYPE_OPTIONS, default="Release", reader=reader
    )
