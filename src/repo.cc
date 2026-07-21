#include "repo.h"
#include "process.h"
#include "util.h"
#include <sstream>
#include <cstdio>
#include <cstdlib>

std::string currentBranch(const std::string& dir) {
    return gitCapture(ctxPrefix(dir) + "rev-parse --abbrev-ref HEAD");
}

std::vector<std::string> submodulePaths() {
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

int commitsAhead(const std::string& dir) {
    std::string pre = ctxPrefix(dir);
    std::string up = gitCapture(pre + "rev-parse --abbrev-ref --symbolic-full-name @{upstream}");
    if (up.empty() || up.find("fatal") != std::string::npos) return -1;
    std::string c = gitCapture(pre + "rev-list --count @{upstream}..HEAD");
    if (c.empty()) return -1;
    return atoi(c.c_str());
}

bool verifyNoTrailers(const std::string& dir) {
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
