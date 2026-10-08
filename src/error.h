#pragma once

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

// Every source file the compiler has read; positions point into this list.
struct SourceFile {
    std::string path;
    std::string text;
};
inline std::vector<SourceFile> g_files;
inline int g_curFile = 0;  // the file the lexer/parser is working on

// The language server needs errors as values: it shows them in the editor and keeps running.
struct FinchError {
    int file, line, col;
    std::string msg;
};
inline bool g_throwErrors = false;

// Finch stops at the first error and shows exactly where it is.
[[noreturn]] inline void failAt(int file, int line, int col, const std::string &msg) {
    if (g_throwErrors) throw FinchError{file, line, col, msg};
    const SourceFile &f = g_files.at(file);
    std::fprintf(stderr, "%s:%d:%d: error: %s\n", f.path.c_str(), line, col, msg.c_str());

    size_t start = 0;
    for (int l = 1; l < line && start != std::string::npos; l++) {
        start = f.text.find('\n', start);
        if (start != std::string::npos) start++;
    }
    if (line > 0 && start != std::string::npos && start < f.text.size()) {
        std::string text = f.text.substr(start, f.text.find('\n', start) - start);
        std::string pad;
        int c = 1;  // columns count characters; skip UTF-8 continuation bytes
        for (size_t k = 0; k < text.size() && c < col; k++) {
            if (((unsigned char)text[k] & 0xC0) == 0x80) continue;
            pad += text[k] == '\t' ? '\t' : ' ';
            c++;
        }
        std::fprintf(stderr, "%5d | %s\n      | %s^\n", line, text.c_str(), pad.c_str());
    }
    std::exit(1);
}

[[noreturn]] inline void fail(int line, int col, const std::string &msg) { failAt(g_curFile, line, col, msg); }
