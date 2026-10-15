// Unit tests for the pure credential-resolution logic. No toolchain, git or
// network needed: `ctest` (or running the binary) exercises URL parsing, rule
// matching and shell quoting directly.
#include "credstore.h"
#include "forge.h"
#include "util.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

#ifdef _WIN32
#  include <process.h>
#  define GW_GETPID _getpid
static void setEnvVar(const char* k, const char* v) { _putenv_s(k, v); }
static void unsetEnvVar(const char* k) { _putenv_s(k, ""); }
#else
#  include <unistd.h>
#  define GW_GETPID getpid
static void setEnvVar(const char* k, const char* v) { setenv(k, v, 1); }
static void unsetEnvVar(const char* k) { unsetenv(k); }
#endif

static int g_failures = 0;
static int g_checks = 0;

static void check(bool cond, const std::string& what) {
    ++g_checks;
    if (!cond) {
        ++g_failures;
        fprintf(stderr, "FAIL: %s\n", what.c_str());
    }
}

static void checkEq(const std::string& got, const std::string& want,
                    const std::string& what) {
    check(got == want, what + " (got \"" + got + "\", want \"" + want + "\")");
}

static void testScpLike() {
    Remote r = parseRemote("git@github.com:minervarr/repo.git");
    check(r.isSsh(), "scp-like is ssh");
    checkEq(r.user, "git", "scp user");
    checkEq(r.host, "github.com", "scp host");
    checkEq(r.owner, "minervarr", "scp owner");
    checkEq(r.path, "minervarr/repo.git", "scp path");
    checkEq(hostOwnerKey(r), "github.com/minervarr", "scp key");
}

static void testSshUrls() {
    Remote a = parseRemote("ssh://git@codeberg.org:2222/minervarr/repo.git");
    check(a.isSsh(), "ssh:// is ssh");
    checkEq(a.host, "codeberg.org", "ssh host");
    check(a.port == 2222, "ssh port parsed");
    checkEq(a.owner, "minervarr", "ssh owner");

    Remote b = parseRemote("git+ssh://git@github.com/owner/repo.git");
    check(b.isSsh(), "git+ssh is ssh");
    checkEq(b.host, "github.com", "git+ssh host");
    checkEq(b.owner, "owner", "git+ssh owner");

    Remote c = parseRemote("github.com:owner/repo");
    check(c.isSsh(), "host:path without user is ssh");
    checkEq(c.host, "github.com", "userless host");
    checkEq(c.owner, "owner", "userless owner");
}

static void testHttps() {
    Remote r = parseRemote("https://github.com/minervarr/repo.git");
    check(r.isHttp() && !r.isSsh(), "https is http");
    checkEq(r.host, "github.com", "https host");
    checkEq(r.owner, "minervarr", "https owner");

    Remote u = parseRemote("https://user@github.com/minervarr/repo.git");
    checkEq(u.user, "user", "https user");
    checkEq(u.host, "github.com", "https host with user");
}

static void testLocal() {
    checkEq(parseRemote("C:\\Users\\x\\repo").scheme, "local", "windows drive path");
    checkEq(parseRemote("C:/Users/x").scheme, "local", "windows forward path");
    checkEq(parseRemote("/home/x/repo").scheme, "local", "posix absolute path");
    checkEq(parseRemote("file:///srv/repo.git").scheme, "file", "file url");
}

static void testGlob() {
    check(globMatch("github.com/*", "github.com/minervarr"), "host/* matches owner");
    check(globMatch("*/minervarr", "github.com/minervarr"), "*/owner matches");
    check(globMatch("github.com", "github.com"), "exact host");
    check(!globMatch("github.com", "github.com/minervarr"), "host does not match host/owner");
    check(!globMatch("codeberg.org/*", "github.com/minervarr"), "different host");
    check(globMatch("github.com/minervar?", "github.com/minervarr"), "? wildcard");
}

static void testQuote() {
    checkEq(shellQuote("plain"), "'plain'", "plain quoting");
    checkEq(shellQuote("a b"), "'a b'", "space quoting");
    checkEq(shellQuote("it's"), "'it'\\''s'", "single-quote escaping");
}

static void testRuleMatching() {
    const char* tmp = getenv("TMPDIR");
#ifdef _WIN32
    if (!tmp) tmp = getenv("TEMP");
    const char* sep = "\\";
    if (!tmp) { tmp = "."; }
#else
    const char* sep = "/";
    if (!tmp) tmp = "/tmp";
#endif
    std::string path = std::string(tmp) + sep + "git_wrapper_credtest_" +
                       std::to_string((long)GW_GETPID()) + ".conf";
    {
        std::ofstream f(path, std::ios::trunc);
        f << "# comment line\n";
        f << "github.com/minervarr ssh ~/.ssh/k1\n";
        f << "github.com ssh ~/.ssh/k2\n";
        f << "github.com/* ssh ~/.ssh/k3\n";
        f << "codeberg.org ssh ~/.ssh/k9\n";
    }
    setEnvVar("GIT_WRAPPER_CREDENTIALS", path.c_str());
    CredStore store = CredStore::load();
    check(store.rules().size() == 4, "four rules parsed");

    const Rule* exact = store.find("github.com", "minervarr", "ssh");
    check(exact != nullptr, "exact host/owner found");
    if (exact) checkEq(exact->value, "~/.ssh/k1", "most specific wins");

    const Rule* wild = store.find("github.com", "someoneelse", "ssh");
    check(wild != nullptr, "wildcard owner found");
    if (wild) checkEq(wild->value, "~/.ssh/k3", "wildcard value");

    const Rule* hostOnly = store.find("codeberg.org", "", "ssh");
    check(hostOnly != nullptr, "host-only rule found");
    if (hostOnly) checkEq(hostOnly->value, "~/.ssh/k9", "host-only value");

    check(store.find("gitlab.com", "x", "ssh") == nullptr, "unknown host yields nothing");
    check(store.find("github.com", "minervarr", "https") == nullptr, "kind separates rules");

    std::remove(path.c_str());
    unsetEnvVar("GIT_WRAPPER_CREDENTIALS");
}

int main() {
    testScpLike();
    testSshUrls();
    testHttps();
    testLocal();
    testGlob();
    testQuote();
    testRuleMatching();
    printf("%d checks, %d failure(s)\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
