"""Repository discovery, build directory naming, and the manifest."""

from __future__ import annotations

from pathlib import Path

import pytest

from git_wrapper_build import paths
from git_wrapper_build.errors import BuildError


@pytest.fixture
def repo(tmp_path: Path) -> Path:
    (tmp_path / "CMakeLists.txt").write_text("# fake\n", encoding="utf-8")
    (tmp_path / "manifest.json").write_text('{"version": "9.9.9"}\n', encoding="utf-8")
    return tmp_path


def test_find_repo_root_walks_up_from_a_nested_directory(repo: Path):
    nested = repo / "scripts" / "src"
    nested.mkdir(parents=True)
    assert paths.find_repo_root(nested) == repo


def test_find_repo_root_needs_both_marker_files(tmp_path: Path):
    (tmp_path / "CMakeLists.txt").write_text("", encoding="utf-8")
    with pytest.raises(BuildError, match="not inside a git_wrapper checkout"):
        paths.find_repo_root(tmp_path)


def test_version_comes_from_the_manifest(repo: Path):
    assert paths.version(repo) == "9.9.9"


def test_broken_manifest_is_a_build_error(repo: Path):
    (repo / "manifest.json").write_text("{not json", encoding="utf-8")
    with pytest.raises(BuildError, match="not valid JSON"):
        paths.load_manifest(repo)


@pytest.mark.parametrize(
    ("target", "arch", "debug", "expected"),
    [
        ("linux", "", False, "build/linux"),
        ("linux", "", True, "build/linux_debug"),
        ("linux", "native", False, "build/linux_native"),
        ("linux", "v3", True, "build/linux_custom-v3_debug"),
        ("windows", "", False, "build/windows"),
        ("windows", "", True, "build/windows_debug"),
    ],
)
def test_single_build_dir(repo: Path, target, arch, debug, expected):
    assert (
        paths.display(
            repo, paths.single_build_dir(repo, target, arch=arch, debug=debug)
        )
        == expected
    )


@pytest.mark.parametrize(
    ("debug", "expected"),
    [(False, "build/linux_share"), (True, "build/linux_share_debug")],
)
def test_share_root(repo: Path, debug, expected):
    assert paths.display(repo, paths.share_root(repo, debug=debug)) == expected


def test_dist_dir_and_binary_names(repo: Path):
    assert paths.display(repo, paths.dist_dir(repo)) == "dist/linux"
    assert paths.binary_name("linux") == "git_wrapper"
    assert paths.binary_name("windows") == "git_wrapper.exe"
    assert (
        paths.display(repo, paths.binary_path(repo / "build/linux", "linux"))
        == "build/linux/git_wrapper"
    )


def test_display_falls_back_to_an_absolute_path(repo: Path, tmp_path_factory):
    outside = tmp_path_factory.mktemp("outside")
    assert paths.display(repo, outside) == str(outside)
