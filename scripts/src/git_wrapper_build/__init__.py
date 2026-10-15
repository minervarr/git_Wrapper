"""Build tooling for git_wrapper: one stdlib-only zipapp, Windows or Linux.

The C++ project at the repository root owns the build; this package only drives
``cmake``, ``ninja`` and the built binary. ``build_pyz.py`` zips it into
``git_wrapper_builder.pyz``, which runs on a bare interpreter — no third-party
imports, no ``pip``, no network.
"""

from __future__ import annotations

from .cli import main

__all__ = ["main"]
