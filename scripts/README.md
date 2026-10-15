# git_wrapper build tooling

One artifact that builds `git_wrapper` on Windows and Linux:
`git_wrapper_builder.pyz`. It replaces `build_msvc.ps1`, `build.sh`,
`smoke_test.ps1` and `smoke_test.sh` — same behaviour, one code path, and the
microarchitecture menu now exists on both platforms.

Why Python: the C++ side is a handful of files, but the *build* side was four
scripts in two shells repeating the same three steps (check toolchain →
configure → build). Collapsed into a standard-library-only package, there is
one place to fix a bug, one place to add a variant, and one file to hand
someone.

## Quick start

Windows — double-click, or from a terminal:

```bat
scripts\Build.bat
```

Linux:

```bash
./scripts/build.sh
```

Both prefer `uv` when it is installed (uv can fetch its own Python), fall back
to `python3`/`py`, build `git_wrapper_builder.pyz` if it is missing, then run it
with no arguments — which opens the menu. Non-interactive use:

```bash
python git_wrapper_builder.pyz build --release
python git_wrapper_builder.pyz package
```

## The menu

```
Select microarchitecture target:
  [1] universal — portable generic x86-64 baseline (default)
  [2] native    — tuned to this exact CPU (-march=native)
  [3] custom    — enter a specific -march value (v2/v3/v4/znver4/...)
  [4] all       — build universal/v3/v4/zen4 in one pass

Select build type:
  [1] Release — optimised, the one you ship (default)
  [2] Debug   — asserts and symbols, unpackaged
```

The answers are exactly the flag values below, so the menu is a convenience over
the CLI and not a second code path. Under MSVC the microarch question is skipped:
`GIT_WRAPPER_ARCH_LEVEL` is a GCC/Clang flag that CMake ignores, and the menu
says so instead of offering a choice that would silently do nothing.

## Commands

| command | does | output |
|---|---|---|
| *(no args)* | menu when stdin is a terminal; portable Release otherwise | see below |
| `build` | one variant, Release | `build/linux/git_wrapper` |
| `build --debug` | one variant, Debug | `build/linux_debug/git_wrapper` |
| `build --arch v3` | one variant, `-march=x86-64-v3` | `build/linux_custom-v3/git_wrapper` |
| `build --arch native` | `-march=native` | `build/linux_native/git_wrapper` |
| `all` | all four variants, no tarballs | `build/linux_share/<variant>/git_wrapper` |
| `package` | all four variants + tarballs | `dist/linux/git_wrapper-linux-<variant>.tar.gz` |
| `smoke` | build, then assert `help`/`version` exit 0 | — |
| `menu` | the menu explicitly (needs a terminal) | depends on the answers |

Other flags: `--release` / `--debug`, `--ninja <path>`, `--dry-run` (print the
exact cmake commands and run nothing — it works with no toolchain installed at
all), `--version` (from `../manifest.json`). Extra CMake cache entries pass
through after `--`:

```bash
python git_wrapper_builder.pyz build -- -DCMAKE_CXX_FLAGS=-flto
```

Windows always configures `build/windows` (`build/windows_debug` for
`--debug`) and produces `git_wrapper.exe`. `all` and `package` are Linux-only,
because the four variants are compiler flags MSVC does not have.

`package` refuses to produce a Debug tarball: a Debug build is useful locally
and nothing you hand someone.

## Development

`uv` is the dev toolchain; it is dev-only, and the artifact never needs it.

```bash
uv sync                              # .venv from uv.lock (fetches Python 3.15 if absent)
uv run git-wrapper-build             # the menu
uv run pytest                        # unit tests, no toolchain required
uv format --check && uv check        # formatting + types (both provisioned by uv)
uv run python build_pyz.py           # -> git_wrapper_builder.pyz
```

`.python-version` pins the dev interpreter (3.15) while `requires-python =
">=3.12"` is the floor the `.pyz` promises to run on, so the artifact stays
usable with an older system Python. The interpreter is the standard build, not
free-threaded: this package only spawns subprocesses, so the GIL is not in the
picture — `ninja -j$(nproc)` is where the parallelism lives.

## Layout

```
Build.bat / build.sh          thin entry shims (prefer uv, then python)
build_pyz.py                  bootstrap: zips the package into the .pyz
pyproject.toml / uv.lock      uv dev toolchain; runtime dependencies are empty
.python-version               dev interpreter pin
src/git_wrapper_build/        the package that goes into the .pyz
tests/                        unit + fake-toolchain tests (no cmake/ninja needed)
git_wrapper_builder.pyz       build output, gitignored
```

Inside the package: `cli.py` (flags and dispatch), `menu.py`/`prompts.py` (the
interactive bits), `variants.py` (arch table), `paths.py` (repo root, build
directories, manifest), `toolchain.py` (`cmake`/`ninja`/compiler discovery),
`msvc.py` (Windows `vcvars64.bat` import), `builder.py` (the cmake commands),
`package.py` (tarballs), `smoke.py`, `ui.py`, `process.py`.

## Requirements

- `uv` (recommended) or Python 3.12+ on `PATH`.
- `cmake`, `ninja`, and `g++`/`clang++` (Linux), or Visual Studio Build Tools
  with **Desktop development with C++** (Windows).
- `git` on `PATH`.

Windows finds `vcvars64.bat` through `vswhere`, so any VS 2019+/2022 install
with the C++ tools works — no hardcoded installation path except the 2022 Build
Tools fallback. Ninja defaults to `C:\PPProgam\ninja_win\bin\ninja.exe`;
override it with `GIT_WRAPPER_NINJA` or `--ninja <path>` if yours lives
elsewhere.

## Migrating from the shell scripts

| old | new |
|---|---|
| `scripts\Build.bat` → `build_msvc.ps1` | `scripts\Build.bat` (→ `git_wrapper_builder.pyz build`) |
| `./scripts/build.sh` | `./scripts/build.sh` (shim) or `python git_wrapper_builder.pyz` |
| `./scripts/build.sh --share` | `python git_wrapper_builder.pyz package` |
| `./scripts/build.sh --debug` | `python git_wrapper_builder.pyz build --debug` |
| `./scripts/smoke_test.sh` / `smoke_test.ps1` | `python git_wrapper_builder.pyz smoke` |
