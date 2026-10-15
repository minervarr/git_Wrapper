"""End-to-end runs against a fake ``cmake``/``ninja``/``g++``.

A build machine may have no toolchain at all, and CI should not need one, so the
fakes are real executables that record their arguments and fabricate the binary.
That exercises the orchestration for real: directory naming, variant expansion,
tarball layout, dry runs, and smoke-test exit codes.
"""

from __future__ import annotations

import os
import sys
import tarfile
from pathlib import Path

import pytest

from git_wrapper_build import cli

pytestmark = pytest.mark.skipif(
    os.name == "nt", reason="the fakes are POSIX shell/py scripts"
)

FAKE_CMAKE = """#!{python}
import pathlib
import sys

log = pathlib.Path({log!r})
with log.open("a", encoding="utf-8") as handle:
    handle.write("cmake " + " ".join(sys.argv[1:]) + "\\n")

args = sys.argv[1:]
if args and args[0] == "--build":
    build = pathlib.Path(args[1])
    build.mkdir(parents=True, exist_ok=True)
    binary = build / "git_wrapper"
    binary.write_text("#!/bin/sh\\nexit ${{FAKE_BINARY_RC:-0}}\\n", encoding="utf-8")
    binary.chmod(0o755)
else:
    for index, arg in enumerate(args):
        if arg == "-B":
            pathlib.Path(args[index + 1]).mkdir(parents=True, exist_ok=True)
"""

FAKE_TOOL = """#!{python}
import pathlib
import sys

with pathlib.Path({log!r}).open("a", encoding="utf-8") as handle:
    handle.write(pathlib.Path(sys.argv[0]).name + " " + " ".join(sys.argv[1:]) + "\\n")
"""

FAKE_TOOLS = {"cmake": FAKE_CMAKE, "ninja": FAKE_TOOL, "g++": FAKE_TOOL}


@pytest.fixture
def fake_repo(tmp_path: Path, monkeypatch) -> tuple[Path, Path]:
    """A checkout plus a PATH containing the fake tools; returns (root, call log)."""
    root = tmp_path / "repo"
    root.mkdir()
    (root / "CMakeLists.txt").write_text("# fake\n", encoding="utf-8")
    (root / "manifest.json").write_text('{"version": "0.1.0"}\n', encoding="utf-8")

    bindir = tmp_path / "bin"
    bindir.mkdir()
    log = tmp_path / "calls.log"
    log.write_text("", encoding="utf-8")
    for name, template in FAKE_TOOLS.items():
        script = bindir / name
        script.write_text(
            template.format(python=sys.executable, log=str(log)), encoding="utf-8"
        )
        script.chmod(0o755)

    monkeypatch.setenv("PATH", f"{bindir}{os.pathsep}{os.environ['PATH']}")
    monkeypatch.chdir(root)
    return root, log


def test_build_produces_the_portable_binary(fake_repo):
    root, log = fake_repo
    assert cli.main(["build", "--release"]) == 0
    assert (root / "build" / "linux" / "git_wrapper").is_file()
    calls = log.read_text(encoding="utf-8")
    assert "-G Ninja" in calls
    assert "-DCMAKE_BUILD_TYPE=Release" in calls
    assert "--build" in calls


def test_build_debug_uses_its_own_directory(fake_repo):
    root, _ = fake_repo
    assert cli.main(["build", "--debug"]) == 0
    assert (root / "build" / "linux_debug" / "git_wrapper").is_file()


def test_custom_arch_lands_in_a_custom_directory_and_as_a_cache_variable(fake_repo):
    root, log = fake_repo
    assert cli.main(["build", "--arch", "znver4"]) == 0
    assert (root / "build" / "linux_custom-znver4" / "git_wrapper").is_file()
    assert "-DGIT_WRAPPER_ARCH_LEVEL=znver4" in log.read_text(encoding="utf-8")


def test_extra_cmake_arguments_pass_through(fake_repo):
    _, log = fake_repo
    assert cli.main(["build", "--", "-DGIT_WRAPPER_ARCH_LEVEL=v3"]) == 0
    assert "-DGIT_WRAPPER_ARCH_LEVEL=v3" in log.read_text(encoding="utf-8")


def test_package_writes_four_tarballs_with_one_binary_each(fake_repo):
    root, _ = fake_repo
    assert cli.main(["package"]) == 0
    dist = root / "dist" / "linux"
    assert sorted(path.name for path in dist.glob("*.tar.gz")) == [
        "git_wrapper-linux-universal.tar.gz",
        "git_wrapper-linux-v3.tar.gz",
        "git_wrapper-linux-v4.tar.gz",
        "git_wrapper-linux-zen4.tar.gz",
    ]
    with tarfile.open(dist / "git_wrapper-linux-v3.tar.gz") as archive:
        assert archive.getnames() == ["git_wrapper-linux-v3/git_wrapper"]


def test_all_variants_builds_everything_but_packages_nothing(fake_repo):
    root, _ = fake_repo
    assert cli.main(["all"]) == 0
    for variant in ("universal", "v3", "v4", "zen4"):
        assert (root / "build" / "linux_share" / variant / "git_wrapper").is_file()
    assert not (root / "dist").exists()


def test_package_debug_is_built_but_not_packaged(fake_repo):
    root, _ = fake_repo
    assert cli.main(["package", "--debug"]) == 0
    assert (root / "build" / "linux_share_debug" / "v4" / "git_wrapper").is_file()
    assert not (root / "dist").exists()


def test_smoke_passes_and_says_so(fake_repo, capsys):
    assert cli.main(["smoke"]) == 0
    assert "PASS" in capsys.readouterr().out


def test_smoke_fails_when_the_binary_fails(fake_repo, monkeypatch, capsys):
    monkeypatch.setenv("FAKE_BINARY_RC", "3")
    assert cli.main(["smoke"]) == 1
    assert "exited 3" in capsys.readouterr().err


def test_dry_run_runs_nothing(fake_repo, capsys):
    root, log = fake_repo
    assert cli.main(["build", "--dry-run"]) == 0
    assert not (root / "build").exists()
    assert log.read_text(encoding="utf-8") == ""
    output = capsys.readouterr().out
    assert "-G Ninja" in output
    assert "cmake" in output


def test_missing_toolchain_is_reported_in_one_message(
    tmp_path: Path, monkeypatch, capsys
):
    root = tmp_path / "empty-repo"
    root.mkdir()
    (root / "CMakeLists.txt").write_text("# fake\n", encoding="utf-8")
    (root / "manifest.json").write_text('{"version": "0.1.0"}\n', encoding="utf-8")
    empty = tmp_path / "empty"
    empty.mkdir()
    monkeypatch.setenv("PATH", str(empty))
    monkeypatch.chdir(root)

    assert cli.main(["build"]) == 1
    message = capsys.readouterr().err
    for expected in ("cmake", "ninja", "g++ or clang++"):
        assert expected in message


def test_dry_run_works_without_any_toolchain(tmp_path: Path, monkeypatch, capsys):
    root = tmp_path / "empty-repo"
    root.mkdir()
    (root / "CMakeLists.txt").write_text("# fake\n", encoding="utf-8")
    (root / "manifest.json").write_text('{"version": "0.1.0"}\n', encoding="utf-8")
    empty = tmp_path / "empty"
    empty.mkdir()
    monkeypatch.setenv("PATH", str(empty))
    monkeypatch.chdir(root)

    assert cli.main(["build", "--dry-run"]) == 0
    assert "cmake" in capsys.readouterr().out
