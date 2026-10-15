#pragma once
#include <string>
#include <vector>

// Expand a leading "~/" (or bare "~") to the user's home directory. Other
// paths are returned unchanged.
std::string expandTilde(const std::string& path);

// The IdentityFile declared for `host` in ~/.ssh/config, or "" if no Host
// block matches. When this is non-empty, ssh already resolves the host and
// git_wrapper must not override it.
std::string sshConfigIdentityFor(const std::string& host);

// Every concrete (non-wildcard) Host pattern in ~/.ssh/config that declares an
// IdentityFile, for `keys sync`. Wildcards and negations are skipped.
struct SshConfigEntry {
    std::string pattern;
    std::string identity;
};
std::vector<SshConfigEntry> sshConfigEntries();

// Private keys in ~/.ssh whose filename follows a ownership convention, in
// priority order: id_<algo>_<owner>, then id_<algo>_<hostlabel>, then
// id_<algo>_<host>_<owner>. Only paths that exist are returned.
std::vector<std::string> conventionKeys(const std::string& host, const std::string& owner);

// `ssh -i '<key>' -o IdentitiesOnly=yes`, ready to use as the value of
// core.sshCommand. The path is rendered for a shell on this platform.
std::string buildSshCommand(const std::string& key);
