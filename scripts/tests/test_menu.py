"""The interactive menu, driven by scripted readers instead of a terminal."""

from __future__ import annotations

import pytest

from git_wrapper_build import menu
from git_wrapper_build.prompts import choose

from conftest import ScriptedReader


def test_arch_menu_default_is_the_portable_build():
    assert menu.choose_arch(reader=ScriptedReader("")) == ("", False)


def test_arch_menu_native():
    assert menu.choose_arch(reader=ScriptedReader("2")) == ("native", False)


def test_arch_menu_all_means_every_variant():
    assert menu.choose_arch(reader=ScriptedReader("4")) == ("", True)


def test_arch_menu_custom_value():
    assert menu.choose_arch(reader=ScriptedReader("3", "znver4")) == ("znver4", False)


def test_arch_menu_reprompts_after_a_path_shaped_value(capsys):
    reader = ScriptedReader("3", "../etc", "v3")
    assert menu.choose_arch(reader=reader) == ("v3", False)
    assert len(reader.prompts) == 3
    assert "only letters, digits" in capsys.readouterr().out


def test_arch_menu_reprompts_after_an_empty_value():
    reader = ScriptedReader("3", "", "v4")
    assert menu.choose_arch(reader=reader) == ("v4", False)


def test_windows_is_told_microarch_is_a_gcc_feature(capsys):
    assert menu.choose_arch(supported=False, reader=ScriptedReader()) == ("", False)
    assert "GCC/Clang" in capsys.readouterr().out


@pytest.mark.parametrize(
    ("answer", "expected"), [("", "Release"), ("1", "Release"), ("2", "Debug")]
)
def test_build_type_menu(answer, expected):
    assert menu.choose_build_type(reader=ScriptedReader(answer)) == expected


def test_choose_reprompts_past_an_out_of_range_answer():
    reader = ScriptedReader("9", "2")
    options = (("a", "first"), ("b", "second"))
    assert choose("Pick:", options, default="a", reader=reader) == "b"
