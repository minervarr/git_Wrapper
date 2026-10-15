"""The bootstrap, and the archive it produces.

The archive is run with a bare interpreter from an unrelated directory: the
`.pyz` must carry its own entry point and must not need the checkout to print
its help.
"""

from __future__ import annotations

import subprocess
import sys
import zipfile
from pathlib import Path

import pytest

import build_pyz

SCRIPTS = Path(__file__).resolve().parents[1]


def _run(args: list[str], cwd: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, *args],
        cwd=cwd,
        capture_output=True,
        text=True,
        check=False,
    )


def test_is_stale_when_the_archive_is_missing(tmp_path: Path):
    assert build_pyz.is_stale(tmp_path / "nope.pyz") is True


def test_source_files_cover_the_bootstrap_and_the_package():
    names = {path.name for path in build_pyz.source_files()}
    assert {"build_pyz.py", "cli.py", "variants.py"} <= names
    assert not any("__pycache__" in path.parts for path in build_pyz.source_files())


def test_archive_builds_and_prints_help_outside_the_checkout(tmp_path: Path):
    out = tmp_path / "builder.pyz"
    result = _run([str(SCRIPTS / "build_pyz.py"), str(out)], tmp_path)
    assert result.returncode == 0, result.stderr
    assert out.is_file()
    assert out.read_bytes().startswith(b"#!")

    helped = _run([str(out), "--help"], tmp_path)
    assert helped.returncode == 0
    assert "usage" in helped.stdout.lower()

    with zipfile.ZipFile(out) as archive:
        names = archive.namelist()
    assert "git_wrapper_build/cli.py" in names
    assert "__main__.py" in names


def test_archive_exit_codes_reach_the_caller(tmp_path: Path):
    """zipapp's own entry point swallows the return value; ours must not."""
    out = tmp_path / "builder.pyz"
    assert _run([str(SCRIPTS / "build_pyz.py"), str(out)], tmp_path).returncode == 0
    assert _run([str(out), "--version"], tmp_path).returncode == 0
    assert _run([str(out), "bogus"], tmp_path).returncode == 2


def test_run_flag_executes_the_fresh_archive(tmp_path: Path):
    result = _run(
        [str(SCRIPTS / "build_pyz.py"), str(tmp_path / "b.pyz"), "--run", "--version"],
        tmp_path,
    )
    assert result.returncode == 0
    assert "git-wrapper-build" in result.stdout


def test_second_build_is_skipped_unless_forced(tmp_path: Path):
    out = tmp_path / "builder.pyz"
    assert _run([str(SCRIPTS / "build_pyz.py"), str(out)], tmp_path).returncode == 0
    skipped = _run([str(SCRIPTS / "build_pyz.py"), str(out)], tmp_path)
    assert "up to date" in skipped.stdout
    forced = _run([str(SCRIPTS / "build_pyz.py"), str(out), "--force"], tmp_path)
    assert "built" in forced.stdout


@pytest.mark.skipif(sys.platform == "win32", reason="the shell shim is POSIX")
def test_build_sh_shim_runs_the_archive(tmp_path: Path, monkeypatch):
    # Deliberately without uv on PATH: the shim must fall back to python3.
    monkeypatch.setenv("PATH", f"{Path(sys.executable).parent}:/usr/bin:/bin")
    result = subprocess.run(
        [str(SCRIPTS / "build.sh"), "--version"],
        cwd=tmp_path,
        capture_output=True,
        text=True,
        check=False,
    )
    assert result.returncode == 0, result.stderr
    assert "git-wrapper-build" in result.stdout
