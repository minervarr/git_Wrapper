# git_wrapper

An error-proof front-end for `git commit` and `git push`, built to stop two
mistakes that already cost a history rewrite:

1. **Foreign contributors on commits.** Every commit is forced to
   `nava <nava@noreply.com>` for *both* author and committer, and any
   `Co-Authored-By:` / "Generated with … Claude" / 🤖 line is stripped from the
   message. No trailer, no stray identity, ever.
2. **Wrong push order with submodules.** `push` pushes each submodule that has
   unpushed commits **first**, then the parent — so a fresh clone never points
   at a submodule commit the remote doesn't have.

## Build

```bat
scripts\Build.bat    :: Windows: vswhere -> vcvars64 -> Ninja -> cl, produces build\windows\git_wrapper.exe
```

```bash
./scripts/build.sh   # Linux: cmake -> ninja, produces build/linux/git_wrapper
```

Then put the resulting binary on PATH and run it from anywhere inside a
repository.

## Usage

```
git_wrapper commit "<message>"            stage everything + commit as nava
git_wrapper push  [--force]               push submodules first, then this repo
git_wrapper save  "<message>" [--force]   commit, then push
```

See [docs/USAGE.md](docs/USAGE.md) for the full command reference (flags,
exit codes, troubleshooting).

## Notes

- The identity is the two constants `kName` / `kEmail` in `src/config.h`.
- Commit messages are passed to git via a temp file, so spaces and shell
  metacharacters (`&`, `%`, quotes, newlines) in the message are always safe.
- See [docs/ROADMAP.md](docs/ROADMAP.md) for planned work.
