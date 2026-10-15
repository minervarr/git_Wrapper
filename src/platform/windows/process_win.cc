#include "process.h"
#include "util.h"
#include <process.h>
#include <windows.h>
#include <conio.h>
#include <direct.h>
#include <io.h>
#include <cstdio>
#include <cstdlib>
#include <iostream>

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
           std::to_string((unsigned long)GetCurrentProcessId()) + ".txt";
}

std::string homeDir() {
    const char* h = getenv("USERPROFILE");
    if (h && *h) return h;
    const char* drive = getenv("HOMEDRIVE");
    const char* path = getenv("HOMEPATH");
    if (drive && path) return std::string(drive) + path;
    return std::string();
}

std::string configDir() {
    const char* override_ = getenv("GIT_WRAPPER_HOME");
    if (override_ && *override_) return override_;
    const char* appdata = getenv("APPDATA");
    if (appdata && *appdata) return std::string(appdata) + "\\git_wrapper";
    std::string h = homeDir();
    if (h.empty()) return std::string();
    return h + "\\.config\\git_wrapper";
}

bool pathExists(const std::string& path) {
    if (path.empty()) return false;
    DWORD a = GetFileAttributesA(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES;
}

bool makeDirs(const std::string& path) {
    if (path.empty()) return false;
    std::string cur;
    for (size_t i = 0; i < path.size(); ++i) {
        cur.push_back(path[i]);
        if ((path[i] == '\\' || path[i] == '/') && cur.size() > 3 &&
            cur[cur.size() - 2] != ':') {
            _mkdir(cur.c_str());
        }
    }
    _mkdir(path.c_str());
    return pathExists(path);
}

void restrictFilePerms(const std::string&) {
    // Windows ACLs are inherited from the parent directory; nothing simple to
    // tighten here. The file lives under %APPDATA% (per-user) already.
}

std::string selfExePath() {
    char buf[MAX_PATH];
    DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return std::string();
    return std::string(buf, n);
}

std::string readSecret(const std::string& prompt) {
    fprintf(stderr, "%s", prompt.c_str());
    fflush(stderr);
    // Piped input (tests, scripts) has no console to read key-by-key from.
    if (!_isatty(_fileno(stdin))) {
        std::string line;
        std::getline(std::cin, line);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        return line;
    }
    std::string line;
    int c;
    while ((c = _getch()) != EOF) {
        if (c == '\r' || c == '\n') break;
        if (c == '\b') {
            if (!line.empty()) line.pop_back();
            continue;
        }
        line.push_back((char)c);
    }
    fprintf(stderr, "\n");
    return line;
}

std::string toSshPath(const std::string& path) {
    std::string out = path;
    for (char& c : out)
        if (c == '\\') c = '/';
    return out;
}
