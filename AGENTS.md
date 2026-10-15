# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

`git_wrapper` is a cross-platform (Windows + Linux) C++ CLI (split across
`src/`) that wraps `git commit`/`git push` to make several mistakes
structurally impossible:

1. **Foreign identity on commits.** Every commit is forced to author+committer
   `nava <nava@noreply.com>` via `-c user.name=` / `-c user.email=`, and the
   message is scrubbed of any `Co-Authored-By:`, `Claude-Session:`, "Generated
   with … Claude" line, or 🤖 emoji line before it ever reaches git.
2. **Wrong push order with submodules.** `push` pushes every submodule that has
   unpushed commits *before* the parent repo, so a fresh clone never ends up
   with the parent's gitlink pointing at a submodule commit the remote doesn't
   have yet.
3. **Wrong key/token for a remote.** `push`/`save` resolve the ssh key or https
   credential per remote (parent *and* each submodule) from a small rules file,
   learning an unambiguous first-seen key automatically — see
   [Per-remote credentials](#per-remote-credentials).

The build tooling lives in `scripts/` as a standard-library-only Python zipapp
(`scripts/git_wrapper_builder.pyz`) with its own test suite — see
[`scripts/AGENTS.md`](scripts/AGENTS.md) for its rules. There is no CI config.

## Build

```bat
scripts\Build.bat
```

```bash
./scripts/build.sh
```

Both are thin shims that build `scripts/git_wrapper_builder.pyz` (if needed) and
run it. The builder locates the toolchain, configures CMake with Ninja, and
builds. On Windows it imports `vcvars64.bat` — found via `vswhere` — so `cmake`
can see `cl.exe`; the ninja default is `C:\PPProgam\ninja_win\bin\ninja.exe`
(override with `$GIT_WRAPPER_NINJA` or `--ninja`). Outputs are
`build/windows/git_wrapper.exe` and `build/linux/git_wrapper`. Requires VS Build
Tools ("Desktop development with C++") or `cmake` + `ninja` + a C++ compiler,
plus `git` on `PATH`.

Run it with no arguments for a menu (microarchitecture, then build type), or
with a command: `build [--release|--debug] [--arch NAME]`, `all`, `package`
(universal/v3/v4/zen4 tarballs in `dist/linux/`), `smoke`. `--dry-run` prints the
exact cmake commands and runs nothing, so it works with no toolchain installed.
See [`scripts/README.md`](scripts/README.md).

Builder tests: `cd scripts && uv run pytest` (no toolchain needed — the workflow
tests use fake tools). Formatting and types: `uv format --check` and `uv check`,
also from `scripts/`. `uv` is the dev toolchain only; the `.pyz` itself needs
Python 3.12+ and nothing else.

## Architecture

Source lives in `src/`, one file per concern, wired together via `CMakeLists.txt`:

- **`config.h`** — `kName`/`kEmail` identity constants, the single place to
  change the forced commit identity.
- **`util.h`/`.cc`** — `toLower`, `trim`, `shellQuote` (safe POSIX-shell quoting
  for the values we hand git), `globMatch` (the `*`/`?` matcher used by the
  credential rules).
- **`process.h`** — the portable process interface: `git()` (spawn, no shell,
  for anything that mutates state: `add`, `commit`, `push`, `fetch`),
  `gitCapture()` (capture stdout, for read-only queries), `tempMessageFilePath()`,
  `homeDir()`/`configDir()`/`pathExists()`/`makeDirs()`/`restrictFilePerms()`
  (per-user config + secret storage), `selfExePath()` (the credential helper
  points git back at this binary), `readSecret()` (hidden token input),
  `toSshPath()`, and `ctxPrefix()` (trivial string formatting, defined inline
  since it's identical on every platform). Two implementations, chosen by
  `CMakeLists.txt` via `if(WIN32)`:
  - **`platform/windows/process_win.cc`** — `_spawnvp`, `_popen`/`_pclose`,
    `%TEMP%`/`%TMP%`, `%USERPROFILE%`/`%APPDATA%`, backslash paths.
  - **`platform/linux/process_posix.cc`** — `fork`/`execvp`/`waitpid`,
    `popen`/`pclose`, `$TMPDIR`/`/tmp`, `$HOME`/`$XDG_CONFIG_HOME`, forward-slash
    paths.
- **`sanitize.h`/`.cc`** — strips trailer/attribution lines from a commit
  message. This is the enforcement point for rule #1; extend the line-filter
  here if a new unwanted trailer pattern shows up.
- **`forge.h`/`.cc`** — pure remote-URL parsing (scp-like, `ssh://[user@]host[:port]`,
  `git+ssh://`, `https://`, `file://`, Windows drive-letter guard) into
  `{scheme,user,host,port,owner,path}` and the `host/owner` match key.
- **`credstore.h`/`.cc`** — the `credentials.conf` rules (match/kind/value),
  most-specific matching (`host/owner` then `host`, globs allowed), atomic save.
- **`secrets.h`/`.cc`** — the `0600` token file, keyed by `host/owner`.
- **`ssh_identity.h`/`.cc`** — candidate discovery from `~/.ssh` naming
  conventions and `~/.ssh/config`, and construction of `core.sshCommand`.
- **`resolve.h`/`.cc`** — `resolveForRepo(dir, store, opt)`: the one function
  that decides how a repo authenticates, returning the `-c …` argv fragment plus
  a human-readable reason. Used by `doPush` and by `keys whoami`.
- **`credhelper.h`/`.cc`** — git's credential-helper protocol
  (`get`/`store`/`erase`) over stdin/stdout; also the https auto-learn point.
- **`repo.h`/`.cc`** — repo introspection built on `process.h`:
  `currentBranch`, `submodulePaths` (direct/non-recursive submodules only —
  nested third-party submodules are intentionally not touched),
  `commitsAhead`, `verifyNoTrailers` (blocks pushing any commit that slipped
  through with a foreign trailer, e.g. one made with plain `git` outside this
  wrapper).
- **`commands.h`/`.cc`** — `doCommit`, `fetchRepo`/`pushRepo`, `doPush`.
- **`keyscmd.h`/`.cc`** — the `keys` subcommand (list/add/rm/sync/whoami/install).
- **`main.cc`** — `usage()` + `main()`, arg parsing and dispatch to the verbs
  (`commit`, `push`, `save`, `keys`, and the internal `credential`).

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

### Per-remote credentials

`doPush` calls `resolveForRepo` for the parent and every submodule, splicing the
returned `-c …` options into the `fetch`/`push` argv (so two repos under one
parent can use different keys). Resolution is deliberately conservative:

- **HTTPS** with a matching `https` rule → `-c credential.helper=` (reset) then
  `-c credential.helper=!<self> credential` and `credential.useHttpPath=true`.
  The helper (`credhelper.cc`) answers git's `get`/`store`/`erase` in-process,
  reading tokens from the `0600` `tokens` file (or `env:VAR`). A `store` call
  auto-learns the token.
- **SSH** → `-c core.sshCommand=ssh -i '<key>' -o IdentitiesOnly=yes`, where the
  key comes from: a `credentials.conf` rule > an unambiguous convention key in
  `~/.ssh` (recorded automatically, `--no-learn` opts out) > nothing. A repo's
  existing `core.sshCommand` or a matching `~/.ssh/config` Host block is never
  overridden; several convention candidates means "refuse to guess".

`keys whoami` runs the same resolution read-only and prints why. Rules live in
`<configDir>/credentials.conf`; override their location with
`GIT_WRAPPER_CREDENTIALS` (handy for tests).

### Tests

The pure logic (URL parsing, rule matching, quoting) is covered by
`tests/forge_test.cc`, built as `git_wrapper_tests` and run with
`ctest --test-dir <build dir>`. It needs no toolchain or network.

### Exit codes

`0` success (including "nothing to commit"), `1` a git operation failed or
detached HEAD, `2` usage error, `3` push aborted due to a foreign trailer on
an unpushed commit.

## Changing the forced identity

Edit `kName`/`kEmail` in `src/config.h`, then rebuild with `scripts\Build.bat`.
