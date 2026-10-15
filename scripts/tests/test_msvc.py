"""The MSVC environment import, tested through its pure parts.

``load()`` needs Windows, but parsing and merging are ordinary functions, so the
part that historically broke (``Path`` versus ``PATH``) is covered here.
"""

from __future__ import annotations

import os
import subprocess
from pathlib import Path

import pytest

from git_wrapper_build import msvc
from git_wrapper_build.errors import BuildError


def _completed(stdout: str, returncode: int = 0) -> subprocess.CompletedProcess[str]:
    return subprocess.CompletedProcess(["fake"], returncode, stdout, "")


def test_parse_env_dump_keeps_equals_signs_inside_values():
    assert msvc.parse_env_dump("FLAGS=/DFOO=bar\n") == {"FLAGS": "/DFOO=bar"}


def test_parse_env_dump_skips_junk_and_cmd_drive_variables():
    dumped = msvc.parse_env_dump("no separator here\n=C:=C:\\\nPATH=C:\\Windows\n\n")
    assert dumped == {"PATH": "C:\\Windows"}


def test_merge_env_replaces_path_case_insensitively():
    merged = msvc.merge_env({"Path": "old", "HOME": "/home/nava"}, {"PATH": "new"})
    assert merged["PATH"] == "new"
    assert "Path" not in merged
    assert len(merged) == 2
    assert merged["HOME"] == "/home/nava"


def test_merge_env_overlays_new_variables():
    assert msvc.merge_env({"A": "1"}, {"B": "2", "A": "3"}) == {"A": "3", "B": "2"}


def test_merge_env_keeps_the_overlay_spelling():
    merged = msvc.merge_env({"path": "old"}, {"Path": "new"})
    assert merged == {"Path": "new"}


def test_vswhere_path_uses_program_files_x86(tmp_path: Path):
    env = {"ProgramFiles(x86)": str(tmp_path)}
    assert msvc.vswhere_path(env) == tmp_path / msvc.VSWHERE_RELATIVE


def test_vcvars_candidate_appends_the_relative_path(tmp_path: Path):
    assert msvc.vcvars_candidate(tmp_path) == tmp_path / msvc.VCVARS_RELATIVE


def test_find_vcvars_asks_vswhere_first(tmp_path: Path):
    programs = tmp_path / "pf86"
    vswhere = programs / msvc.VSWHERE_RELATIVE
    vswhere.parent.mkdir(parents=True)
    vswhere.write_text("", encoding="utf-8")

    installation = tmp_path / "Visual Studio"
    expected = msvc.vcvars_candidate(installation)
    expected.parent.mkdir(parents=True)
    expected.write_text("", encoding="utf-8")

    calls: list[list[str]] = []

    def fake_run(cmd, env):  # noqa: ANN001, ARG001 - mirrors the _run signature
        calls.append(cmd)
        return _completed(f"\n{installation}\n")

    found = msvc.find_vcvars(env={"ProgramFiles(x86)": str(programs)}, run=fake_run)
    assert found == expected
    assert calls and "vswhere.exe" in Path(calls[0][0]).name
    assert msvc.VS_CPP_COMPONENT in calls[0]


def test_find_vcvars_reports_a_missing_toolchain(tmp_path: Path):
    with pytest.raises(BuildError, match="could not find vcvars64.bat"):
        msvc.find_vcvars(env={"ProgramFiles(x86)": str(tmp_path)})


def test_find_vcvars_falls_through_when_vswhere_finds_nothing(tmp_path: Path):
    programs = tmp_path / "pf86"
    vswhere = programs / msvc.VSWHERE_RELATIVE
    vswhere.parent.mkdir(parents=True)
    vswhere.write_text("", encoding="utf-8")
    with pytest.raises(BuildError, match="could not find vcvars64.bat"):
        msvc.find_vcvars(
            env={"ProgramFiles(x86)": str(programs)},
            run=lambda cmd, env: _completed("", 1),
        )


@pytest.mark.skipif(os.name == "nt", reason="the Windows branch is the working one")
def test_load_refuses_to_run_off_windows():
    with pytest.raises(BuildError, match="non-Windows"):
        msvc.load()
