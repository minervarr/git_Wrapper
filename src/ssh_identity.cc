#include "ssh_identity.h"
#include "process.h"
#include "util.h"
#include <cctype>
#include <fstream>
#include <set>

std::string expandTilde(const std::string& path) {
    if (path.empty() || path[0] != '~') return path;
    if (path.size() > 1 && path[1] != '/' && path[1] != '\\') return path;  // ~user
    std::string home = homeDir();
    if (home.empty()) return path;
    if (path.size() == 1) return home;
    return home + "/" + path.substr(2);
}

// First whitespace-separated field, lowercased.
static std::string firstField(const std::string& line) {
    std::string t = trim(line);
    size_t sp = t.find_first_of(" \t");
    return toLower(sp == std::string::npos ? t : t.substr(0, sp));
}

static std::string stripComment(const std::string& line) {
    size_t hash = line.find('#');
    return trim(hash == std::string::npos ? line : line.substr(0, hash));
}

// Parse "Host a b c" into its patterns.
static std::vector<std::string> hostPatterns(const std::string& line) {
    std::vector<std::string> pats;
    std::string t = stripComment(line);
    size_t sp = t.find_first_of(" \t");
    if (sp == std::string::npos) return pats;
    std::string rest = trim(t.substr(sp + 1));
    size_t i = 0;
    while (i < rest.size()) {
        while (i < rest.size() && (rest[i] == ' ' || rest[i] == '\t')) ++i;
        std::string tok;
        while (i < rest.size() && rest[i] != ' ' && rest[i] != '\t') tok.push_back(rest[i++]);
        if (!tok.empty()) pats.push_back(toLower(tok));
    }
    return pats;
}

static bool hostMatches(const std::vector<std::string>& pats, const std::string& host) {
    std::string h = toLower(host);
    for (const std::string& p : pats) {
        if (!p.empty() && p[0] == '!') continue;  // negation: ignore
        if (globMatch(p, h)) return true;
    }
    return false;
}

std::string sshConfigIdentityFor(const std::string& host) {
    std::string home = homeDir();
    if (home.empty()) return "";
    std::ifstream f(home + "/.ssh/config");
    if (!f) return "";
    std::vector<std::string> pats;
    bool inBlock = false;
    std::string line;
    while (std::getline(f, line)) {
        std::string key = firstField(line);
        if (key == "host") {
            pats = hostPatterns(line);
            inBlock = true;
            continue;
        }
        if (key == "match") {
            pats.clear();
            inBlock = false;
            continue;
        }
        if (inBlock && key == "identityfile") {
            std::string t = stripComment(line);
            size_t sp = t.find_first_of(" \t");
            if (sp == std::string::npos) continue;
            std::string val = trim(t.substr(sp + 1));
            if (!val.empty()) {
                // First matching block's first IdentityFile wins.
                if (hostMatches(pats, host)) return expandTilde(val);
            }
        }
    }
    return "";
}

std::vector<SshConfigEntry> sshConfigEntries() {
    std::vector<SshConfigEntry> out;
    std::string home = homeDir();
    if (home.empty()) return out;
    std::ifstream f(home + "/.ssh/config");
    if (!f) return out;

    std::vector<std::string> pats;
    std::string identity;
    auto flush = [&]() {
        if (!identity.empty()) {
            for (const std::string& p : pats) {
                if (p.empty() || p[0] == '!' || p.find('*') != std::string::npos ||
                    p.find('?') != std::string::npos)
                    continue;
                out.push_back(SshConfigEntry{p, identity});
            }
        }
        pats.clear();
        identity.clear();
    };

    std::string line;
    while (std::getline(f, line)) {
        std::string key = firstField(line);
        if (key == "host") {
            flush();
            pats = hostPatterns(line);
            continue;
        }
        if (key == "match") {
            flush();
            continue;
        }
        if (key == "identityfile" && identity.empty()) {
            std::string t = stripComment(line);
            size_t sp = t.find_first_of(" \t");
            if (sp != std::string::npos) identity = expandTilde(trim(t.substr(sp + 1)));
        }
    }
    flush();
    return out;
}

std::vector<std::string> conventionKeys(const std::string& host, const std::string& owner) {
    std::vector<std::string> out;
    std::string home = homeDir();
    if (home.empty()) return out;
    std::string sshDir = home + "/.ssh";
    std::string h = toLower(host);
    std::string o = toLower(owner);
    std::string hostLabel = h;
    size_t dot = hostLabel.find('.');
    if (dot != std::string::npos) hostLabel = hostLabel.substr(0, dot);

    const char* algos[] = {"id_ed25519", "id_ecdsa", "id_rsa"};
    std::set<std::string> seen;
    auto probe = [&](const std::string& name) {
        if (name.size() <= 1) return;
        std::string path = sshDir + "/" + name;
        if (!pathExists(path)) return;
        if (seen.insert(path).second) out.push_back(path);
    };
    for (const char* algo : algos)
        if (!o.empty()) probe(std::string(algo) + "_" + o);
    for (const char* algo : algos)
        if (!hostLabel.empty()) probe(std::string(algo) + "_" + hostLabel);
    for (const char* algo : algos)
        if (!h.empty()) probe(std::string(algo) + "_" + h);
    for (const char* algo : algos)
        if (!o.empty() && !h.empty()) probe(std::string(algo) + "_" + h + "_" + o);
    return out;
}

std::string buildSshCommand(const std::string& key) {
    return "ssh -i " + shellQuote(toSshPath(key)) + " -o IdentitiesOnly=yes";
}
