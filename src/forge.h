#pragma once
#include <string>

// A parsed git remote URL. Only the parts we need to pick credentials: the
// transport, and the host/owner that identify "which cloud and which account".
struct Remote {
    std::string scheme;  // "ssh", "https", "http", "file", "local", or ""
    std::string user;    // e.g. "git" from git@github.com:...
    std::string host;    // lowercased, e.g. "github.com"
    int port = 0;        // 0 when unspecified
    std::string owner;   // lowercased first path segment, e.g. "minervarr"
    std::string path;    // full path without a leading '/', e.g. "minervarr/repo"

    bool isSsh() const {
        return scheme == "ssh" || scheme == "git+ssh" || scheme == "git";
    }
    bool isHttp() const { return scheme == "https" || scheme == "http"; }
};

// Understands the three SSH spellings (scp-like `git@host:owner/repo.git`,
// `ssh://[user@]host[:port]/owner/repo`, `git+ssh://...`), `https://`/`http://`,
// `file://`, plain local paths and Windows drive letters (`C:\...` must not be
// mistaken for `host:path`).
Remote parseRemote(const std::string& url);

// The match key used against credentials.conf: "host/owner" when there is an
// owner, else "host" ("" when there is no host).
std::string hostOwnerKey(const Remote& r);
