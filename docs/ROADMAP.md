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

## Phase 4 — done
Replaced the four shell build scripts (`Build.bat` → `build_msvc.ps1`,
`build.sh`, `smoke_test.ps1`, `smoke_test.sh`) with one standard-library-only
Python zipapp, `scripts/git_wrapper_builder.pyz`, built by
`scripts/build_pyz.py`:

- `scripts/src/git_wrapper_build/` — `cli` (commands + exit codes), `menu`/
  `prompts` (injectable-reader menu, now available on Windows for the build
  type), `variants` (universal/v3/v4/zen4 table + `-march` validation),
  `paths` (repo root, build directories, `manifest.json`), `toolchain`,
  `msvc` (`vswhere` → `vcvars64` environment import), `builder`, `package`
  (`tarfile`), `smoke`, `process`, `ui`, `errors`.
- Commands: menu (no args) / `build [--release|--debug] [--arch NAME]` / `all`
  / `package` / `smoke`, plus `--dry-run` (prints the cmake lines and needs no
  toolchain, so it is usable on a bare machine) and `--ninja`.
- `uv` is the dev toolchain (`pyproject.toml`, `uv.lock`, `.python-version` =
  3.15); the artifact stays stdlib-only and `requires-python >= 3.12`, so the
  `.pyz` also runs on an older system Python. `Build.bat`/`build.sh` are now
  thin shims that prefer `uv` and fall back to `python`.
- 98 tests (`uv run pytest`) run with no toolchain at all: `test_workflow.py`
  drives the whole flow against fake `cmake`/`ninja`/`g++` executables that
  record their arguments and fabricate the binary.

Verified: `uv format --check` and `uv check` clean, the archive's exit codes
propagate (zipapp's own generated entry point swallowed them — `build_pyz.py`
writes `__main__.py` itself), `build --dry-run` on this machine (no
cmake/ninja/g++ installed) prints the expected commands, and `build` fails with
the single missing-toolchain message.

## Phase 5 — done
Per-remote credentials: `push`/`save` now choose the ssh key or https token from
the remote itself, for the parent and every submodule.

- New C++ modules: `forge` (URL parsing), `credstore` (`credentials.conf` rules),
  `secrets` (0600 token file), `ssh_identity` (candidate discovery +
  `core.sshCommand`), `resolve` (the single decision point), `credhelper` (git's
  credential protocol), `keyscmd` (the `keys` subcommand). `process.h` grew
  `homeDir`/`configDir`/`pathExists`/`makeDirs`/`restrictFilePerms`/
  `selfExePath`/`readSecret`/`toSshPath` on both platforms; `util` grew
  `shellQuote` and `globMatch`.
- Rules map `host` or `host/owner` (globs allowed) to a key/token; an
  unambiguous first-seen `~/.ssh` key is learned and saved. A repo's own
  `core.sshCommand` and matching `~/.ssh/config` Host blocks are never
  overridden.
- CLI: `git_wrapper keys [list|add|rm|sync|whoami|install]`, `push/save --key`,
  `--no-learn`, and the internal `credential` verb.
- Tests: `tests/forge_test.cc` (parsing, matching, quoting) built as
  `git_wrapper_tests`, run with `ctest`.

Verified on Linux against scratch repos and a local bare remote: commit identity
still forced, normal push unaffected, a GitHub‑named key auto-learned and then
injected (a stub `ssh` recorded `-i <key> -o IdentitiesOnly=yes`), `~/.ssh/config`
and `core.sshCommand` respected, `keys sync`/`keys install`/token `store`→`get`
round-trips working. Manifest version bumped to 0.2.0.

## Phase 6 — ideas, not committed
- Secret backends other than the plaintext 0600 file (OS keychain / libsecret).
- A `--explain`/dry-run on `push` that prints identity decisions before pushing.
- Wildcard-aware secret lookup so a `github.com/*` token rule works without a
  per-owner entry.
