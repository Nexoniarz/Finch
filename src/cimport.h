#pragma once

#include "ast.h"

#include <string>
#include <unordered_map>
#include <vector>

// What Finch learned from `import "something.h"`.

struct CFunc {
    std::string name;
    std::string header;  // the import it came from
    Type ret;
    std::vector<Type> params;
    bool variadic = false;
    std::string unsupported;  // non-empty: why Finch can't call it (yet)
};

struct CConst {  // enum values and simple #define numbers
    Type type;
    long long i = 0;
    double f = 0;
};

struct CGlobal {  // extern variables like stdout
    Type type;
};

struct CImports {
    std::unordered_map<std::string, CFunc> fns;
    std::unordered_map<std::string, CConst> consts;
    std::unordered_map<std::string, CGlobal> globals;
};

// `dir` is the folder of the .fn file, searched first for "local.h" headers.
CImports importHeaders(const std::vector<Import> &imports, const std::string &dir);
