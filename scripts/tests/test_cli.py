"""Argument parsing and the rules about which command may be combined with what."""

from __future__ import annotations

import io
from pathlib import Path

import pytest

from git_wrapper_build import cli
from git_wrapper_build.errors import UsageError

from conftest import ScriptedReader


@pytest.fixture
def repo(tmp_path: Path, monkeypatch) -> Path:
    (tmp_path / "CMakeLists.txt").write_text("# fake\n", encoding="utf-8")
    (tmp_path / "manifest.json").write_text('{"version": "2.5.0"}\n', encoding="utf-8")
    monkeypatch.chdir(tmp_path)
    return tmp_path


def parse(argv: list[str]):
    """Parse like :func:`cli.main` does, returning ``(args, leftovers)``."""
    return cli.build_parser().parse_known_args(argv)


def test_defaults_are_empty_so_the_menu_can_decide():
    args, extra = parse([])
    assert (args.command, args.build_type, args.arch, args.dry_run, extra) == (
        None,
        None,
        "",
        False,
        [],
    )


def test_version_comes_from_the_manifest(repo: Path, capsys):
    assert cli.main(["--version"]) == 0
    assert "2.5.0" in capsys.readouterr().out


def test_version_outside_a_checkout_says_unknown(tmp_path: Path, monkeypatch, capsys):
    monkeypatch.chdir(tmp_path)
    # sys.argv[0] is another way to locate the checkout; neutralise it so this
    # really is "no checkout anywhere near".
    monkeypatch.setattr("sys.argv", ["/nonexistent/git-wrapper-build"])
    assert cli.main(["--version"]) == 0
    assert "unknown" in capsys.readouterr().out


@pytest.mark.parametrize("argv", [["bogus"], ["build", "--oops"]])
def test_usage_errors_exit_two(argv):
    with pytest.raises(SystemExit) as exit_info:
        cli.main(argv)
    assert exit_info.value.code == 2


class _Tty(io.StringIO):
    """A fake stdin that claims to be a terminal."""

    def isatty(self) -> bool:
        return True


def test_menu_answers_become_a_command(repo: Path, monkeypatch):
    """The menu is the only interactive path; drive it with a scripted reader."""
    monkeypatch.setattr("sys.stdin", _Tty())
    args, _ = parse([])
    reader = ScriptedReader("4", "2")  # all variants, Debug
    assert cli._resolve_command(args, "linux", reader=reader) == ("all", "", "Debug")


def test_menu_single_variant_answer(repo: Path, monkeypatch):
    monkeypatch.setattr("sys.stdin", _Tty())
    args, _ = parse([])
    reader = ScriptedReader("3", "znver4", "1")  # custom arch, Release
    assert cli._resolve_command(args, "linux", reader=reader) == (
        "build",
        "znver4",
        "Release",
    )


def test_menu_on_windows_skips_the_arch_question(repo: Path, monkeypatch, capsys):
    monkeypatch.setattr("sys.stdin", _Tty())
    args, _ = parse([])
    reader = ScriptedReader("2")  # build type only
    assert cli._resolve_command(args, "windows", reader=reader) == (
        "build",
        "",
        "Debug",
    )
    assert "GCC/Clang" in capsys.readouterr().out


def test_passthrough_strips_the_double_dash():
    parser = cli.build_parser()
    assert cli._passthrough(parser, ["--", "-DFOO=1"]) == ["-DFOO=1"]


def test_resolve_without_a_terminal_builds_the_portable_release(
    repo: Path, monkeypatch
):
    monkeypatch.setattr("sys.stdin", io.StringIO())
    args, _ = parse([])
    assert cli._resolve_command(args, "linux") == ("build", "", "Release")


def test_resolve_rejects_the_menu_without_a_terminal(repo: Path, monkeypatch):
    monkeypatch.setattr("sys.stdin", io.StringIO())
    args, _ = parse(["menu"])
    with pytest.raises(UsageError, match="needs a terminal"):
        cli._resolve_command(args, "linux")


def test_resolve_keeps_an_explicit_command_and_debug_flag(repo: Path):
    args, _ = parse(["build", "--debug"])
    assert cli._resolve_command(args, "linux") == ("build", "", "Debug")


def test_resolve_rejects_arch_on_windows(repo: Path):
    args, _ = parse(["build", "--arch", "v3"])
    with pytest.raises(UsageError, match="GCC/Clang feature"):
        cli._resolve_command(args, "windows")


def test_resolve_rejects_package_on_windows(repo: Path):
    args, _ = parse(["package"])
    with pytest.raises(UsageError, match="Linux-only"):
        cli._resolve_command(args, "windows")


@pytest.mark.parametrize("command", ["all", "package"])
def test_resolve_rejects_arch_with_a_multi_variant_command(repo: Path, command):
    args, _ = parse([command, "--arch", "v3"])
    with pytest.raises(UsageError, match="drop --arch"):
        cli._resolve_command(args, "linux")


def test_resolve_rejects_arch_with_smoke(repo: Path):
    args, _ = parse(["smoke", "--arch", "v3"])
    with pytest.raises(UsageError, match="drop --arch"):
        cli._resolve_command(args, "linux")


def test_resolve_rejects_a_relative_arch_value(repo: Path):
    args, _ = parse(["build", "--arch", "../../etc"])
    with pytest.raises(Exception, match="refusing"):
        cli._resolve_command(args, "linux")
