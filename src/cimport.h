#pragma once

#include "ast.h"

#include <map>
#include <string>
#include <unordered_map>
#include <vector>

// What Finch learned from `import "something.h"`.
// C structs by value appear in types as Named{name, module "C"}.

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

struct CField {
    std::string name;
    Type type;
    long long offset = 0;  // bytes
    long long size = 0, align = 1;
    bool hidden = false;   // unions, bitfields, unmappable types: kept for layout only
};

struct CStruct {
    std::string name;
    std::vector<CField> fields;
    long long size = 0, align = 1;
    std::string unsupported;  // why it can't be used by value (pointers to it still work)
    bool hasHidden = false;
};

struct CImports {
    std::unordered_map<std::string, CFunc> fns;
    std::unordered_map<std::string, CConst> consts;
    std::unordered_map<std::string, CGlobal> globals;
    std::map<std::string, CStruct> structs;
};

// `dirs`: folders searched first for "local.h" headers (the .fn files' folders).
CImports importHeaders(const std::vector<Import> &imports, const std::vector<std::string> &dirs);
