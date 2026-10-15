# Project instructions

`git_wrapper`'s build tooling: a standard-library-only Python zipapp that
configures, builds, packages and smoke-tests the C++ project at the repository
root. It replaces `build_msvc.ps1`, `build.sh`, `smoke_test.ps1` and
`smoke_test.sh` with one artifact — `git_wrapper_builder.pyz` — that runs on
Windows and Linux alike.

The C++ sources are **not** this project's concern. `../CMakeLists.txt` and
`../src/` own the build; this package only drives them. See `README.md` here for
usage and `../docs/USAGE.md` for the manual.

## Development with uv

`uv` is the dev toolchain (environment, lockfile, running); it is **dev-only**.
The shipped artifact stays standard-library-only and is built as a zipapp.

- `uv sync` — create/refresh `.venv` from `uv.lock`.
- `uv run <cmd>` — run inside the project environment, e.g. `uv run pytest` or
  `uv run git-wrapper-build build --release`.
- `uv add <pkg>` / `uv add --dev <pkg>` — add runtime / dev dependencies.
- `uv lock` — refresh `uv.lock` after editing `pyproject.toml`.
- Commit `uv.lock`; never commit `.venv/` (already gitignored).

## Golden rules

- **Standard library only for runtime.** `[project].dependencies` must stay
  empty: the `.pyz` has to run on a bare interpreter with no `pip`, no `uv` and
  no network. Dev/test tools (`pytest`) belong in `[dependency-groups].dev`.
- **`build_pyz.py` stays outside the zipapp.** It is the bootstrap that builds
  the builder — inside the archive it would be a chicken-and-egg. It must keep
  working with a plain interpreter, never `uv`.
- **Never let zipapp write the entry point.** `zipapp.create_archive(main=...)`
  generates `<module>.<fn>()` and discards the return value, so the archive
  would always exit 0. `build_pyz.py` writes its own `__main__.py` with an
  explicit `sys.exit(main())`; `tests/test_pyz.py` guards this.
- **`uv` is never required to *consume* the artifact.** `Build.bat` and
  `build.sh` prefer `uv` when present and fall back to `python`/`py`.
- **Never hand-roll the C++ build.** No bare compiler invocations, ever: every
  path goes through `cmake -S .. -B <dir>`. Toolchain and microarch knobs live
  in `CMakeLists.txt` and are passed as `-D` flags.
- **`../manifest.json` stays the single source of truth for the tool's
  version.** Read it (`paths.version`); never copy the string into Python or
  docs. The builder's own `pyproject.toml` version is unrelated metadata for a
  thing that is never published.
- **Standard (GIL) interpreter, deliberately.** Nothing here does CPU-bound
  Python work — it waits on `cmake`, `ninja` and the built binary. The real
  parallelism is `ninja -j$(nproc)`, so a free-threaded build (`3.15t`) would
  buy nothing and slow startup down. `.python-version` pins the standard build.
- **Echo before you spawn.** Every command is printed before it runs, and
  `--dry-run` prints the exact cmake lines without running anything.

## Layout

- `src/git_wrapper_build/` — the package (this is what gets zipped)
  - `cli.py` — argparse surface, command resolution, dispatch, exit codes
  - `menu.py` — interactive arch menu, then build-type menu
  - `prompts.py` — `ask`/`choose` helpers with an injectable `reader`
  - `variants.py` — the universal/v3/v4/zen4 table, `-march` validation, and
    build-directory suffixes
  - `paths.py` — repo-root discovery, build directories, `manifest.json`
  - `toolchain.py` — locate `cmake`, `ninja`, a C++ compiler; job count
  - `msvc.py` — Windows only: `vswhere` → `vcvars64.bat` → environment import
  - `builder.py` — the only place cmake commands are spelled out
  - `package.py` — `tarfile` packaging into `../dist/linux/`
  - `smoke.py` — build, then assert `help` and `version` exit 0
  - `process.py` — one subprocess helper (echo, capture, dry run)
  - `ui.py` — step/note/warn/fail lines, colour on a real terminal
  - `errors.py` — `BuildError` (exit 1) and `UsageError` (exit 2)
- `tests/` — unit tests plus fake-`cmake`/`ninja`/`g++` workflow tests; they must
  run unprivileged and **must not need cmake, ninja, a compiler, or Windows**
- `build_pyz.py` — bootstrap that writes `git_wrapper_builder.pyz` (zipapp)

## Conventions

- Python 3.12+ syntax; `from __future__ import annotations`; type hints; small
  modules; no circular imports.
- Every prompt helper takes `reader: Reader = input` so the menus are testable
  without a TTY.
- Paths and names are derived in `paths.py`/`variants.py`, never inlined at a
  call site; the four variants stay declarative.
- Subprocesses go through `process.run`/`run_checked`; a failed step raises with
  the tool's own stderr attached, so cmake's message is not swallowed.
- Exit codes: `0` success, `1` a step failed, `2` the command line was wrong,
  `130` interrupted.

## Verification

- Tests: `uv run pytest`. Single test:
  `uv run pytest tests/test_variants.py::test_arch_suffix_matches_the_old_build_script`.
- Formatting and types: `uv format --check` and `uv check` (uv provisions ruff
  and ty; there is nothing to install).
- A real compiler is not needed for the test suite — `tests/test_workflow.py`
  runs the whole flow against fake tools that record their arguments.
- Before release: `uv run python build_pyz.py --force`, then on a clean
  interpreter `python git_wrapper_builder.pyz --help` and
  `python git_wrapper_builder.pyz smoke` (the latter does need a toolchain).
- No toolchain on the machine? `python git_wrapper_builder.pyz build --dry-run`
  still prints the exact commands.

## Version control

Commits in this repository go through `git_wrapper`, which forces the identity
to `nava <nava@noreply.com>` for both author and committer and strips
`Co-Authored-By:` / tool-generated trailers. Do not add them.

- Linux: `../build/linux/git_wrapper commit "<message>"`
- Windows: `..\build\windows\git_wrapper.exe commit "<message>"`
