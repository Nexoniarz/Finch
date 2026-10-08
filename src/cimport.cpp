#include "cimport.h"
#include "error.h"
#include "target.h"

#include <clang-c/Index.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>

namespace {

// Ask the system C compiler where it looks for headers, so we find the same stdio.h it would.
std::vector<std::string> systemIncludeDirs() {
    std::string output;
    capture(g_target.cc + " -E -v -x c " + nullDevice(), output);
    std::vector<std::string> dirs;
    std::stringstream lines(output);
    bool inList = false;
    for (std::string l; std::getline(lines, l);) {
        if (!l.empty() && l.back() == '\r') l.pop_back();
        l += "\n";
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
    return dirs;
}

std::string str(CXString s) {
    std::string out = clang_getCString(s) ? clang_getCString(s) : "";
    clang_disposeString(s);
    return out;
}

// Where a C type is used decides how `char *` looks in Finch: in function signatures it is
// a `str`; inside structs, globals and behind pointers it stays a raw `ptr[char]`.
enum class Ctx { Signature, Data };

struct Importer {
    CImports &out;
    CXTranslationUnit tu = nullptr;
    std::string header;
    std::map<std::string, std::string> recordNames;  // record USR -> Finch name (typedef wins)
    std::map<std::string, int> defining;             // USR -> 1 while its fields are being mapped
    int anon = 0;

    explicit Importer(CImports &o) : out(o) {}

    // The Finch name of a C record: its typedef name, its tag, or a made-up one.
    std::string recordName(CXType t) {
        CXCursor decl = clang_getTypeDeclaration(t);
        std::string usr = str(clang_getCursorUSR(decl));
        auto it = recordNames.find(usr);
        if (it != recordNames.end()) return it->second;
        std::string name = str(clang_getCursorSpelling(decl));
        if (name.empty() || name.find(' ') != std::string::npos) name = "anon_struct_" + std::to_string(++anon);
        recordNames[usr] = name;
        return name;
    }

    // Fill out.structs[name] for a complete record; returns its name, or "" if it can't be one.
    std::string defineRecord(CXType t) {
        t = clang_getCanonicalType(t);
        CXCursor decl = clang_getTypeDeclaration(t);
        std::string usr = str(clang_getCursorUSR(decl));
        std::string name = recordName(t);
        if (out.structs.count(name) || defining.count(usr)) return name;
        long long size = clang_Type_getSizeOf(t);
        if (size < 0) return "";  // incomplete (opaque) struct
        defining[usr] = 1;

        CStruct s;
        s.name = name;
        s.size = size;
        s.align = clang_Type_getAlignOf(t);
        if (clang_getCursorKind(decl) == CXCursor_UnionDecl) s.unsupported = "is a C union";
        struct Ctx2 { Importer *self; CStruct *s; } c2{this, &s};
        clang_Type_visitFields(t, [](CXCursor field, CXClientData data) -> CXVisitorResult {
            auto &c = *static_cast<Ctx2 *>(data);
            CField f;
            f.name = str(clang_getCursorSpelling(field));
            CXType ft = clang_getCanonicalType(clang_getCursorType(field));
            f.offset = clang_Cursor_getOffsetOfField(field) / 8;
            f.size = clang_Type_getSizeOf(ft);
            f.align = clang_Type_getAlignOf(ft);
            std::string why;
            if (clang_Cursor_isBitField(field)) {
                c.s->unsupported = "has bit fields";
                f.hidden = true;
            } else if (ft.kind == CXType_Record &&
                       clang_getCursorKind(clang_getTypeDeclaration(ft)) == CXCursor_UnionDecl) {
                f.hidden = true;  // unions are kept as opaque bytes
            } else if (!c.self->mapType(ft, f.type, why, Ctx::Data)) {
                f.hidden = true;
            }
            if (f.hidden) c.s->hasHidden = true;
            if (f.name.empty()) f.hidden = true;
            c.s->fields.push_back(f);
            return CXVisit_Continue;
        }, &c2);
        defining.erase(usr);
        out.structs[name] = s;
        return name;
    }

    // C type -> Finch type. Returns false (with a reason) if Finch can't express it.
    bool mapType(CXType t, Type &res, std::string &why, Ctx ctx) {
        t = clang_getCanonicalType(t);
        long long size = clang_Type_getSizeOf(t);
        auto sized = [&](bool isSigned) {
            switch (size) {
            case 1: res = isSigned ? Type::I8 : Type::U8; return true;
            case 2: res = isSigned ? Type::I16 : Type::U16; return true;
            case 4: res = isSigned ? Type::I32 : Type::U32; return true;
            case 8: res = isSigned ? Type::I64 : Type::U64; return true;
            }
            why = "uses a " + std::to_string(size * 8) + "-bit number";
            return false;
        };

        switch (t.kind) {
        case CXType_Void: res = Type::Void; return true;
        case CXType_Bool: res = Type::Bool; return true;
        case CXType_Char_S:
        case CXType_Char_U: res = Type::Char; return true;
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
        case CXType_Float: res = Type::F32; return true;
        case CXType_Double: res = Type::F64; return true;
        case CXType_Pointer: {
            CXType pointee = clang_getCanonicalType(clang_getPointeeType(t));
            if ((pointee.kind == CXType_Char_S || pointee.kind == CXType_Char_U) && ctx == Ctx::Signature) {
                res = Type::Str;  // char* in a signature is a C string
                return true;
            }
            Type inner;
            std::string ignored;
            if (pointee.kind == CXType_Void || pointee.kind == CXType_FunctionProto ||
                pointee.kind == CXType_FunctionNoProto || !mapType(pointee, inner, ignored, Ctx::Data) ||
                inner.kind == Type::Void || inner.kind == Type::Fixed)
                res = Type::Ptr;  // an opaque `ptr` you can pass around
            else
                res = Type::ptrTo(inner);
            return true;
        }
        case CXType_Record: {
            if (clang_getCursorKind(clang_getTypeDeclaration(t)) == CXCursor_UnionDecl) {
                why = "uses a C union by value";
                return false;
            }
            std::string name = defineRecord(t);
            if (name.empty()) {
                why = "uses an incomplete C struct by value";
                return false;
            }
            res = Type(Type::Named);
            res.name = name;
            res.module = "C";
            return true;
        }
        case CXType_ConstantArray: {
            Type inner;
            if (!mapType(clang_getArrayElementType(t), inner, why, Ctx::Data)) return false;
            res = Type(Type::Fixed);
            res.elem = std::make_shared<Type>(inner);
            res.count = clang_getArraySize(t);
            return true;
        }
        case CXType_LongDouble: why = "uses long double"; return false;
        default: why = "uses the C type '" + str(clang_getTypeSpelling(t)) + "'"; return false;
        }
    }

    // Simple #defines: a single number, maybe negative, maybe in parentheses.
    bool macroValue(CXCursor c, CConst &res) {
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
            res.type = Type::F64;
            res.f = std::strtod(lit.c_str(), &end);
            if (neg) res.f = -res.f;
        } else {
            res.type = Type::I64;
            res.i = (long long)std::strtoull(lit.c_str(), &end, 0);  // C rules: 0x.., 0755
            if (neg) res.i = -res.i;
        }
        for (; *end; end++)  // allow suffixes: 10UL, 1.0f
            if (!std::strchr("uUlLfF", *end)) return false;
        return true;
    }

    void visitTop(CXCursor c) {
        std::string name = str(clang_getCursorSpelling(c));
        switch (clang_getCursorKind(c)) {
        case CXCursor_TypedefDecl: {
            // typedef struct {...} Name;  ->  the struct is called Name in Finch
            CXType u = clang_getCanonicalType(clang_getTypedefDeclUnderlyingType(c));
            if (u.kind == CXType_Record) {
                std::string usr = str(clang_getCursorUSR(clang_getTypeDeclaration(u)));
                // prefer the typedef name, unless the struct is already known under its tag
                auto it = recordNames.find(usr);
                bool used = it != recordNames.end() && out.structs.count(it->second);
                if (!used && !out.structs.count(name)) recordNames[usr] = name;
            }
            break;
        }
        case CXCursor_FunctionDecl: {
            if (out.fns.count(name)) break;
            CFunc f;
            f.name = name;
            f.header = header;
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
            if (!mapType(clang_getResultType(ft), f.ret, why, Ctx::Signature)) f.unsupported = why;
            int n = clang_getNumArgTypes(ft);
            for (int k = 0; k < n && f.unsupported.empty(); k++) {
                Type t;
                if (!mapType(clang_getArgType(ft, k), t, why, Ctx::Signature)) f.unsupported = why;
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
            if (mapType(clang_getCursorType(c), g.type, why, Ctx::Data)) out.globals[name] = g;
            break;
        }
        case CXCursor_StructDecl:
            if (clang_isCursorDefinition(c) && !name.empty() && name.find(' ') == std::string::npos) {
                std::string usr = str(clang_getCursorUSR(c));
                if (!recordNames.count(usr)) recordNames[usr] = name;
            }
            break;
        case CXCursor_EnumDecl:
            clang_visitChildren(c, [](CXCursor k, CXCursor, CXClientData d) {
                static_cast<Importer *>(d)->visitTop(k);
                return CXChildVisit_Continue;
            }, this);
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
            if (macroValue(c, k)) out.consts.emplace(name, k);
            break;
        }
        default: break;
        }
    }
};

}  // namespace

CImports importHeaders(const std::vector<Import> &imports, const std::vector<std::string> &dirs) {
    CImports out;
    bool any = false;
    for (const Import &im : imports) any |= im.isC;
    if (!any) return out;

    // parse for the target system: sizes (long is 32 bits on Windows) and headers differ
    std::vector<std::string> args = {"-x", "c", "-std=gnu11", "--target=" + g_target.triple.str()};
    if (g_target.windows) args.push_back("-D_USE_MATH_DEFINES");  // M_PI and friends, as on Linux
    for (const std::string &d : dirs) args.push_back("-I" + d);
    for (const std::string &d : systemIncludeDirs()) args.push_back("-isystem" + d);
    std::vector<const char *> argv;
    for (const std::string &a : args) argv.push_back(a.c_str());

    Importer imp(out);
    CXIndex index = clang_createIndex(0, 0);
    for (const Import &im : imports) {
        if (!im.isC) continue;
        std::string src = "#include \"" + im.path + "\"\n";
        CXUnsavedFile file = {"finch_import.c", src.c_str(), (unsigned long)src.size()};
        CXTranslationUnit tu = nullptr;
        CXErrorCode err = clang_parseTranslationUnit2(
            index, "finch_import.c", argv.data(), (int)argv.size(), &file, 1,
            CXTranslationUnit_DetailedPreprocessingRecord | CXTranslationUnit_SkipFunctionBodies, &tu);
        if (err != CXError_Success || !tu) failAt(im.pos.file, im.pos.line, im.pos.col, "couldn't read C header '" + im.path + "'");

        for (unsigned k = 0; k < clang_getNumDiagnostics(tu); k++) {
            CXDiagnostic d = clang_getDiagnostic(tu, k);
            if (clang_getDiagnosticSeverity(d) >= CXDiagnostic_Error) {
                std::string msg = str(clang_getDiagnosticSpelling(d));
                clang_disposeDiagnostic(d);
                if (msg.find("file not found") != std::string::npos) {
                    // "'GL/gl.h' file not found": it may be a header that this one includes
                    std::string missing = msg.substr(1, msg.find('\'', 1) - 1);
                    if (missing == im.path)
                        failAt(im.pos.file, im.pos.line, im.pos.col, "can't find the C header '" + im.path + "'");
                    failAt(im.pos.file, im.pos.line, im.pos.col, "can't find the C header '" + missing + "', which '" + im.path +
                                                                     "' includes (is its library's development package installed?)");
                }
                failAt(im.pos.file, im.pos.line, im.pos.col, "C header '" + im.path + "' has an error: " + msg);
            }
            clang_disposeDiagnostic(d);
        }

        imp.tu = tu;
        imp.header = im.path;
        clang_visitChildren(clang_getTranslationUnitCursor(tu), [](CXCursor c, CXCursor, CXClientData d) {
            static_cast<Importer *>(d)->visitTop(c);
            return CXChildVisit_Continue;
        }, &imp);
        clang_disposeTranslationUnit(tu);
    }
    clang_disposeIndex(index);
    return out;
}
