#pragma once

#include <cstdio>
#include <cstdlib>
#include <string>

// Finch stops at the first error and shows exactly where it is.
inline std::string g_file;
inline std::string g_source;

[[noreturn]] inline void fail(int line, int col, const std::string &msg) {
    std::fprintf(stderr, "%s:%d:%d: error: %s\n", g_file.c_str(), line, col, msg.c_str());

    size_t start = 0;
    for (int l = 1; l < line && start != std::string::npos; l++) {
        start = g_source.find('\n', start);
        if (start != std::string::npos) start++;
    }
    if (start != std::string::npos && start < g_source.size()) {
        std::string text = g_source.substr(start, g_source.find('\n', start) - start);
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
