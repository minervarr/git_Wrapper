#pragma once
#include <string>

// Implements git's credential-helper protocol (`get` / `store` / `erase`) over
// stdin/stdout. git invokes this as `git_wrapper credential <op>` when a rule
// in credentials.conf selects the `tokens` value for an https remote.
int credentialHelper(const std::string& op);
