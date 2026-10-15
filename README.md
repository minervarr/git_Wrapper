# git_wrapper

An error-proof front-end for `git commit` and `git push`, built to stop three
mistakes that already cost a history rewrite:

1. **Foreign contributors on commits.** Every commit is forced to
   `nava <nava@noreply.com>` for *both* author and committer, and any
   `Co-Authored-By:` / "Generated with … Claude" / 🤖 line is stripped from the
   message. No trailer, no stray identity, ever.
2. **Wrong push order with submodules.** `push` pushes each submodule that has
   unpushed commits **first**, then the parent — so a fresh clone never points
   at a submodule commit the remote doesn't have.
3. **Wrong key/token for a remote.** `push`/`save` pick the ssh key or https
   token per remote — parent *and* every submodule — from a small rules file,
   recording an unambiguous first-seen key automatically. No more "only one
   key".

## Build

```bat
scripts\Build.bat    :: Windows: vcvars64 -> Ninja -> cl, produces build\windows\git_wrapper.exe
```

```bash
./scripts/build.sh   # Linux: cmake -> ninja, produces build/linux/git_wrapper
```

Both run `scripts/git_wrapper_builder.pyz`, a standard-library-only Python
zipapp that drives CMake on either platform. Run it with no arguments for a menu
(microarchitecture, then build type), or use `package` for universal/v3/v4/zen4
tarballs in `dist/linux/`. See [scripts/README.md](scripts/README.md).

Then put the resulting binary on PATH and run it from anywhere inside a
repository.

## Usage

```
git_wrapper commit "<message>"            stage everything + commit as nava
git_wrapper push  [--force]               push submodules first, then this repo
git_wrapper save  "<message>" [--force]   commit, then push
git_wrapper keys  [list|add|rm|sync|whoami|install]
```

See [docs/USAGE.md](docs/USAGE.md) for the full command reference (flags,
exit codes, credential setup, troubleshooting).

## Notes

- The identity is the two constants `kName` / `kEmail` in `src/config.h`.
- Commit messages are passed to git via a temp file, so spaces and shell
  metacharacters (`&`, `%`, quotes, newlines) in the message are always safe.
- Which key/token a remote uses lives in `credentials.conf` next to the
  binary's config (`~/.config/git_wrapper/`, `%APPDATA%\git_wrapper\`); run
  `git_wrapper keys whoami` to see what a repo would use and why.
- See [docs/ROADMAP.md](docs/ROADMAP.md) for planned work.
