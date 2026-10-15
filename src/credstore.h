#pragma once
#include <string>
#include <vector>

// One line of credentials.conf: which identity to use for which remote.
//   <match>  <kind>  <value>
// match = "host", "host/owner", or a glob of either (e.g. "github.com/*",
// "*/minervarr"). kind = "ssh" | "https". value is a reference, never a
// secret: an ssh key path, "tokens" (the 0600 secrets file), or "env:VAR".
struct Rule {
    std::string match;
    std::string kind;
    std::string value;
    int line = 0;  // source line in the file, for `keys list` / diagnostics
};

class CredStore {
  public:
    // Reads GIT_WRAPPER_CREDENTIALS, else <configDir>/credentials.conf.
    // A missing file yields an empty store.
    static CredStore load();
    static std::string defaultPath();

    const std::vector<Rule>& rules() const { return rules_; }
    const std::string& path() const { return path_; }

    // Most specific match wins: "host/owner" first, then "host". Within a
    // candidate, the first rule in file order wins. nullptr when none match.
    const Rule* find(const std::string& host, const std::string& owner,
                     const std::string& kind) const;

    // Replace-or-append. Returns true when something changed.
    bool add(const std::string& match, const std::string& kind, const std::string& value);
    // Drop every rule for this match+kind (kind empty = every kind).
    bool remove(const std::string& match, const std::string& kind);
    // Write the file (0600), creating the directory. False on I/O failure.
    bool save() const;

  private:
    std::vector<Rule> rules_;
    std::string path_;
};
