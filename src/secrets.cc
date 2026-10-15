#include "secrets.h"
#include "process.h"
#include "util.h"
#include <cstdlib>
#include <fstream>

std::string SecretStore::defaultPath() {
    const char* override_ = getenv("GIT_WRAPPER_CREDENTIALS");
    if (override_ && *override_) {
        // Keep tokens beside a redirected rules file so tests stay hermetic.
        std::string p = override_;
        size_t slash = p.find_last_of("/\\");
        std::string dir = (slash == std::string::npos) ? "" : p.substr(0, slash + 1);
        return dir + "tokens";
    }
    std::string dir = configDir();
    if (dir.empty()) return "tokens";
    return dir + "/tokens";
}

static void splitTabs(const std::string& line, std::string& a, std::string& b,
                      std::string& c) {
    std::vector<std::string> parts;
    std::string cur;
    for (char ch : line) {
        if (ch == '\t') {
            parts.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(ch);
        }
    }
    parts.push_back(cur);
    a = parts.size() > 0 ? parts[0] : "";
    b = parts.size() > 1 ? parts[1] : "";
    c = parts.size() > 2 ? parts[2] : "";
}

SecretStore SecretStore::load() {
    SecretStore store;
    store.path_ = defaultPath();
    std::ifstream f(store.path_);
    if (!f) return store;
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        Entry e;
        splitTabs(line, e.match, e.user, e.token);
        if (!e.match.empty()) store.entries_.push_back(e);
    }
    return store;
}

bool SecretStore::set(const std::string& match, const std::string& user,
                      const std::string& token) {
    std::string m = toLower(match);
    for (Entry& e : entries_) {
        if (e.match == m) {
            e.user = user;
            e.token = token;
            return true;
        }
    }
    entries_.push_back(Entry{m, user, token});
    return true;
}

bool SecretStore::getFirst(const std::vector<std::string>& keys, std::string& user,
                           std::string& token) const {
    for (const std::string& k : keys) {
        std::string kk = toLower(k);
        for (const Entry& e : entries_) {
            if (e.match == kk) {
                user = e.user;
                token = e.token;
                return true;
            }
        }
    }
    return false;
}

bool SecretStore::erase(const std::string& match) {
    std::string m = toLower(match);
    size_t before = entries_.size();
    std::vector<Entry> kept;
    for (const Entry& e : entries_) {
        if (e.match == m) continue;
        kept.push_back(e);
    }
    entries_ = kept;
    return entries_.size() != before;
}

bool SecretStore::save() const {
    size_t slash = path_.find_last_of("/\\");
    if (slash != std::string::npos) makeDirs(path_.substr(0, slash));
    std::ofstream f(path_, std::ios::trunc | std::ios::binary);
    if (!f) return false;
    f << "# git_wrapper tokens — 0600, keep private. <match>\\t<user>\\t<token>\n";
    for (const Entry& e : entries_) {
        f << e.match << "\t" << e.user << "\t" << e.token << "\n";
    }
    f.close();
    restrictFilePerms(path_);
    return true;
}
