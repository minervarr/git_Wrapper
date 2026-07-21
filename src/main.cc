// git_wrapper — an error-proof front-end for `git commit` and `git push`.
//
// Why it exists: commits must never carry a Co-Authored-By trailer or any
// identity other than nava <nava@noreply.com>, and a repository with submodules
// must have its submodules pushed BEFORE the parent (otherwise a fresh clone
// points the parent's gitlink at a commit the remote doesn't have and submodule
// init fails). This tool bakes both rules in so they cannot be forgotten.
//
//   git_wrapper commit "<message>"            stage everything + commit as nava
//   git_wrapper push   [--force]              push submodules first, then this repo
//   git_wrapper save   "<message>" [--force]  commit, then push
//
// --force maps to --force-with-lease (refuses to overwrite remote work you have
// not seen locally). Run it from anywhere inside the repository.

#include "commands.h"
#include "config.h"
#include "version.h"
#include <cstdio>
#include <string>

static void usage() {
    printf(
        "git_wrapper — nava-only commits, submodule-safe pushes\n\n"
        "  git_wrapper commit \"<message>\"           stage all + commit as nava\n"
        "  git_wrapper push  [--force]              push submodules first, then this repo\n"
        "  git_wrapper save  \"<message>\" [--force]  commit, then push\n"
        "  git_wrapper version                      print the version\n\n"
        "Identity is always forced to %s <%s>; Co-Authored-By / Claude trailers\n"
        "are stripped from the message. --force uses --force-with-lease.\n",
        kName, kEmail);
}

int main(int argc, char** argv) {
    if (argc < 2) { usage(); return 2; }
    std::string cmd = argv[1];

    bool force = false;
    std::string msg;
    for (int i = 2; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--force" || a == "-f") { force = true; continue; }
        if (!msg.empty()) msg += " ";
        msg += a;
    }

    if (cmd == "commit") {
        if (msg.empty()) { fprintf(stderr, "error: `commit` needs a message\n"); return 2; }
        return doCommit(msg);
    }
    if (cmd == "push") {
        return doPush(force);
    }
    if (cmd == "save") {
        if (msg.empty()) { fprintf(stderr, "error: `save` needs a message\n"); return 2; }
        int rc = doCommit(msg);
        if (rc != 0) return rc;
        return doPush(force);
    }
    if (cmd == "help" || cmd == "-h" || cmd == "--help") { usage(); return 0; }
    if (cmd == "version" || cmd == "-v" || cmd == "--version") {
        printf("git_wrapper %s\n", GIT_WRAPPER_VERSION);
        return 0;
    }

    fprintf(stderr, "unknown command: %s\n\n", cmd.c_str());
    usage();
    return 2;
}
