#include "process.h"
#include "util.h"
#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
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

std::string homeDir() {
    const char* h = getenv("HOME");
    return (h && *h) ? std::string(h) : std::string();
}

std::string configDir() {
    const char* override_ = getenv("GIT_WRAPPER_HOME");
    if (override_ && *override_) return override_;
    const char* xdg = getenv("XDG_CONFIG_HOME");
    if (xdg && *xdg) return std::string(xdg) + "/git_wrapper";
    std::string h = homeDir();
    if (h.empty()) return std::string();
    return h + "/.config/git_wrapper";
}

bool pathExists(const std::string& path) {
    struct stat st;
    return !path.empty() && stat(path.c_str(), &st) == 0;
}

bool makeDirs(const std::string& path) {
    if (path.empty()) return false;
    std::string cur;
    for (size_t i = 0; i < path.size(); ++i) {
        cur.push_back(path[i]);
        if (path[i] == '/' && cur.size() > 1) mkdir(cur.c_str(), 0700);
    }
    mkdir(path.c_str(), 0700);
    return pathExists(path);
}

void restrictFilePerms(const std::string& path) {
    chmod(path.c_str(), S_IRUSR | S_IWUSR);
}

std::string selfExePath() {
    char buf[4096];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) return std::string();
    buf[n] = '\0';
    return std::string(buf);
}

std::string readSecret(const std::string& prompt) {
    fprintf(stderr, "%s", prompt.c_str());
    fflush(stderr);
    termios oldt{};
    bool tty = tcgetattr(STDIN_FILENO, &oldt) == 0;
    if (tty) {
        termios noecho = oldt;
        noecho.c_lflag &= ~(tcflag_t)ECHO;
        tcsetattr(STDIN_FILENO, TCSANOW, &noecho);
    }
    std::string line;
    char c;
    while (read(STDIN_FILENO, &c, 1) == 1 && c != '\n') {
        if (c != '\r') line.push_back(c);
    }
    if (tty) {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        fprintf(stderr, "\n");
    }
    return line;
}

std::string toSshPath(const std::string& path) {
    return path;  // forward slashes already
}
