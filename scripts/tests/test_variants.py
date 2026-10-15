"""The variant table and the ``-march`` value rules."""

from __future__ import annotations

import pytest

from git_wrapper_build.errors import BuildError
from git_wrapper_build.variants import (
    PACKAGE_VARIANTS,
    UNIVERSAL,
    arch_suffix,
    sanitize_arch,
)


def test_package_variants_are_the_four_known_builds():
    assert [variant.name for variant in PACKAGE_VARIANTS] == [
        "universal",
        "v3",
        "v4",
        "zen4",
    ]
    assert [variant.arch for variant in PACKAGE_VARIANTS] == ["", "v3", "v4", "znver4"]


def test_universal_passes_no_arch_flag():
    assert UNIVERSAL.cmake_args == ()


def test_other_variants_pass_a_cache_variable():
    assert PACKAGE_VARIANTS[-1].cmake_args == ("-DGIT_WRAPPER_ARCH_LEVEL=znver4",)


@pytest.mark.parametrize(
    ("arch", "expected"),
    [
        ("", ""),
        ("native", "_native"),
        ("v3", "_custom-v3"),
        ("znver4", "_custom-znver4"),
    ],
)
def test_arch_suffix_matches_the_old_build_script(arch, expected):
    assert arch_suffix(arch) == expected


@pytest.mark.parametrize(
    "value", ["v2", "v3", "v4", "znver4", "native", "x86-64-v3", "skylake+x"]
)
def test_sanitize_accepts_compiler_values(value):
    assert sanitize_arch(value) == value


@pytest.mark.parametrize(
    "value", ["", "   ", "../../etc", "v3/../../etc", "-march=v3", "v 3", "$(rm -rf /)"]
)
def test_sanitize_rejects_path_shaped_values(value):
    with pytest.raises(BuildError):
        sanitize_arch(value)


def test_sanitize_strips_surrounding_space():
    assert sanitize_arch("  v3 ") == "v3"
