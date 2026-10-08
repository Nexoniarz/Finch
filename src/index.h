#pragma once
// What the compiler learns about a program while checking it, for the language server:
// what each name is, its type, and where it was defined.

#include <string>
#include <vector>

struct SymRef {  // a name in the source
    int file, line, col, len;
    std::string hover;          // markdown shown on mouse-over
    int defFile = -1, defLine = 0, defCol = 0;
};

struct VarInfo {  // a variable or parameter, for completion
    std::string name, type;
    int file, line, col;
    int fnFile, fnLine, fnEnd;  // the function it lives in
};

struct FnInfo {
    std::string module, name, signature, doc;
    std::vector<std::string> params;
    int file, line, col, endLine;
};

struct StructIndex {
    std::string module, name;
    std::vector<std::pair<std::string, std::string>> fields;  // name, type
    int file, line, col;
};

struct SemIndex {
    std::vector<SymRef> refs;
    std::vector<VarInfo> vars;
    std::vector<FnInfo> fns;
    std::vector<StructIndex> structs;
};

inline SemIndex *g_index = nullptr;  // set by the language server; null when compiling
