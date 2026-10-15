#include "credstore.h"
#include "process.h"
#include "util.h"
#include <cstdlib>
#include <fstream>
#include <sstream>

// Split a rule line into tokens, honouring double quotes so a Windows key path
// with spaces (`C:\Users\Foo Bar\.ssh\id`) stays one value.
static std::vector<std::string> tokenize(const std::string& line) {
    std::vector<std::string> out;
    size_t i = 0;
    while (i < line.size()) {
        while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
        if (i >= line.size()) break;
        std::string tok;
        if (line[i] == '"') {
            ++i;
            while (i < line.size() && line[i] != '"') tok.push_back(line[i++]);
            if (i < line.size()) ++i;  // closing quote
        } else {
            while (i < line.size() && line[i] != ' ' && line[i] != '\t') tok.push_back(line[i++]);
        }
        out.push_back(tok);
    }
    return out;
}

static bool parseRule(const std::string& line, int lineNo, Rule& out) {
    std::string s = trim(line);
    if (s.empty() || s[0] == '#') return false;
    std::vector<std::string> tok = tokenize(s);
    if (tok.size() < 3) return false;
    std::string kind = toLower(tok[1]);
    if (kind != "ssh" && kind != "https") return false;

    // value = everything after the first two tokens, minus an optional '='.
    std::string value = tok[2];
    for (size_t i = 3; i < tok.size(); ++i) value += " " + tok[i];
    if (!value.empty() && value[0] == '=') value = trim(value.substr(1));

    out.match = toLower(tok[0]);
    out.kind = kind;
    out.value = value;
    out.line = lineNo;
    return true;
}

std::string CredStore::defaultPath() {
    const char* override_ = getenv("GIT_WRAPPER_CREDENTIALS");
    if (override_ && *override_) return override_;
    std::string dir = configDir();
    if (dir.empty()) return "credentials.conf";
    return dir + "/credentials.conf";
}

CredStore CredStore::load() {
    CredStore store;
    store.path_ = defaultPath();
    std::ifstream f(store.path_);
    if (!f) return store;
    std::string line;
    int lineNo = 0;
    while (std::getline(f, line)) {
        ++lineNo;
        Rule r;
        if (parseRule(line, lineNo, r)) store.rules_.push_back(r);
    }
    return store;
}

const Rule* CredStore::find(const std::string& host, const std::string& owner,
                            const std::string& kind) const {
    if (host.empty()) return nullptr;
    std::vector<std::string> candidates;
    if (!owner.empty()) candidates.push_back(toLower(host) + "/" + toLower(owner));
    candidates.push_back(toLower(host));
    for (const std::string& cand : candidates) {
        for (const Rule& r : rules_) {
            if (r.kind != kind) continue;
            if (globMatch(r.match, cand)) return &r;
        }
    }
    return nullptr;
}

bool CredStore::add(const std::string& match, const std::string& kind,
                    const std::string& value) {
    std::string m = toLower(match);
    for (Rule& r : rules_) {
        if (r.match == m && r.kind == kind) {
            if (r.value == value) return false;
            r.value = value;
            return true;
        }
    }
    rules_.push_back(Rule{m, kind, value, 0});
    return true;
}

bool CredStore::remove(const std::string& match, const std::string& kind) {
    std::string m = toLower(match);
    size_t before = rules_.size();
    std::vector<Rule> kept;
    for (const Rule& r : rules_) {
        if (r.match == m && (kind.empty() || r.kind == kind)) continue;
        kept.push_back(r);
    }
    rules_ = kept;
    return rules_.size() != before;
}

bool CredStore::save() const {
    size_t slash = path_.find_last_of("/\\");
    if (slash != std::string::npos) makeDirs(path_.substr(0, slash));
    std::ofstream f(path_, std::ios::trunc | std::ios::binary);
    if (!f) return false;
    f << "# git_wrapper credential rules — which identity to use for which remote.\n"
         "# <match> <kind> <value>   match: host or host/owner (may contain '*')\n"
         "# kind: ssh | https  (https value: \"tokens\" or \"env:VAR\")\n";
    for (const Rule& r : rules_) {
        f << r.match << " " << r.kind << " " << r.value << "\n";
    }
    f.close();
    restrictFilePerms(path_);
    return true;
}
