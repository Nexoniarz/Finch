#pragma once
// The system Finch compiles for, and everything that depends on it:
// the C compiler used to link, how the shell quotes, file names, the C ABI.

#include <llvm/TargetParser/Triple.h>

#include <string>

struct TargetInfo {
    llvm::Triple triple;
    bool windows = false;   // Windows (MSVC or MinGW): Microsoft x64 calling convention, .exe
    bool msvc = false;      // the MSVC environment (link with clang + the Windows SDK)
    bool cross = false;     // compiling for another system than the one we run on
    std::string cc;         // the C compiler that links programs and builds the runtime
    std::string exe;        // ".exe" or ""
};

inline TargetInfo g_target;

// "windows" / "linux" / a full triple; "" = the system finch runs on
void setTarget(const std::string &name);

// Quote one argument for the shell that runs our commands (sh on Unix, cmd.exe on Windows).
std::string shellQuote(const std::string &s);

// Run a command; its exit status, and everything it printed in `output`.
int capture(const std::string &cmd, std::string &output);

// Run a command with its output going to the terminal; its exit status (128+n for a signal).
int runCommand(const std::string &cmd, std::string *crash = nullptr);

// The null device: /dev/null or NUL.
const char *nullDevice();

// The folder for cached files (~/.cache/finch, %LOCALAPPDATA%\finch).
std::string cacheDir();

// Separator of FINCH_PATH entries (':' or ';').
char pathListSeparator();

// The folder part of a path (both / and \ on Windows).
std::string dirOf(const std::string &path);
