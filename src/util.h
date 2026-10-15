#pragma once
#include <string>

std::string toLower(std::string s);
std::string trim(const std::string& s);

// Quote a string for a POSIX shell (single quotes, with '\'' escaping). Used
// for the values we hand to git as `core.sshCommand` / `credential.helper`,
// which git runs through a shell. Always quoting is safe and keeps Windows
// paths with spaces intact.
std::string shellQuote(const std::string& s);

// Case-sensitive glob with a single wildcard: '*' matches any run of
// characters (including '/'), '?' matches one. Used by the credentials store
// to let a rule be `github.com/minervarr`, `github.com/*`, or `*/minervarr`.
bool globMatch(const std::string& pattern, const std::string& text);
