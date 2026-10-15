#pragma once
#include <string>
#include <vector>

#include "credstore.h"

struct ResolveOptions {
    std::string forcedKey;  // --key: force this ssh key, and record it
    bool learn = true;      // persist an unambiguous first-time association
};

struct Resolved {
    std::vector<std::string> args;  // git options to splice after -C, before the verb
    std::string note;               // human-readable; "" when nothing to say
    bool learned = false;

    std::string remoteUrl;
    std::string transport;  // "ssh" | "https" | "file" | "local" | ""
    std::string identity;   // chosen key path / "tokens" / "env:VAR" / ""
    std::string source;     // why it was chosen ("credentials.conf:4", "--key", ...)
};

// Decide how `dir` should authenticate to its origin. Pure of side effects
// except, when `opt.learn` is set, recording a newly discovered ssh key in the
// store. Returns an empty `args` when git's own configuration should be left
// alone (https with no rule, an existing core.sshCommand, a matching
// ~/.ssh/config, or no usable key found).
Resolved resolveForRepo(const std::string& dir, CredStore& store, const ResolveOptions& opt);
