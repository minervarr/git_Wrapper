#pragma once
#include <string>
#include <vector>

std::string currentBranch(const std::string& dir);

// Direct submodules (non-recursive: nested third-party submodules are not ours
// to push). Uninitialised entries (leading '-') are skipped.
std::vector<std::string> submodulePaths();

// -1 = no upstream (unknown); otherwise the number of commits HEAD is ahead.
int commitsAhead(const std::string& dir);

// Refuse to push if any not-yet-pushed commit carries a Co-Authored-By or
// Claude-Session trailer (e.g. a commit made with plain `git` outside this
// wrapper).
bool verifyNoTrailers(const std::string& dir);
