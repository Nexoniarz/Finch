#include "target.h"

#include <llvm/TargetParser/Host.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#else
#include <sys/wait.h>
#endif

static bool hostIsWindows() {
#ifdef _WIN32
    return true;
#else
    return false;
#endif
}

void setTarget(const std::string &name) {
    std::string host = llvm::sys::getDefaultTargetTriple();
    std::string t = name;
    if (name.empty()) t = host;
    else if (name == "windows") t = hostIsWindows() ? host : "x86_64-w64-mingw32";
    else if (name == "linux") t = "x86_64-unknown-linux-gnu";
    g_target.triple = llvm::Triple(llvm::Triple::normalize(t));
    g_target.windows = g_target.triple.isOSWindows();
    g_target.msvc = g_target.triple.isWindowsMSVCEnvironment();
    g_target.cross = llvm::Triple(llvm::Triple::normalize(host)).getOS() != g_target.triple.getOS();
    g_target.exe = g_target.windows ? ".exe" : "";

    // FINCH_CC chooses the C compiler for any target; CC only when not cross-compiling.
    if (const char *fc = std::getenv("FINCH_CC")) g_target.cc = fc;
    else if (!g_target.cross && std::getenv("CC")) g_target.cc = std::getenv("CC");
    else if (g_target.cross && g_target.windows) g_target.cc = "x86_64-w64-mingw32-gcc";
    else if (g_target.cross) g_target.cc = "clang --target=" + g_target.triple.str();
    else if (hostIsWindows()) g_target.cc = "clang";
    else g_target.cc = "cc";
}

std::string shellQuote(const std::string &s) {
#ifdef _WIN32
    // cmd.exe: double quotes; a " inside a file name is impossible on Windows
    return "\"" + s + "\"";
#else
    std::string out = "'";
    for (char ch : s) out += ch == '\'' ? std::string("'\\''") : std::string(1, ch);
    return out + "'";
#endif
}

static int exitStatus(int status) {
#ifdef _WIN32
    return status;
#else
    if (status == -1) return -1;
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return -1;
#endif
}

int capture(const std::string &cmd, std::string &output) {
    FILE *p = popen((cmd + " 2>&1").c_str(), "r");
    if (!p) return -1;
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, p)) > 0) output.append(buf, n);
    return exitStatus(pclose(p));
}

int runCommand(const std::string &cmd, std::string *crash) {
    std::fflush(stdout);
    int status = std::system(cmd.c_str());
#ifndef _WIN32
    if (crash && status != -1 && WIFSIGNALED(status)) *crash = strsignal(WTERMSIG(status));
#else
    // a crashed Windows program exits with an NTSTATUS code like 0xC0000005
    if (crash && (unsigned)status >= 0xC0000000u) {
        char buf[64];
        std::snprintf(buf, sizeof buf, "exception 0x%08X%s", (unsigned)status,
                      (unsigned)status == 0xC0000005u ? " (access violation)" : (unsigned)status == 0xC00000FDu ? " (stack overflow)" : "");
        *crash = buf;
    }
#endif
    return exitStatus(status);
}

const char *nullDevice() { return hostIsWindows() ? "NUL" : "/dev/null"; }

std::string cacheDir() {
#ifdef _WIN32
    if (const char *l = std::getenv("LOCALAPPDATA")) return std::string(l) + "\\finch";
    if (const char *t = std::getenv("TEMP")) return std::string(t) + "\\finch";
    return "finch-cache";
#else
    if (const char *x = std::getenv("XDG_CACHE_HOME")) return std::string(x) + "/finch";
    if (const char *h = std::getenv("HOME")) return std::string(h) + "/.cache/finch";
    return "/tmp/finch-cache";
#endif
}

char pathListSeparator() { return hostIsWindows() ? ';' : ':'; }

std::string dirOf(const std::string &path) {
    size_t slash = hostIsWindows() ? path.find_last_of("/\\") : path.find_last_of('/');
    return slash == std::string::npos ? "." : path.substr(0, slash);
}
