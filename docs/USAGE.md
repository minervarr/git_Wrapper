# git_wrapper — Manual

A small command-line tool that wraps `git commit` and `git push` so two mistakes
can't happen again:

- a commit ever showing a contributor other than **nava <nava@noreply.com>**
  (no `Co-Authored-By:` trailers, no stray author/committer identity), and
- pushing a repository with submodules in the wrong order (parent before its
  submodules), which breaks fresh clones.

You drive it with three verbs: `commit`, `push`, `save`.

---

## 1. Build

The build tooling is a standard-library-only Python zipapp,
`git_wrapper_builder.pyz`, that drives CMake on Windows and Linux. You never
type a compiler command.

### Windows

From the tool's folder:

```bat
cd C:\Users\incxiuefb\Documents\Files\clone\git_wrapper
scripts\Build.bat
```

`Build.bat` builds `scripts\git_wrapper_builder.pyz` if needed, then runs it,
which yields:

```
git_wrapper\build\windows\git_wrapper.exe
```

Ninja is expected at `C:\PPProgam\ninja_win\bin\ninja.exe`. If yours lives
elsewhere, set `GIT_WRAPPER_NINJA` or pass `--ninja <path>`.

### Linux

```bash
./scripts/build.sh
```

Same flow, yielding `build/linux/git_wrapper`.

### Choosing what to build

Run with no arguments from a terminal for a menu — microarchitecture first
(Universal, Native, Custom `-march`, or All), then build type (Release or
Debug). The flags are the same answers without the prompts:

```bash
./scripts/build.sh build --release          # portable baseline
./scripts/build.sh build --debug --arch v4  # x86-64-v4, Debug
./scripts/build.sh package                  # universal + v3 + v4 + zen4 tarballs
./scripts/build.sh build --dry-run          # print the cmake commands, run nothing
```

Single-variant builds land in `build/linux` (`_debug`, `_native`,
`_custom-<value>` appended as they apply); `package` writes
`dist/linux/git_wrapper-linux-{universal,v3,v4,zen4}.tar.gz`. A `v4`/`zen4`
binary traps (SIGILL) on a CPU without those extensions — build it only for a
machine you know. `package` and `all` are Linux-only (the variants are
GCC/Clang flags MSVC does not have), and a Debug build is never packaged.

### Requirements

- Windows: Visual Studio Build Tools with **Desktop development with C++**,
  plus `git` on `PATH`. `vcvars64.bat` is located through `vswhere`.
- Linux: `cmake`, `ninja`, and `g++` or `clang++`, plus `git`.
- `uv` (recommended) or Python 3.12+ to run the builder itself. It can also be
  invoked directly — `python scripts/git_wrapper_builder.pyz build` — and with
  `uv` installed, `uv run --directory scripts git-wrapper-build` gives the same
  menu from the repository root.

---

## 2. Install (put it on PATH)

So you can type `git_wrapper` from any repository, add its build folder to PATH
**once**:

- Press Start, type "Edit the system environment variables" → *Environment
  Variables…*
- Under *User variables* select **Path** → *Edit* → *New*, paste:
  ```
  C:\Users\incxiuefb\Documents\Files\clone\git_wrapper\build\windows
  ```
- OK out of all dialogs, then open a **new** terminal.

Verify:

```bat
git_wrapper help
```

(Alternatively, just copy `build\windows\git_wrapper.exe` into a folder that
is already on PATH.)

---

## 3. Commands

Run these from **inside** the repository you want to act on (any subfolder is
fine).

### `commit` — stage everything and commit as nava

```bat
git_wrapper commit "Short summary of the change"
```

- Runs `git add -A` first, then commits.
- Author **and** committer are forced to `nava <nava@noreply.com>`, whatever the
  repo/global git config says.
- The message is cleaned: any `Co-Authored-By:` line, "Generated with … Claude"
  line, or 🤖 line is removed. The rest of your text is kept verbatim.
- If there is nothing to commit, it says so and exits 0 (no error).

### `push` — submodules first, then the parent

```bat
git_wrapper push
git_wrapper push --force
```

- Pushes each **submodule that has unpushed commits** to its `origin` first,
  then the parent repo — the safe order.
- Skips submodules that are pinned/detached (third-party dependencies) or already
  up to date.
- Uses `git push -u origin <current-branch>`, so the first push also sets the
  upstream.
- `--force` (or `-f`) uses `--force-with-lease` — the safe force that refuses to
  overwrite remote commits you haven't fetched. You need this after rewriting
  history (see §5).
- If a submodule push fails, it stops **before** touching the parent.
- It refuses to push if an un-pushed commit still carries a `Co-Authored-By`
  trailer (e.g. one made with plain `git`), telling you to fix it first.
- It uses the right ssh key or https token for each remote — see §10.

### `save` — commit then push in one step

```bat
git_wrapper save "Short summary of the change"
git_wrapper save "Summary" --force
```

Equivalent to `commit` followed by `push`. If the commit step fails, it does not
push.

### `help`

```bat
git_wrapper help
```

---

## 4. Everyday workflow

```bat
:: hack on code ...
git_wrapper save "Add Windows resize handling"
```

That stages everything, commits as nava (clean message), and pushes — submodules
first if any changed. That's the whole loop.

If you'd rather review before pushing:

```bat
git_wrapper commit "Add Windows resize handling"
git status          :: or `git log -1`, `git show`
git_wrapper push
```

---

## 5. Pushing after a history rewrite

If history was rewritten (as it was to remove co-authors), the remote still has
the old commits, so a normal push is rejected. Use force — the wrapper still
pushes the submodule first:

```bat
git_wrapper push --force
```

This runs `--force-with-lease` in the submodule, then the parent. Push order and
safety are handled for you; you don't push the two repos by hand.

> Local backup branches (e.g. `backup-before-trailer-strip`) are **not** pushed —
> `push` only sends the current branch to `origin`. They stay local until you
> delete them.

---

## 6. Exit codes

| Code | Meaning |
|------|---------|
| 0 | Success (including "nothing to commit"). |
| 1 | A git operation failed, or a repo is in detached HEAD and can't be pushed. |
| 2 | Usage error (no command, unknown command, or missing commit message). |
| 3 | Push aborted: an un-pushed commit carries a `Co-Authored-By` trailer. |

---

## 7. Troubleshooting

- **"detached HEAD — check out a branch to push"** — the repo (or a submodule
  you own) isn't on a branch. Run `git switch main` (or the right branch) and
  retry. Third-party submodules are pinned/detached on purpose and are skipped
  automatically.
- **"has un-pushed commits with a Co-Authored-By trailer"** — a commit was made
  outside this wrapper and still credits someone else. Rewrite that commit (only
  nava may appear), then push again.
- **force-with-lease rejected** — the remote moved since you last fetched. Run
  `git fetch`, confirm you still want to overwrite, and retry.
- **submodule push fails with no `origin`** — give the submodule a remote
  (`git -C <path> remote add origin <url>`), or if it's a third-party dep you
  don't own, you shouldn't be pushing it (it should be pinned/detached).
- **"several candidate keys … refusing to guess"** — more than one `~/.ssh` key
  matches. Pick one: `git_wrapper keys add <host/owner> <key>` (or `--key`).
- **auth still wrong after adding a key** — run `git_wrapper keys whoami`; it
  prints each remote and the identity chosen (and why). A `core.sshCommand`
  already set on the repo, or a matching `~/.ssh/config` Host block, wins over
  automatic detection — remove it there if you want the rules file to decide.
- **`keys whoami` says "using git/ssh defaults"** — no rule and no unambiguous
  key were found; either add one (`keys add`) or place a key named after the
  owner/host in `~/.ssh` (see §10).

---

## 8. Changing the identity

The forced name/email are the two constants in `src/config.h`:

```cpp
static const char* kName  = "nava";
static const char* kEmail = "nava@noreply.com";
```

Edit them, then rebuild with `scripts\Build.bat`.

---

## 9. Why a temp file for the message

Windows' program-spawn layer does not quote arguments, so a commit message with
spaces or characters like `&`, `%`, or quotes would be mangled if passed on the
command line. The wrapper writes the (sanitized) message to a temporary file and
uses `git commit -F`, so any message text is safe.

---

## 10. SSH keys and https tokens per remote

When you have several forges (GitHub, Codeberg, …) and several accounts on the
same forge, one key is not enough. `push`/`save` pick the identity from the
remote itself — for the parent repo **and** for every submodule, so a Codeberg
submodule under a GitHub parent uses its own key. Plain `commit` never needs
this.

### The rules file

A small, non-secret file at:

- Linux: `~/.config/git_wrapper/credentials.conf` (or `$XDG_CONFIG_HOME`)
- Windows: `%APPDATA%\git_wrapper\credentials.conf`

Override with `GIT_WRAPPER_CREDENTIALS`. One rule per line:

```
# <match> <kind> <value>
github.com/minervarrbara   ssh    ~/.ssh/id_ed25519_bara
github.com/minervarr       ssh    ~/.ssh/id_ed25519_minervarr
codeberg.org               ssh    ~/.ssh/id_ed25519_codeberg
github.com/minervarr       https  tokens
```

- **match** — `host` or `host/owner`; the more specific `host/owner` wins. `*`
  is a wildcard (`github.com/*`, `*/minervarr`).
- **kind** — `ssh` or `https`.
- **value** — an ssh key path, `tokens` (a token stored in the `0600` file
  beside the rules), or `env:VAR` (read the token from an environment variable).

The file holds **references**, never secrets, so it is safe to read and back up.

### Managing it

```bash
git_wrapper keys                       # list the rules, with source line
git_wrapper keys whoami                # what THIS repo (and each submodule) would use, and why
git_wrapper keys add github.com/minervarrbara ~/.ssh/id_ed25519_bara
git_wrapper keys add github.com/minervarr --token     # prompts, input hidden
git_wrapper keys rm  codeberg.org
git_wrapper keys sync                  # import every IdentityFile from ~/.ssh/config
git_wrapper keys install               # opt-in: register the git credential helper globally
```

### Automatic selection

You often don't need to add anything. On each push:

1. A matching rule wins.
2. Otherwise, an unambiguous key in `~/.ssh` is used **and remembered**, so the
   next run is instant and deterministic:

   ```
   ==> learned github.com/minervarr → ~/.ssh/id_ed25519_minervarr (saved)
   ```

   Keys are matched by filename: `id_ed25519_<owner>`, `id_ed25519_<host>`,
   `id_ed25519_<host>_<owner>` (also `id_rsa_`, `id_ecdsa_`).
3. If two or more keys match, it refuses to guess and tells you to use
   `keys add`.

Nothing is overridden if the repo already sets `core.sshCommand`, or if
`~/.ssh/config` has a `Host` block for the remote — ssh keeps working exactly as
before. HTTPS remotes are untouched unless a rule selects a token.

For a chosen ssh key the wrapper runs git with
`-c core.sshCommand="ssh -i '<key>' -o IdentitiesOnly=yes"`, so only that key is
offered.

### Forcing a key / disabling the learning

```bash
git_wrapper push --key ~/.ssh/id_ed25519_minervarr   # use this key (and record it)
git_wrapper push --no-learn                          # use a detected key, don't save it
```

### HTTPS tokens (two accounts on one host)

`keys add <match> --token` stores the token in `~/.config/git_wrapper/tokens`
(mode `0600`) and selects it by `host/owner`, so `minervarr` and `minervarrbara`
can each have their own token on `github.com`. When a rule selects a token, git
is run with a per-invocation credential helper (`git_wrapper credential`) that
answers git's `get`/`store`/`erase` — nothing global changes unless you run
`keys install`, which also lets plain `git` use it.

Behind a successful password prompt git calls `store`, which saves the token and
records the rule automatically.
