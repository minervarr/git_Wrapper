#pragma once
#include <string>

// Drop any Co-Authored-By trailer, Claude-Session trailer, "Generated with …
// Claude" line, or robot emoji line the message might carry, so only nava is
// ever credited.
std::string sanitize(const std::string& msg);
