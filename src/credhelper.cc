#include "credhelper.h"
#include "credstore.h"
#include "process.h"
#include "secrets.h"
#include "util.h"
#include <cstdlib>
#include <iostream>
#include <map>

static std::string ownerOf(const std::string& path) {
    size_t slash = path.find('/');
    std::string first = (slash == std::string::npos) ? path : path.substr(0, slash);
    return toLower(first);
}

static std::vector<std::string> keyCandidates(const std::string& host,
                                              const std::string& path) {
    std::vector<std::string> keys;
    std::string hostLower = toLower(host);
    std::string owner = ownerOf(path);
    if (!owner.empty()) keys.push_back(hostLower + "/" + owner);
    keys.push_back(hostLower);
    return keys;
}

int credentialHelper(const std::string& op) {
    std::map<std::string, std::string> in;
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) break;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        in[line.substr(0, eq)] = line.substr(eq + 1);
    }

    std::string protocol = in.count("protocol") ? in["protocol"] : "";
    std::string host = in.count("host") ? in["host"] : "";
    std::string path = in.count("path") ? in["path"] : "";
    if (protocol != "http" && protocol != "https") return 0;
    if (host.empty()) return 0;

    std::vector<std::string> keys = keyCandidates(host, path);
    std::string concrete = keys.front();

    if (op == "get") {
        std::string user, token;
        CredStore store = CredStore::load();
        const Rule* rule = store.find(host, ownerOf(path), "https");
        if (rule && rule->value.rfind("env:", 0) == 0) {
            const char* v = getenv(rule->value.substr(4).c_str());
            if (v && *v) {
                token = v;
                user = in.count("username") && !in["username"].empty()
                           ? in["username"]
                           : (ownerOf(path).empty() ? "git" : ownerOf(path));
            }
        } else {
            SecretStore sec = SecretStore::load();
            sec.getFirst(keys, user, token);
        }
        if (token.empty()) return 0;  // let git fall through / prompt
        std::cout << "username=" << user << "\n";
        std::cout << "password=" << token << "\n\n";
        return 0;
    }

    if (op == "store") {
        std::string token = in.count("password") ? in["password"] : "";
        if (token.empty()) return 0;
        std::string user = in.count("username") ? in["username"] : "";
        if (user.empty()) user = ownerOf(path).empty() ? "git" : ownerOf(path);
        SecretStore sec = SecretStore::load();
        sec.set(concrete, user, token);
        if (!sec.save()) {
            std::cerr << "git_wrapper: could not write " << sec.path() << "\n";
            return 1;
        }
        std::string match = ownerOf(path).empty() ? toLower(host)
                                                  : toLower(host) + "/" + ownerOf(path);
        CredStore store = CredStore::load();
        if (store.add(match, "https", "tokens")) store.save();
        std::cerr << "==> saved https credentials for " << match << " ("
                  << store.path() << ")\n";
        return 0;
    }

    if (op == "erase") {
        SecretStore sec = SecretStore::load();
        bool changed = sec.erase(concrete);
        if (changed) sec.save();
        return 0;
    }

    std::cerr << "git_wrapper: unknown credential operation: " << op << "\n";
    return 2;
}
