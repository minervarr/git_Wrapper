# Roadmap

## Phase 0 — done
Windows CLI (`commit` / `push` / `save`) functional, built via
`scripts/Build.bat` → `scripts/build_msvc.ps1` → CMake/Ninja/MSVC.

## Phase 1 — done
- `manifest.json`: name/version/description/platforms metadata.
- `scripts/build.sh`: native Linux build entrypoint, mirrors `build_msvc.ps1`.
- Platform-split build output: `build/windows/`, `build/linux/`.
- Untracked the committed `git_wrapper.exe` binary; added it to `.gitignore`.
- `.editorconfig` for formatting consistency without a linter dependency.
- `scripts/smoke_test.ps1`: builds, then verifies `git_wrapper help` exits 0.
- `README.md` / `docs/USAGE.md` rebalanced so they stop duplicating each other.
- Code split by concern into `src/` (`config`, `util`, `git_process`,
  `sanitize`, `repo`, `commands`, `main`); build scripts moved to `scripts/`,
  long-form docs moved to `docs/`.

## Phase 2 — done
Made the source portable. `src/process.h` is now the interface `repo.cc`/
`commands.cc` call (`git()`, `gitCapture()`, `tempMessageFilePath()`,
`ctxPrefix()`), with two implementations selected by CMake (`if(WIN32)`):

- `src/platform/windows/process_win.cc` — `_spawnvp`/`_popen`/`%TEMP%`/
  backslash paths (the original `git_process.cc` code, moved as-is).
- `src/platform/linux/process_posix.cc` — `fork`/`execvp`/`waitpid`,
  `popen`/`pclose`, `$TMPDIR` or `/tmp`, forward-slash paths.

Verified with a direct `g++ -Isrc ...` compile+link (no cmake/ninja needed
for the check) and a functional test against a scratch repo + local bare
remote: `commit` correctly attributes `nava <nava@noreply.com>` via the
fork/exec path, and `push` succeeds via the popen-based `gitCapture()` path.

## Phase 3 — done
- `scripts/smoke_test.sh`: Linux counterpart to `smoke_test.ps1` — builds via
  `scripts/build.sh`, then verifies `git_wrapper help` and `git_wrapper
  version` both exit 0.
- `git_wrapper version` / `-v` / `--version`: CMake reads `version` out of
  `manifest.json` at configure time (`string(JSON ... GET)`), generates
  `build/<platform>/generated/version.h` from `src/version.h.in`, and
  `main.cc` prints it. `manifest.json` stays the single source of truth for
  the version string.

Verified end-to-end with a real `cmake`/`ninja` build (this machine had
`cmake`/`ninja` installed by this point): `git_wrapper version` prints
`git_wrapper 0.1.0`, and `scripts/smoke_test.sh` passes.

## Phase 4 — ideas, not committed
Nothing yet. Add future work here as it comes up.
