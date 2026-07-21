#include "process.h"
#include "util.h"
#include <process.h>
#include <cstdio>
#include <cstdlib>

int git(std::vector<std::string> args) {
    std::vector<char*> argv;
    argv.push_back(const_cast<char*>("git"));
    for (auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);
    return (int)_spawnvp(_P_WAIT, "git", argv.data());
}

std::string gitCapture(const std::string& cmdline) {
    std::string full = "git " + cmdline + " 2>NUL";
    std::string out;
    FILE* p = _popen(full.c_str(), "r");
    if (!p) return out;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, p)) > 0) out.append(buf, n);
    _pclose(p);
    return trim(out);
}

std::string tempMessageFilePath() {
    const char* tmpdir = getenv("TEMP");
    if (!tmpdir) tmpdir = getenv("TMP");
    if (!tmpdir) tmpdir = ".";
    return std::string(tmpdir) + "\\gitwrapper_msg_" +
           std::to_string((long)_getpid()) + ".txt";
}
