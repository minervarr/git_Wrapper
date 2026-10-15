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

// The user's home directory ("" if it cannot be determined).
std::string homeDir();

// git_wrapper's per-user config directory: $GIT_WRAPPER_HOME,
// $XDG_CONFIG_HOME/git_wrapper or ~/.config/git_wrapper on POSIX;
// %GIT_WRAPPER_HOME%, %APPDATA%\git_wrapper on Windows. Not created here.
std::string configDir();

// Does the path exist (file or directory)?
bool pathExists(const std::string& path);

// Create a directory and any missing parents. True if it exists afterwards.
bool makeDirs(const std::string& path);

// Best-effort tightening of a file to owner-only (0600). No-op on Windows.
void restrictFilePerms(const std::string& path);

// Absolute path of the running executable ("" if it cannot be determined).
// Used so the credential helper git spawns points back at this binary even
// when it was not found via PATH.
std::string selfExePath();

// Read one line from the terminal without echoing it (for tokens). The prompt
// goes to stderr.
std::string readSecret(const std::string& prompt);

// Render a filesystem path for use inside a shell command on this platform
// (Windows backslashes become forward slashes; POSIX unchanged).
std::string toSshPath(const std::string& path);

// Pure string formatting, identical on every platform.
inline std::string ctxPrefix(const std::string& dir) {
    return dir.empty() ? "" : "-C \"" + dir + "\" ";
}
