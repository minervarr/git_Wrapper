#include "forge.h"
#include "util.h"
#include <cstdlib>

static bool isAllDigits(const std::string& s) {
    if (s.empty()) return false;
    for (char c : s)
        if (c < '0' || c > '9') return false;
    return true;
}

// C:\... , C:/... , \\server\share → a local path, never host:path.
static bool looksLocal(const std::string& s) {
    if (s.empty()) return true;
    if (s[0] == '/') return true;
    if (s.rfind("\\\\", 0) == 0) return true;
    if (s.size() >= 2 && s[1] == ':' &&
        ((s[0] >= 'A' && s[0] <= 'Z') || (s[0] >= 'a' && s[0] <= 'z')) &&
        (s.size() == 2 || s[2] == '/' || s[2] == '\\'))
        return true;
    return false;
}

static void setOwner(Remote& r) {
    size_t slash = r.path.find('/');
    std::string first = (slash == std::string::npos) ? r.path : r.path.substr(0, slash);
    r.owner = toLower(first);
}

static void splitAuthority(const std::string& auth, Remote& r) {
    std::string hostpart = auth;
    size_t at = hostpart.find('@');
    if (at != std::string::npos) {
        r.user = hostpart.substr(0, at);
        hostpart = hostpart.substr(at + 1);
    }
    if (hostpart.empty() || hostpart[0] != '[') {  // skip IPv6 bracket handling
        size_t colon = hostpart.rfind(':');
        if (colon != std::string::npos) {
            std::string maybePort = hostpart.substr(colon + 1);
            if (isAllDigits(maybePort)) {
                r.port = atoi(maybePort.c_str());
                hostpart = hostpart.substr(0, colon);
            }
        }
    }
    r.host = toLower(hostpart);
}

Remote parseRemote(const std::string& url) {
    Remote r;
    std::string s = trim(url);
    if (s.empty()) return r;

    if (s.rfind("file://", 0) == 0 || s.rfind("FILE://", 0) == 0) {
        r.scheme = "file";
        return r;
    }

    size_t sep = s.find("://");
    if (sep != std::string::npos) {
        std::string scheme = toLower(s.substr(0, sep));
        std::string rest = s.substr(sep + 3);
        if (scheme == "file") {
            r.scheme = "file";
            return r;
        }
        if (scheme == "ssh" || scheme == "git+ssh" || scheme == "git")
            r.scheme = "ssh";
        else if (scheme == "https")
            r.scheme = "https";
        else if (scheme == "http")
            r.scheme = "http";
        else
            r.scheme = scheme;

        size_t slash = rest.find('/');
        std::string auth = (slash == std::string::npos) ? rest : rest.substr(0, slash);
        r.path = (slash == std::string::npos) ? "" : rest.substr(slash + 1);
        splitAuthority(auth, r);
        setOwner(r);
        return r;
    }

    if (looksLocal(s)) {
        r.scheme = "local";
        return r;
    }

    // scp-like: [user@]host:path
    size_t colon = s.find(':');
    if (colon == std::string::npos) {
        r.scheme = "local";
        return r;
    }
    std::string left = s.substr(0, colon);
    std::string path = s.substr(colon + 1);
    size_t at = left.find('@');
    if (at != std::string::npos) {
        r.user = left.substr(0, at);
        left = left.substr(at + 1);
    }
    r.scheme = "ssh";
    r.host = toLower(left);
    if (!path.empty() && path[0] == '/') path = path.substr(1);
    r.path = path;
    setOwner(r);
    return r;
}

std::string hostOwnerKey(const Remote& r) {
    if (r.host.empty()) return "";
    if (r.owner.empty()) return r.host;
    return r.host + "/" + r.owner;
}
