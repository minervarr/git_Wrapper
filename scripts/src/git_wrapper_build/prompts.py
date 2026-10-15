"""Small interactive prompt helpers.

Every helper takes an injectable ``reader`` (defaulting to :func:`input`) so the
menus can be tested without a terminal.
"""

from __future__ import annotations

from collections.abc import Callable, Sequence

Reader = Callable[[str], str]


def ask(prompt: str, *, default: str = "", reader: Reader = input) -> str:
    """Ask for a value; an empty answer returns *default*."""
    suffix = f" [{default}]" if default else ""
    answer = reader(f"{prompt}{suffix}: ").strip()
    return answer or default


def choose(
    title: str,
    options: Sequence[tuple[str, str]],
    *,
    default: str | None = None,
    reader: Reader = input,
) -> str:
    """Show a numbered menu; return the chosen key.

    *options* are ``(key, description)`` pairs, so callers keep their own values
    instead of mapping an index back afterwards. *default* is the key returned
    for an empty answer; ``None`` means an answer is required.
    """
    print(f"\n{title}")
    width = max(len(key) for key, _ in options)
    for index, (key, description) in enumerate(options, start=1):
        marker = " (default)" if key == default else ""
        print(f"  [{index}] {key:<{width}} — {description}{marker}")
    keys = [key for key, _ in options]
    while True:
        answer = reader("Enter choice: ").strip()
        if not answer and default is not None:
            return default
        if answer.isdigit() and 1 <= int(answer) <= len(options):
            return keys[int(answer) - 1]
        print(f"Please choose 1-{len(options)}.")
