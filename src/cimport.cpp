#include "cimport.h"
#include "error.h"

#include <clang-c/Index.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

// Ask the system C compiler where it looks for headers, so we find the same stdio.h it would.
std::vector<std::string> systemIncludeDirs() {
    const char *cc = std::getenv("CC");
    std::string cmd = std::string(cc ? cc : "cc") + " -E -v -x c /dev/null 2>&1";
    std::vector<std::string> dirs;
    FILE *p = popen(cmd.c_str(), "r");
    if (!p) return dirs;
    char line[4096];
    bool inList = false;
    while (std::fgets(line, sizeof line, p)) {
        std::string l = line;
        if (l.rfind("#include <...> search starts here:", 0) == 0) { inList = true; continue; }
        if (l.rfind("End of search list.", 0) == 0) break;
        if (!inList) continue;
        size_t a = l.find_first_not_of(' ');
        size_t z = l.find_last_not_of(" \n");
        if (a == std::string::npos) continue;
        std::string d = l.substr(a, z - a + 1);
        if (d.size() > 20 && d.compare(d.size() - 20, 20, " (framework directory)") == 0) continue;
        dirs.push_back(d);
    }
    pclose(p);
    return dirs;
}

std::string str(CXString s) {
    std::string out = clang_getCString(s) ? clang_getCString(s) : "";
    clang_disposeString(s);
    return out;
}

// C type -> Finch type. Returns false (with a reason) if Finch can't express it yet.
bool mapType(CXType t, Type &out, std::string &why) {
    t = clang_getCanonicalType(t);
    long long size = clang_Type_getSizeOf(t);
    auto sized = [&](bool isSigned) {
        switch (size) {
        case 1: out = isSigned ? Type::I8 : Type::U8; return true;
        case 2: out = isSigned ? Type::I16 : Type::U16; return true;
        case 4: out = isSigned ? Type::I32 : Type::U32; return true;
        case 8: out = isSigned ? Type::I64 : Type::U64; return true;
        }
        why = "uses a " + std::to_string(size * 8) + "-bit number";
        return false;
    };

    switch (t.kind) {
    case CXType_Void: out = Type::Void; return true;
    case CXType_Bool: out = Type::Bool; return true;
    case CXType_Char_S:
    case CXType_Char_U: out = Type::Char; return true;
    case CXType_SChar:
    case CXType_Short:
    case CXType_Int:
    case CXType_Long:
    case CXType_LongLong: return sized(true);
    case CXType_UChar:
    case CXType_UShort:
    case CXType_UInt:
    case CXType_ULong:
    case CXType_ULongLong: return sized(false);
    case CXType_Enum: return sized(true);
    case CXType_Float: out = Type::F32; return true;
    case CXType_Double: out = Type::F64; return true;
    case CXType_Pointer: {
        CXType pointee = clang_getCanonicalType(clang_getPointeeType(t));
        if (pointee.kind == CXType_Char_S || pointee.kind == CXType_Char_U) {
            out = Type::Str;  // char* is a C string
            return true;
        }
        Type inner;
        std::string ignored;
        // pointers to structs, functions, void...: an opaque `ptr` you can pass around
        if (pointee.kind == CXType_Void || !mapType(pointee, inner, ignored) || inner.kind == Type::Void)
            out = Type::Ptr;
        else
            out = Type::ptrTo(inner);
        return true;
    }
    case CXType_Record: why = "passes a C struct by value (structs are coming)"; return false;
    case CXType_LongDouble: why = "uses long double"; return false;
    default: why = "uses the C type '" + str(clang_getTypeSpelling(t)) + "'"; return false;
    }
}

// Simple #defines: a single number, maybe negative, maybe in parentheses.
bool macroValue(CXTranslationUnit tu, CXCursor c, CConst &out) {
    CXToken *toks;
    unsigned n;
    clang_tokenize(tu, clang_getCursorExtent(c), &toks, &n);
    std::vector<std::string> parts;
    for (unsigned k = 1; k < n; k++) {  // token 0 is the macro's name
        if (clang_getTokenKind(toks[k]) == CXToken_Comment) continue;
        parts.push_back(str(clang_getTokenSpelling(tu, toks[k])));
    }
    clang_disposeTokens(tu, toks, n);

    while (parts.size() >= 2 && parts.front() == "(" && parts.back() == ")") {
        parts.erase(parts.begin());
        parts.pop_back();
    }
    bool neg = false;
    if (parts.size() == 2 && parts[0] == "-") {
        neg = true;
        parts.erase(parts.begin());
    }
    if (parts.size() != 1) return false;
    const std::string &lit = parts[0];
    if (lit.empty() || !(std::isdigit((unsigned char)lit[0]) || lit[0] == '.')) return false;

    bool isFloat = lit.find_first_of(".eE") != std::string::npos && lit.rfind("0x", 0) != 0;
    char *end;
    if (isFloat) {
        out.type = Type::F64;
        out.f = std::strtod(lit.c_str(), &end);
        if (neg) out.f = -out.f;
    } else {
        out.type = Type::I64;
        out.i = (long long)std::strtoull(lit.c_str(), &end, 0);  // C rules: 0x.., 0755
        if (neg) out.i = -out.i;
    }
    for (; *end; end++)  // allow suffixes: 10UL, 1.0f
        if (!std::strchr("uUlLfF", *end)) return false;
    return true;
}

struct Visit {
    CImports *out;
    CXTranslationUnit tu;
    std::string header;
};

CXChildVisitResult visit(CXCursor c, CXCursor, CXClientData data) {
    Visit &v = *static_cast<Visit *>(data);
    CImports &out = *v.out;
    std::string name = str(clang_getCursorSpelling(c));

    switch (clang_getCursorKind(c)) {
    case CXCursor_FunctionDecl: {
        if (out.fns.count(name)) break;
        CFunc f;
        f.name = name;
        f.header = v.header;
        if (clang_Cursor_getStorageClass(c) == CX_SC_Static) {
            f.unsupported = "is a static inline C function (it has no code to link against)";
            out.fns[name] = f;
            break;
        }
        CXType ft = clang_getCanonicalType(clang_getCursorType(c));
        if (ft.kind == CXType_FunctionNoProto) {
            f.unsupported = "has no parameter list in its C declaration";
            out.fns[name] = f;
            break;
        }
        std::string why;
        if (!mapType(clang_getResultType(ft), f.ret, why)) f.unsupported = why;
        int n = clang_getNumArgTypes(ft);
        for (int k = 0; k < n && f.unsupported.empty(); k++) {
            Type t;
            if (!mapType(clang_getArgType(ft, k), t, why)) f.unsupported = why;
            f.params.push_back(t);
        }
        f.variadic = clang_isFunctionTypeVariadic(ft);
        out.fns[name] = f;
        break;
    }
    case CXCursor_VarDecl: {
        if (clang_Cursor_getStorageClass(c) != CX_SC_Extern || out.globals.count(name)) break;
        CGlobal g;
        std::string why;
        if (mapType(clang_getCursorType(c), g.type, why)) out.globals[name] = g;
        break;
    }
    case CXCursor_EnumDecl:
        clang_visitChildren(c, visit, data);
        break;
    case CXCursor_EnumConstantDecl: {
        CConst k;
        k.type = Type::I64;
        k.i = clang_getEnumConstantDeclValue(c);
        out.consts.emplace(name, k);
        break;
    }
    case CXCursor_MacroDefinition: {
        if (clang_Cursor_isMacroBuiltin(c) || clang_Cursor_isMacroFunctionLike(c)) break;
        CConst k;
        if (macroValue(v.tu, c, k)) out.consts.emplace(name, k);
        break;
    }
    default: break;
    }
    return CXChildVisit_Continue;
}

}  // namespace

CImports importHeaders(const std::vector<Import> &imports, const std::string &dir) {
    CImports out;
    if (imports.empty()) return out;

    std::vector<std::string> args = {"-x", "c", "-std=gnu11", "-I" + dir};
    for (const std::string &d : systemIncludeDirs()) args.push_back("-isystem" + d);
    std::vector<const char *> argv;
    for (const std::string &a : args) argv.push_back(a.c_str());

    CXIndex index = clang_createIndex(0, 0);
    for (const Import &im : imports) {
        std::string src = "#include \"" + im.path + "\"\n";
        CXUnsavedFile file = {"finch_import.c", src.c_str(), (unsigned long)src.size()};
        CXTranslationUnit tu = nullptr;
        CXErrorCode err = clang_parseTranslationUnit2(
            index, "finch_import.c", argv.data(), (int)argv.size(), &file, 1,
            CXTranslationUnit_DetailedPreprocessingRecord | CXTranslationUnit_SkipFunctionBodies, &tu);
        if (err != CXError_Success || !tu) fail(im.pos.line, im.pos.col, "couldn't read C header '" + im.path + "'");

        for (unsigned k = 0; k < clang_getNumDiagnostics(tu); k++) {
            CXDiagnostic d = clang_getDiagnostic(tu, k);
            if (clang_getDiagnosticSeverity(d) >= CXDiagnostic_Error) {
                std::string msg = str(clang_getDiagnosticSpelling(d));
                clang_disposeDiagnostic(d);
                if (msg.find("file not found") != std::string::npos)
                    fail(im.pos.line, im.pos.col, "can't find the C header '" + im.path + "'");
                fail(im.pos.line, im.pos.col, "C header '" + im.path + "' has an error: " + msg);
            }
            clang_disposeDiagnostic(d);
        }

        Visit v{&out, tu, im.path};
        clang_visitChildren(clang_getTranslationUnitCursor(tu), visit, &v);
        clang_disposeTranslationUnit(tu);
    }
    clang_disposeIndex(index);
    return out;
}
