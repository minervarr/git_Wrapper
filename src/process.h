#pragma once
#include <string>
#include <vector>

// Run git with explicit argv — no shell, so spaces/quotes in a commit message
// can never be misparsed. stdio is inherited, so git's own output is shown.
// Implemented per platform (see platform/windows, platform/linux).
int git(std::vector<std::string> args);

// Run a git query and capture its trimmed stdout (stderr silenced).
// Implemented per platform.
std::string gitCapture(const std::string& cmdline);

// A unique path for the temp commit-message file, in the platform's temp
// dir. Implemented per platform (env var and separator differ).
std::string tempMessageFilePath();

// Pure string formatting, identical on every platform.
inline std::string ctxPrefix(const std::string& dir) {
    return dir.empty() ? "" : "-C \"" + dir + "\" ";
}
