#include "commands.h"
#include "config.h"
#include "process.h"
#include "repo.h"
#include "sanitize.h"
#include <cstdio>
#include <fstream>

int doCommit(const std::string& rawMsg) {
    std::string msg = sanitize(rawMsg);
    if (msg.empty()) {
        fprintf(stderr, "error: empty commit message\n");
        return 2;
    }
    printf("==> staging all changes\n");
    if (git({"add", "-A"}) != 0) {
        fprintf(stderr, "error: `git add -A` failed\n");
        return 1;
    }
    if (git({"diff", "--cached", "--quiet"}) == 0) {
        printf("    nothing to commit — working tree clean\n");
        return 0;
    }
    printf("==> committing as %s <%s>\n", kName, kEmail);

    // Pass the message through a temp file (`-F`), never as an argv string: on
    // Windows the spawn layer does not quote arguments, so a message with
    // spaces would be shattered into pathspecs — and a file is also immune to
    // shell metacharacters like & or %. Using the same approach on every
    // platform keeps this function identical everywhere. `-c user.name/email`
    // sets BOTH author and committer, so no --author (with its
    // space-containing value) is needed.
    std::string msgPath = tempMessageFilePath();
    {
        std::ofstream f(msgPath, std::ios::binary);
        if (!f) { fprintf(stderr, "error: cannot write temp message file\n"); return 1; }
        f << msg << "\n";
    }
    int rc = git({
        "-c", std::string("user.name=") + kName,
        "-c", std::string("user.email=") + kEmail,
        "commit",
        "-F", msgPath,
    });
    std::remove(msgPath.c_str());
    return rc;
}

// Refresh the remote-tracking ref before a push. force-with-lease and the
// "commits ahead" skip check both compare against origin/<branch>; if that ref
// is stale (e.g. after a local history rewrite) the push is wrongly rejected
// as "stale info" or the submodule is wrongly skipped. A fetch fixes both.
static void fetchRepo(const std::string& dir) {
    std::string branch = currentBranch(dir);
    if (branch.empty() || branch == "HEAD") return;
    std::vector<std::string> args;
    if (!dir.empty()) { args.push_back("-C"); args.push_back(dir); }
    args.push_back("fetch");
    args.push_back("--quiet");
    args.push_back("origin");
    args.push_back(branch);
    git(args);  // best-effort: if it fails (offline / no origin), the push reports it
}

static int pushRepo(const std::string& dir, bool force) {
    std::string label = dir.empty() ? "this repo" : dir;
    std::string branch = currentBranch(dir);
    if (branch.empty() || branch == "HEAD") {
        fprintf(stderr, "  ! %s is in detached HEAD — check out a branch to push.\n", label.c_str());
        return 1;
    }
    std::vector<std::string> args;
    if (!dir.empty()) { args.push_back("-C"); args.push_back(dir); }
    args.push_back("push");
    args.push_back("-u");
    args.push_back("origin");
    args.push_back(branch);
    if (force) args.push_back("--force-with-lease");
    printf("==> pushing %s (branch %s%s)\n", label.c_str(), branch.c_str(),
           force ? ", force-with-lease" : "");
    return git(args);
}

int doPush(bool force) {
    fetchRepo("");
    if (!verifyNoTrailers("")) return 3;

    // Submodules first, but only the ones that actually have something to push
    // (this skips pinned/detached third-party submodules automatically).
    for (auto& s : submodulePaths()) {
        std::string branch = currentBranch(s);
        if (branch.empty() || branch == "HEAD") {
            printf("    %s: detached (pinned) — skipping\n", s.c_str());
            continue;
        }
        fetchRepo(s);
        if (commitsAhead(s) == 0) {
            printf("    %s: nothing to push\n", s.c_str());
            continue;
        }
        if (!verifyNoTrailers(s)) return 3;
        int rc = pushRepo(s, force);
        if (rc != 0) {
            fprintf(stderr, "error: submodule %s push failed (rc=%d) — aborting before parent.\n",
                    s.c_str(), rc);
            return rc;
        }
    }
    return pushRepo("", force);
}
