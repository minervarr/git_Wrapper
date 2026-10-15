"""Shared test helpers."""

from __future__ import annotations


class ScriptedReader:
    """A ``reader`` that hands out pre-set answers and records the prompts.

    The package's prompt helpers all take an injectable reader precisely so the
    interactive paths can be tested without a terminal.
    """

    def __init__(self, *answers: str) -> None:
        self.answers = list(answers)
        self.prompts: list[str] = []

    def __call__(self, prompt: str) -> str:
        self.prompts.append(prompt)
        if not self.answers:
            raise AssertionError(f"unexpected extra prompt: {prompt!r}")
        return self.answers.pop(0)
