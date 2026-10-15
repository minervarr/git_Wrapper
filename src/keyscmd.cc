#include "keyscmd.h"
#include "credstore.h"
#include "forge.h"
#include "process.h"
#include "repo.h"
#include "resolve.h"
#include "secrets.h"
#include "ssh_identity.h"
#include "util.h"
#include <cstdio>
#include <cstdlib>

static void printKeysUsage() {
    printf(
        "git_wrapper keys — which identity is used for which remote\n\n"
        "  git_wrapper keys                      list the rules\n"
        "  git_wrapper keys add <match> <key>    ssh key for host or host/owner\n"
        "  git_wrapper keys add <match> --token  prompt for an https token\n"
        "  git_wrapper keys rm  <match>          remove a rule (and its token)\n"
        "  git_wrapper keys sync                 import ~/.ssh/config\n"
        "  git_wrapper keys whoami [dir]         explain what this repo would use\n"
        "  git_wrapper keys install              register the git credential helper\n\n"
        "<match> is a host (github.com) or host/owner (github.com/minervarr),\n"
        "and may contain '*' (github.com/*, */minervarr).\n");
}

static int keysList() {
    CredStore store = CredStore::load();
    printf("%s\n", store.path().c_str());
    if (store.rules().empty()) {
        printf("(no rules — add one with `git_wrapper keys add`)\n");
        return 0;
    }
    for (const Rule& r : store.rules()) {
        printf("  %-30s %-6s %-40s :%d\n", r.match.c_str(), r.kind.c_str(),
               r.value.c_str(), r.line);
    }
    return 0;
}

static int keysAdd(const std::vector<std::string>& rest) {
    std::string match, key, user;
    bool wantToken = false;
    for (size_t i = 0; i < rest.size(); ++i) {
        const std::string& a = rest[i];
        if (a == "--key") {
            if (i + 1 >= rest.size()) { fprintf(stderr, "error: --key needs a path\n"); return 2; }
            key = rest[++i];
        } else if (a == "--token") {
            wantToken = true;
        } else if (a == "--user") {
            if (i + 1 >= rest.size()) { fprintf(stderr, "error: --user needs a name\n"); return 2; }
            user = rest[++i];
        } else if (match.empty()) {
            match = a;
        } else if (key.empty()) {
            key = a;  // positional key: `keys add github.com/minervarr ~/.ssh/k`
        } else {
            fprintf(stderr, "error: unexpected argument: %s\n", a.c_str());
            return 2;
        }
    }
    if (match.empty()) { fprintf(stderr, "error: `keys add` needs a match\n"); return 2; }

    CredStore store = CredStore::load();
    if (wantToken) {
        std::string token = readSecret("token (input hidden): ");
        if (token.empty()) { fprintf(stderr, "error: empty token\n"); return 1; }
        std::string concrete = toLower(match);
        if (user.empty()) {
            size_t slash = match.find('/');
            user = slash == std::string::npos ? "git" : match.substr(slash + 1);
        }
        SecretStore sec = SecretStore::load();
        sec.set(concrete, user, token);
        if (!sec.save()) { fprintf(stderr, "error: cannot write %s\n", sec.path().c_str()); return 1; }
        bool changed = store.add(concrete, "https", "tokens");
        (void)changed;
        if (!store.save()) { fprintf(stderr, "error: cannot write %s\n", store.path().c_str()); return 1; }
        printf("==> %s https → token for user %s (%s)\n", concrete.c_str(), user.c_str(),
               sec.path().c_str());
        return 0;
    }
    if (key.empty()) {
        fprintf(stderr, "error: give a key path or --token\n");
        return 2;
    }
    if (!pathExists(expandTilde(key)))
        fprintf(stderr, "warning: %s does not exist (adding it anyway)\n", key.c_str());
    if (!store.add(match, "ssh", key)) {
        printf("    unchanged: %s ssh already = %s\n", toLower(match).c_str(), key.c_str());
        return 0;
    }
    if (!store.save()) { fprintf(stderr, "error: cannot write %s\n", store.path().c_str()); return 1; }
    printf("==> %s ssh → %s (%s)\n", toLower(match).c_str(), key.c_str(), store.path().c_str());
    return 0;
}

static int keysRm(const std::vector<std::string>& rest) {
    if (rest.empty()) { fprintf(stderr, "error: `keys rm` needs a match\n"); return 2; }
    std::string match = toLower(rest[0]);
    CredStore store = CredStore::load();
    bool changed = store.remove(match, "");
    if (changed && !store.save()) {
        fprintf(stderr, "error: cannot write %s\n", store.path().c_str());
        return 1;
    }
    SecretStore sec = SecretStore::load();
    if (sec.erase(match) && !sec.save()) {
        fprintf(stderr, "error: cannot write %s\n", sec.path().c_str());
        return 1;
    }
    if (changed) printf("==> removed %s\n", match.c_str());
    else printf("    no rule for %s\n", match.c_str());
    return 0;
}

static int keysSync() {
    CredStore store = CredStore::load();
    int added = 0;
    for (const SshConfigEntry& e : sshConfigEntries()) {
        if (store.add(e.pattern, "ssh", e.identity)) {
            ++added;
            printf("    %s ssh → %s\n", e.pattern.c_str(), e.identity.c_str());
        }
    }
    if (added && !store.save()) {
        fprintf(stderr, "error: cannot write %s\n", store.path().c_str());
        return 1;
    }
    printf("==> imported %d rule(s) from ~/.ssh/config into %s\n", added,
           store.path().c_str());
    return 0;
}

static void whoamiOne(const std::string& dir, CredStore& store) {
    std::string label = dir.empty() ? "this repo" : dir;
    ResolveOptions opt;
    opt.learn = false;
    Resolved r = resolveForRepo(dir, store, opt);
    printf("%s: %s\n", label.c_str(),
           r.remoteUrl.empty() ? "(no origin)" : r.remoteUrl.c_str());
    if (r.identity.empty()) {
        printf("    transport %s — %s\n", r.transport.empty() ? "?" : r.transport.c_str(),
               r.note.empty() ? "using git/ssh defaults" : r.note.c_str());
        return;
    }
    printf("    %-9s %s\n", r.transport.c_str(), r.identity.c_str());
    if (!r.source.empty()) printf("    source    %s\n", r.source.c_str());
}

static int keysWhoami(const std::vector<std::string>& rest) {
    std::string dir;
    for (const std::string& a : rest) {
        if (a == "-h" || a == "--help") { printKeysUsage(); return 0; }
        if (!a.empty() && a[0] == '-') {
            fprintf(stderr, "error: unknown option for `keys whoami`: %s\n", a.c_str());
            return 2;
        }
        if (!dir.empty()) { fprintf(stderr, "error: too many arguments\n"); return 2; }
        dir = a;
    }
    CredStore store = CredStore::load();
    whoamiOne(dir, store);
    if (dir.empty()) {
        for (const std::string& s : submodulePaths()) {
            printf("\n");
            whoamiOne(s, store);
        }
    }
    return 0;
}

static int keysInstall() {
    std::string self = selfExePath();
    if (self.empty()) {
        fprintf(stderr, "error: cannot determine this executable's path\n");
        return 1;
    }
    std::string value = "!" + shellQuote(self) + " credential";
    // Preserve any helpers already configured, after ours.
    std::vector<std::string> existing;
    {
        std::string all = gitCapture("config --global --get-all credential.helper");
        size_t start = 0;
        while (start <= all.size()) {
            size_t nl = all.find('\n', start);
            std::string l = all.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
            if (!l.empty()) existing.push_back(trim(l));
            if (nl == std::string::npos) break;
            start = nl + 1;
        }
    }
    git({"config", "--global", "--unset-all", "credential.helper"});
    git({"config", "--global", "--add", "credential.helper", value});
    for (const std::string& h : existing) {
        if (h == value) continue;
        git({"config", "--global", "--add", "credential.helper", h});
    }
    git({"config", "--global", "credential.useHttpPath", "true"});
    printf("==> registered credential helper: %s\n", value.c_str());
    for (const std::string& h : existing)
        if (h != value) printf("    kept fallback helper: %s\n", h.c_str());
    return 0;
}

int keysCommand(const std::vector<std::string>& args) {
    std::string verb = args.empty() ? "list" : args[0];
    std::vector<std::string> rest(args.begin() + (args.empty() ? 0 : 1), args.end());
    if (verb == "list") return keysList();
    if (verb == "add") return keysAdd(rest);
    if (verb == "rm" || verb == "remove") return keysRm(rest);
    if (verb == "sync") return keysSync();
    if (verb == "whoami") return keysWhoami(rest);
    if (verb == "install") return keysInstall();
    if (verb == "help" || verb == "-h" || verb == "--help") { printKeysUsage(); return 0; }
    fprintf(stderr, "error: unknown `keys` verb: %s\n\n", verb.c_str());
    printKeysUsage();
    return 2;
}
