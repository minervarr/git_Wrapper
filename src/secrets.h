#pragma once
#include <string>
#include <vector>

// The 0600 token file: one line per entry, tab-separated
//   <match>\t<username>\t<token>
// match is a concrete "host/owner" or "host" (never a glob — the helper turns
// the git-provided host/path into the same concrete key).
class SecretStore {
  public:
    static SecretStore load();
    static std::string defaultPath();

    const std::string& path() const { return path_; }

    bool set(const std::string& match, const std::string& user, const std::string& token);
    // First key in `keys` that has an entry wins.
    bool getFirst(const std::vector<std::string>& keys, std::string& user,
                  std::string& token) const;
    bool erase(const std::string& match);
    bool save() const;

  private:
    struct Entry {
        std::string match, user, token;
    };
    std::vector<Entry> entries_;
    std::string path_;
};
