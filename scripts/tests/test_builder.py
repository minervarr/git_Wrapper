"""The cmake command lines — the contract with CMakeLists.txt."""

from __future__ import annotations

from pathlib import Path

from git_wrapper_build import builder
from git_wrapper_build.toolchain import Toolchain

POSIX = Toolchain(
    cmake="/usr/bin/cmake", ninja="/usr/bin/ninja", compiler="/usr/bin/g++"
)
MSVC = Toolchain(
    cmake="cmake", ninja=r"C:\PPProgam\ninja_win\bin\ninja.exe", compiler="cl"
)
ROOT = Path("/repo")
BUILD = Path("/repo/build/linux")


def test_linux_configure_is_a_plain_ninja_configure():
    assert builder.configure_command(
        ROOT, BUILD, toolchain=POSIX, target="linux", build_type="Release"
    ) == [
        "/usr/bin/cmake",
        "-S",
        "/repo",
        "-B",
        "/repo/build/linux",
        "-G",
        "Ninja",
        "-DCMAKE_BUILD_TYPE=Release",
    ]


def test_arch_is_passed_as_a_cache_variable():
    cmd = builder.configure_command(
        ROOT, BUILD, toolchain=POSIX, target="linux", build_type="Debug", arch="znver4"
    )
    assert "-DCMAKE_BUILD_TYPE=Debug" in cmd
    assert "-DGIT_WRAPPER_ARCH_LEVEL=znver4" in cmd


def test_no_arch_variable_for_the_portable_build():
    cmd = builder.configure_command(
        ROOT, BUILD, toolchain=POSIX, target="linux", build_type="Release"
    )
    assert not any(arg.startswith("-DGIT_WRAPPER_ARCH_LEVEL") for arg in cmd)


def test_windows_pins_msvc_and_the_resolved_ninja():
    cmd = builder.configure_command(
        ROOT, BUILD, toolchain=MSVC, target="windows", build_type="Release"
    )
    assert f"-DCMAKE_MAKE_PROGRAM={MSVC.ninja}" in cmd
    assert "-DCMAKE_C_COMPILER=cl" in cmd
    assert "-DCMAKE_CXX_COMPILER=cl" in cmd


def test_dry_run_extra_arguments_are_appended_verbatim():
    cmd = builder.configure_command(
        ROOT,
        BUILD,
        toolchain=POSIX,
        target="linux",
        build_type="Release",
        extra=("-DCMAKE_CXX_FLAGS=-O3",),
    )
    assert cmd[-1] == "-DCMAKE_CXX_FLAGS=-O3"


def test_build_command_passes_the_job_count():
    assert builder.build_command(POSIX, BUILD, 16) == [
        "/usr/bin/cmake",
        "--build",
        "/repo/build/linux",
        "-j",
        "16",
    ]
