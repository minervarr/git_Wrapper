#include "sanitize.h"
#include "util.h"
#include <sstream>

std::string sanitize(const std::string& msg) {
    std::istringstream in(msg);
    std::string line, out;
    bool first = true;
    while (std::getline(in, line)) {
        std::string low = toLower(trim(line));
        if (low.rfind("co-authored-by:", 0) == 0) continue;
        if (low.rfind("claude-session:", 0) == 0) continue;
        if (low.find("generated with") != std::string::npos &&
            low.find("claude") != std::string::npos) continue;
        if (line.find("\xF0\x9F\xA4\x96") != std::string::npos) continue;  // robot emoji
        if (!first) out += "\n";
        out += line;
        first = false;
    }
    return trim(out);
}
