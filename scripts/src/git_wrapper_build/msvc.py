"""Windows only: locate ``vcvars64.bat`` and import its environment.

``cmake`` cannot see ``cl.exe`` until vcvars has run, and a child process
cannot mutate its parent, so the environment is imported the way the old
PowerShell script did it: run ``vcvars64.bat`` in a throwaway ``cmd``, ask it
to dump the result with ``set``, and merge those ``KEY=VALUE`` lines into ours.

The parsing and merging are pure functions, so they are tested on any platform;
only :func:`load` needs Windows.
"""

from __future__ import annotations

import os
import subprocess
from collections.abc import Mapping
from dataclasses import dataclass, field
from pathlib import Path

from .errors import BuildError
from .ui import note

VCVARS_RELATIVE = Path("VC") / "Auxiliary" / "Build" / "vcvars64.bat"
VSWHERE_RELATIVE = Path("Microsoft Visual Studio") / "Installer" / "vswhere.exe"
FALLBACK_INSTALLATION = Path(
    r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"
)
VS_CPP_COMPONENT = "Microsoft.VisualStudio.Component.VC.Tools.x86.x64"


@dataclass(frozen=True)
class VcEnv:
    """The vcvars script that ran, and the environment it produced."""

    vcvars: Path
    env: dict[str, str] = field(default_factory=dict)


def vswhere_path(env: Mapping[str, str] | None = None) -> Path:
    """``vswhere.exe``, which lives under ``ProgramFiles(x86)``."""
    programs = (env or os.environ).get("ProgramFiles(x86)", r"C:\Program Files (x86)")
    return Path(programs) / VSWHERE_RELATIVE


def vcvars_candidate(installation: str | Path) -> Path:
    """``vcvars64.bat`` inside a Visual Studio installation directory."""
    return Path(installation) / VCVARS_RELATIVE


def parse_env_dump(text: str) -> dict[str, str]:
    """Parse the ``KEY=VALUE`` lines printed by ``cmd /c "...vcvars64.bat" && set``.

    Lines without ``=`` are skipped, as are cmd's hidden per-drive variables
    (``=C:=C:\\``, whose name starts with ``=``). Values may contain ``=``.
    """
    dumped: dict[str, str] = {}
    for line in text.splitlines():
        key, separator, value = line.partition("=")
        if not separator or not key or key.startswith("="):
            continue
        dumped[key] = value
    return dumped


def merge_env(base: Mapping[str, str], overlay: Mapping[str, str]) -> dict[str, str]:
    """Overlay *overlay* onto *base*, comparing names case-insensitively.

    Windows has a single ``Path``, so importing ``PATH`` from vcvars must
    replace the ``Path`` already there instead of leaving both in the dict.
    """
    merged = dict(base)
    names = {name.lower(): name for name in merged}
    for key, value in overlay.items():
        previous = names.get(key.lower())
        if previous is not None and previous != key:
            del merged[previous]
        merged[key] = value
        names[key.lower()] = key
    return merged


def _run(
    cmd: list[str], env: Mapping[str, str] | None
) -> subprocess.CompletedProcess[str]:
    """Spawn a command and capture its output (injectable for tests)."""
    return subprocess.run(
        cmd,
        capture_output=True,
        text=True,
        check=False,
        env=dict(env) if env is not None else None,
    )


def find_vcvars(
    *,
    env: Mapping[str, str] | None = None,
    run=_run,
) -> Path:
    """Ask vswhere for the newest VS with the C++ tools; else the 2022 Build Tools path."""
    vswhere = vswhere_path(env)
    if vswhere.is_file():
        result = run(
            [
                str(vswhere),
                "-latest",
                "-products",
                "*",
                "-requires",
                VS_CPP_COMPONENT,
                "-property",
                "installationPath",
            ],
            env,
        )
        if result.returncode == 0:
            first = next(
                (line.strip() for line in result.stdout.splitlines() if line.strip()),
                "",
            )
            if first:
                candidate = vcvars_candidate(first)
                if candidate.is_file():
                    return candidate
    if vcvars_candidate(FALLBACK_INSTALLATION).is_file():
        return vcvars_candidate(FALLBACK_INSTALLATION)
    raise BuildError(
        "could not find vcvars64.bat: install Visual Studio (or Build Tools) with the "
        "'Desktop development with C++' workload"
    )


def load(
    *,
    env: Mapping[str, str] | None = None,
    run=_run,
) -> VcEnv:
    """Import ``vcvars64.bat``'s environment so cmake can find ``cl.exe``."""
    if os.name != "nt":
        raise BuildError(
            "MSVC environment setup was requested on a non-Windows platform"
        )
    base = dict(os.environ if env is None else env)
    vcvars = find_vcvars(env=base, run=run)
    note(f"    using {vcvars}")
    result = run(["cmd.exe", "/c", "call", str(vcvars), ">nul", "&&", "set"], base)
    if result.returncode != 0:
        raise BuildError(
            f"running {vcvars} failed (exit {result.returncode})"
            + (f": {result.stderr.strip()}" if result.stderr.strip() else "")
        )
    imported = parse_env_dump(result.stdout)
    if not imported:
        raise BuildError(f"{vcvars} produced no environment variables")
    return VcEnv(vcvars=vcvars, env=merge_env(base, imported))
