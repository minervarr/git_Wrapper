# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

`git_wrapper` is a cross-platform (Windows + Linux) C++ CLI (~290 lines
total, split across `src/`) that wraps `git commit`/`git push` to make two
mistakes structurally impossible:

1. **Foreign identity on commits.** Every commit is forced to author+committer
   `nava <nava@noreply.com>` via `-c user.name=` / `-c user.email=`, and the
   message is scrubbed of any `Co-Authored-By:`, `Claude-Session:`, "Generated
   with … Claude" line, or 🤖 emoji line before it ever reaches git.
2. **Wrong push order with submodules.** `push` pushes every submodule that has
   unpushed commits *before* the parent repo, so a fresh clone never ends up
   with the parent's gitlink pointing at a submodule commit the remote doesn't
   have yet.

There is no test suite and no CI config — verification is the manual
`scripts/smoke_test.ps1` (build, then check `git_wrapper help` exits 0).

## Build

```bat
scripts\Build.bat
```

This drives `scripts\build_msvc.ps1`, which: locates `vcvars64.bat` via
`vswhere`, configures CMake with Ninja (`C:\PPProgam\ninja_win\bin\ninja.exe`
— a fixed path, edit `build_msvc.ps1` if Ninja lives elsewhere), and builds.
Output is `build\windows\git_wrapper.exe`. Requires VS Build Tools ("Desktop
development with C++"), Ninja, and `git` on `PATH`.

```bash
./scripts/build.sh
```

Same CMake project, Ninja-based, on Linux. Output is `build/linux/git_wrapper`.
Requires `cmake`, `ninja`, and a C++ compiler (`g++`/`clang++`) on `PATH`.

There is no separate lint command.

## Architecture

Source lives in `src/`, one file per concern, wired together via `CMakeLists.txt`:

- **`config.h`** — `kName`/`kEmail` identity constants, the single place to
  change the forced commit identity.
- **`util.h`/`.cc`** — `toLower`, `trim`.
- **`process.h`** — the portable process interface: `git()` (spawn, no shell,
  for anything that mutates state: `add`, `commit`, `push`, `fetch`),
  `gitCapture()` (capture stdout, for read-only queries), `tempMessageFilePath()`,
  and `ctxPrefix()` (trivial string formatting, defined inline since it's
  identical on every platform). Two implementations, chosen by
  `CMakeLists.txt` via `if(WIN32)`:
  - **`platform/windows/process_win.cc`** — `_spawnvp`, `_popen`/`_pclose`,
    `%TEMP%`/`%TMP%`, backslash paths.
  - **`platform/linux/process_posix.cc`** — `fork`/`execvp`/`waitpid`,
    `popen`/`pclose`, `$TMPDIR`/`/tmp`, forward-slash paths.
- **`sanitize.h`/`.cc`** — strips trailer/attribution lines from a commit
  message. This is the enforcement point for rule #1; extend the line-filter
  here if a new unwanted trailer pattern shows up.
- **`repo.h`/`.cc`** — repo introspection built on `process.h`:
  `currentBranch`, `submodulePaths` (direct/non-recursive submodules only —
  nested third-party submodules are intentionally not touched),
  `commitsAhead`, `verifyNoTrailers` (blocks pushing any commit that slipped
  through with a foreign trailer, e.g. one made with plain `git` outside this
  wrapper).
- **`commands.h`/`.cc`** — `doCommit`, `fetchRepo`/`pushRepo`, `doPush`.
- **`main.cc`** — `usage()` + `main()`, arg parsing and dispatch to the three
  verbs: `commit`, `push`, `save` (= commit then push).

### Why a temp file for commit messages

Windows' process-spawn layer does not quote arguments, so a message with
spaces would be shattered into pathspecs if passed as an argv string. `doCommit`
writes the sanitized message to a temp file and commits with `git commit -F`
instead — this also makes shell metacharacters (`&`, `%`, quotes, newlines)
non-issues.

### Push safety invariants (`doPush`)

In order: fetch the parent's remote-tracking ref (keeps `--force-with-lease`
and the ahead-check from acting on stale info) → `verifyNoTrailers` on the
parent → for each submodule with actual unpushed commits, fetch + verify +
push it, aborting immediately (before touching the parent) if any submodule
push fails → push the parent last. Detached-HEAD repos/submodules are skipped
with a message rather than treated as errors, since pinned third-party
submodules are expected to be detached.

### Exit codes

`0` success (including "nothing to commit"), `1` a git operation failed or
detached HEAD, `2` usage error, `3` push aborted due to a foreign trailer on
an unpushed commit.

## Changing the forced identity

Edit `kName`/`kEmail` in `src/config.h`, then rebuild with `scripts\Build.bat`.
