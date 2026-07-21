#include "process.h"
#include "util.h"
#include <cstdio>
#include <cstdlib>
#include <sys/wait.h>
#include <unistd.h>

int git(std::vector<std::string> args) {
    std::vector<char*> argv;
    argv.push_back(const_cast<char*>("git"));
    for (auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);

    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        execvp("git", argv.data());
        _exit(127);  // exec failed (e.g. git not on PATH)
    }
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) return -1;
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

std::string gitCapture(const std::string& cmdline) {
    std::string full = "git " + cmdline + " 2>/dev/null";
    std::string out;
    FILE* p = popen(full.c_str(), "r");
    if (!p) return out;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, p)) > 0) out.append(buf, n);
    pclose(p);
    return trim(out);
}

std::string tempMessageFilePath() {
    const char* tmpdir = getenv("TMPDIR");
    if (!tmpdir) tmpdir = "/tmp";
    return std::string(tmpdir) + "/gitwrapper_msg_" +
           std::to_string((long)getpid()) + ".txt";
}
