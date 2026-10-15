#include "resolve.h"
#include "forge.h"
#include "process.h"
#include "ssh_identity.h"
#include "util.h"

static std::string httpsHelperValue() {
    std::string self = selfExePath();
    if (self.empty()) self = "git_wrapper";
    return "!" + shellQuote(self) + " credential";
}

Resolved resolveForRepo(const std::string& dir, CredStore& store,
                        const ResolveOptions& opt) {
    Resolved res;
    res.remoteUrl = gitCapture(ctxPrefix(dir) + "config --get remote.origin.url");
    if (res.remoteUrl.empty()) return res;

    Remote r = parseRemote(res.remoteUrl);
    res.transport = r.scheme;
    if (r.host.empty()) return res;

    if (r.isHttp()) {
        const Rule* rule = store.find(r.host, r.owner, "https");
        if (!rule) return res;  // leave git's existing credential handling alone
        res.args = {
            "-c", "credential.helper=",  // reset inherited helpers for this run
            "-c", "credential.helper=" + httpsHelperValue(),
            "-c", "credential.useHttpPath=true",
        };
        res.identity = rule->value;
        res.source = rule->line > 0 ? "credentials.conf:" + std::to_string(rule->line)
                                    : "credentials.conf";
        res.note = "https credentials for " + hostOwnerKey(r) + " via git_wrapper (" +
                   res.source + ")";
        return res;
    }

    if (!r.isSsh()) return res;  // file:// or local

    std::string match = hostOwnerKey(r);

    if (!opt.forcedKey.empty()) {
        res.identity = opt.forcedKey;
        res.source = "--key";
        res.args = {"-c", "core.sshCommand=" + buildSshCommand(expandTilde(opt.forcedKey))};
        if (!match.empty()) {
            if (store.add(match, "ssh", expandTilde(opt.forcedKey))) {
                store.save();
                res.learned = true;
            }
        }
        res.note = "using forced ssh key " + res.identity + " for " + match;
        return res;
    }

    // Respect an explicit core.sshCommand (repo or global).
    if (!gitCapture(ctxPrefix(dir) + "config --get core.sshCommand").empty()) {
        res.note = match + ": core.sshCommand already set — left alone";
        return res;
    }

    if (const Rule* rule = store.find(r.host, r.owner, "ssh")) {
        res.identity = expandTilde(rule->value);
        res.source = rule->line > 0 ? "credentials.conf:" + std::to_string(rule->line)
                                    : "credentials.conf";
        res.args = {"-c", "core.sshCommand=" + buildSshCommand(res.identity)};
        res.note = "using ssh key " + res.identity + " for " + match + " (" + res.source + ")";
        return res;
    }

    // A ~/.ssh/config Host block already resolves this host — never override it.
    if (!sshConfigIdentityFor(r.host).empty()) {
        res.note = match + ": ~/.ssh/config resolves this host — left alone";
        return res;
    }

    std::vector<std::string> cands = conventionKeys(r.host, r.owner);
    if (cands.size() == 1) {
        res.identity = cands[0];
        res.args = {"-c", "core.sshCommand=" + buildSshCommand(res.identity)};
        if (opt.learn && !match.empty() && store.add(match, "ssh", res.identity)) {
            store.save();
            res.learned = true;
            res.source = "learned";
            res.note = "learned " + match + " → " + res.identity + " (saved)";
        } else {
            res.source = "key name";
            res.note = "using ssh key " + res.identity + " for " + match;
        }
        return res;
    }
    if (cands.size() > 1) {
        std::string list;
        for (size_t i = 0; i < cands.size(); ++i) list += (i ? ", " : "") + cands[i];
        res.note = match + ": several candidate keys (" + list +
                   ") — refusing to guess; run `git_wrapper keys add " + match + " <key>`";
        return res;
    }
    return res;  // fall through to ssh defaults
}
