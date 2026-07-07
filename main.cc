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

#include <process.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <cctype>

static const char* kName  = "nava";
static const char* kEmail = "nava@noreply.com";

// ---- small helpers ---------------------------------------------------------

static std::string toLower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

// Run git with explicit argv — no shell, so spaces/quotes in a commit message
// can never be misparsed. stdio is inherited, so git's own output is shown.
static int git(std::vector<std::string> args) {
    std::vector<char*> argv;
    argv.push_back(const_cast<char*>("git"));
    for (auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);
    return (int)_spawnvp(_P_WAIT, "git", argv.data());
}

// Run a git query and capture its trimmed stdout (stderr silenced).
static std::string gitCapture(const std::string& cmdline) {
    std::string full = "git " + cmdline + " 2>NUL";
    std::string out;
    FILE* p = _popen(full.c_str(), "r");
    if (!p) return out;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, p)) > 0) out.append(buf, n);
    _pclose(p);
    return trim(out);
}

// Drop any Co-Authored-By trailer, Claude-Session trailer, "Generated with …
// Claude" line, or robot emoji line the message might carry, so only nava is
// ever credited.
static std::string sanitize(const std::string& msg) {
    std::istringstream in(msg);
    std::string line, out;
    bool first = true;
    while (std::getline(in, line)) {
        std::string low = toLower(trim(line));
        if (low.rfind("co-authored-by:", 0) == 0) continue;
        if (low.rfind("claude-session:", 0) == 0) continue;
        if (low.find("generated with") != std::string::npos &&
            low.find("claude") != std::string::npos) continue;
        if (line.find("\xF0\x9F\xA4\x96") != std::string::npos) continue;  // robot emoji
        if (!first) out += "\n";
        out += line;
        first = false;
    }
    return trim(out);
}

static std::string ctxPrefix(const std::string& dir) {
    return dir.empty() ? "" : "-C \"" + dir + "\" ";
}

static std::string currentBranch(const std::string& dir) {
    return gitCapture(ctxPrefix(dir) + "rev-parse --abbrev-ref HEAD");
}

// Direct submodules (non-recursive: nested third-party submodules are not ours
// to push). Uninitialised entries (leading '-') are skipped.
static std::vector<std::string> submodulePaths() {
    std::vector<std::string> paths;
    std::istringstream in(gitCapture("submodule status"));
    std::string line;
    while (std::getline(in, line)) {
        std::string t = trim(line);
        if (t.empty() || t[0] == '-') continue;
        std::istringstream ls(t);
        std::string sha, path;
        ls >> sha >> path;
        if (!path.empty()) paths.push_back(path);
    }
    return paths;
}

// -1 = no upstream (unknown); otherwise the number of commits HEAD is ahead.
static int commitsAhead(const std::string& dir) {
    std::string pre = ctxPrefix(dir);
    std::string up = gitCapture(pre + "rev-parse --abbrev-ref --symbolic-full-name @{upstream}");
    if (up.empty() || up.find("fatal") != std::string::npos) return -1;
    std::string c = gitCapture(pre + "rev-list --count @{upstream}..HEAD");
    if (c.empty()) return -1;
    return atoi(c.c_str());
}

// Refuse to push if any not-yet-pushed commit carries a Co-Authored-By or
// Claude-Session trailer (e.g. a commit made with plain `git` outside this
// wrapper).
static bool verifyNoTrailers(const std::string& dir) {
    std::string pre = ctxPrefix(dir);
    std::string up = gitCapture(pre + "rev-parse --abbrev-ref --symbolic-full-name @{upstream}");
    if (up.empty() || up.find("fatal") != std::string::npos) return true;  // no upstream: nothing to compare
    std::string body = toLower(gitCapture(pre + "log @{upstream}..HEAD --format=%B"));
    if (body.find("co-authored-by:") != std::string::npos ||
        body.find("claude-session:") != std::string::npos) {
        fprintf(stderr, "  ! %s has un-pushed commits with a Co-Authored-By or Claude-Session trailer.\n",
                dir.empty() ? "this repo" : dir.c_str());
        fprintf(stderr, "    Rewrite them before pushing (only nava may appear).\n");
        return false;
    }
    return true;
}

// ---- commands --------------------------------------------------------------

static int doCommit(const std::string& rawMsg) {
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

    // Pass the message through a temp file (`-F`), never as an argv string: the
    // Windows spawn layer does not quote arguments, so a message with spaces
    // would be shattered into pathspecs — and a file is also immune to shell
    // metacharacters like & or %. `-c user.name/email` sets BOTH author and
    // committer, so no --author (with its space-containing value) is needed.
    const char* tmpdir = getenv("TEMP");
    if (!tmpdir) tmpdir = getenv("TMP");
    if (!tmpdir) tmpdir = ".";
    std::string msgPath = std::string(tmpdir) + "\\gitwrapper_msg_" +
                          std::to_string((long)_getpid()) + ".txt";
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

static int doPush(bool force) {
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

static void usage() {
    printf(
        "git_wrapper — nava-only commits, submodule-safe pushes\n\n"
        "  git_wrapper commit \"<message>\"           stage all + commit as nava\n"
        "  git_wrapper push  [--force]              push submodules first, then this repo\n"
        "  git_wrapper save  \"<message>\" [--force]  commit, then push\n\n"
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

    fprintf(stderr, "unknown command: %s\n\n", cmd.c_str());
    usage();
    return 2;
}
