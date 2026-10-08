// The Finch language server: `finch lsp` speaks the Language Server Protocol on stdin/stdout.
//
// Every time a document changes it is checked by the real compiler (lexer, parser, type
// checker and code generator, without optimizing or writing anything), with errors thrown
// instead of ending the process and a semantic index (index.h) collecting what each name is.
// Hover, go-to-definition, completion, signature help and document symbols read that index.

#include "builtins_doc.h"
#include "cimport.h"
#include "codegen.h"
#include "error.h"
#include "index.h"
#include "loader.h"
#include "target.h"

#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/JSON.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

using namespace llvm;

namespace {

// ---------- URIs ----------

std::string uriToPath(const std::string &uri) {
    std::string s = uri.rfind("file://", 0) == 0 ? uri.substr(7) : uri;
    std::string out;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '%' && i + 2 < s.size()) {
            out += (char)std::stoi(s.substr(i + 1, 2), nullptr, 16);
            i += 2;
        } else {
            out += s[i];
        }
    }
#ifdef _WIN32
    if (out.size() > 2 && out[0] == '/' && out[2] == ':') out = out.substr(1);  // /c:/x -> c:/x
#endif
    return normalizePath(out);
}

std::string pathToUri(const std::string &path) {
    std::string p = normalizePath(path), out = "file://";
#ifdef _WIN32
    for (char &c : p)
        if (c == '\\') c = '/';
    out += "/";
#endif
    for (unsigned char c : p) {
        if (isalnum(c) || c == '/' || c == '-' || c == '_' || c == '.' || c == '~' || c == ':') out += (char)c;
        else {
            char buf[4];
            std::snprintf(buf, sizeof buf, "%%%02X", c);
            out += buf;
        }
    }
    return out;
}

// ---------- one check of a document ----------

struct Diagnostic {
    std::string path;
    int line, col;
    std::string msg;
};

struct Analysis {
    std::vector<SourceFile> files;
    SemIndex index;
    std::vector<Diagnostic> diags;
    std::set<std::string> modules;  // modules the document imports
    const CImports *c = nullptr;
};

class Server {
public:
    int run();

private:
    std::map<std::string, std::string> docs;  // normalized path -> text
    std::map<std::string, std::shared_ptr<Analysis>> results;  // last check of each open document
    std::map<std::string, std::shared_ptr<Analysis>> lastGood; // last check without errors
    std::map<std::string, std::unique_ptr<CImports>> headerCache;
    std::set<std::string> diagnosed;  // paths we published errors for
    TargetMachine *tm = nullptr;
    bool shutdown = false;

    // protocol
    bool readMessage(json::Value &out);
    void send(const json::Value &v);
    void reply(const json::Value &id, json::Value result);
    void notify(const std::string &method, json::Value params);
    void handle(const json::Object &msg);

    // features
    void check(const std::string &path);
    json::Value hover(const std::string &path, int line, int col);
    json::Value definition(const std::string &path, int line, int col);
    json::Value completion(const std::string &path, int line, int col);
    json::Value symbols(const std::string &path);
    json::Value signatureHelp(const std::string &path, int line, int col);

    const SymRef *refAt(const Analysis &a, const std::string &path, int line, int col);
    std::string lineText(const std::string &path, int line);
};

bool Server::readMessage(json::Value &out) {
    size_t length = 0;
    std::string header;
    while (std::getline(std::cin, header)) {
        if (!header.empty() && header.back() == '\r') header.pop_back();
        if (header.empty()) break;
        if (header.rfind("Content-Length:", 0) == 0) length = std::stoul(header.substr(15));
    }
    if (!std::cin || length == 0) return false;
    std::string body(length, '\0');
    std::cin.read(&body[0], length);
    Expected<json::Value> v = json::parse(body);
    if (!v) {
        consumeError(v.takeError());
        return true;  // skip a broken message
    }
    out = std::move(*v);
    return true;
}

void Server::send(const json::Value &v) {
    std::string body;
    raw_string_ostream os(body);
    os << v;
    os.flush();
    std::cout << "Content-Length: " << body.size() << "\r\n\r\n" << body;
    std::cout.flush();
}

void Server::reply(const json::Value &id, json::Value result) {
    send(json::Object{{"jsonrpc", "2.0"}, {"id", id}, {"result", std::move(result)}});
}

void Server::notify(const std::string &method, json::Value params) {
    send(json::Object{{"jsonrpc", "2.0"}, {"method", method}, {"params", std::move(params)}});
}

int Server::run() {
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    initTargets();
    setTarget("");
    std::string err;
    const Target *target = TargetRegistry::lookupTarget(g_target.triple, err);
    if (!target) return 1;
    tm = target->createTargetMachine(g_target.triple, "generic", "", TargetOptions(), Reloc::PIC_);

    json::Value msg(nullptr);
    while (readMessage(msg)) {
        if (const json::Object *o = msg.getAsObject()) handle(*o);
        if (shutdown && !std::cin) break;
    }
    return 0;
}

static int intAt(const json::Object *o, const char *key) {
    if (!o) return 0;
    if (auto v = o->getInteger(key)) return (int)*v;
    return 0;
}

void Server::handle(const json::Object &msg) {
    std::string method = msg.getString("method").value_or("").str();
    const json::Value *id = msg.get("id");
    const json::Object *params = msg.getObject("params");
    auto docPath = [&]() -> std::string {
        if (!params) return "";
        const json::Object *td = params->getObject("textDocument");
        return td ? uriToPath(td->getString("uri").value_or("").str()) : "";
    };
    // positions: LSP is 0-based (line, UTF-16 column); Finch is 1-based (line, character)
    auto position = [&](int &line, int &col) {
        const json::Object *pos = params ? params->getObject("position") : nullptr;
        line = intAt(pos, "line") + 1;
        col = intAt(pos, "character") + 1;
    };

    if (method == "initialize") {
        json::Object caps{
            {"textDocumentSync", json::Object{{"openClose", true}, {"change", 1}, {"save", true}}},
            {"hoverProvider", true},
            {"definitionProvider", true},
            {"documentSymbolProvider", true},
            {"completionProvider", json::Object{{"triggerCharacters", json::Array{"."}}}},
            {"signatureHelpProvider", json::Object{{"triggerCharacters", json::Array{"(", ","}}}},
        };
        reply(*id, json::Object{{"capabilities", std::move(caps)},
                                {"serverInfo", json::Object{{"name", "finch"}, {"version", FINCH_VERSION}}}});
    } else if (method == "shutdown") {
        shutdown = true;
        reply(*id, nullptr);
    } else if (method == "exit") {
        std::exit(shutdown ? 0 : 1);
    } else if (method == "textDocument/didOpen" || method == "textDocument/didChange") {
        std::string path = docPath();
        if (method == "textDocument/didOpen") {
            docs[path] = params->getObject("textDocument")->getString("text").value_or("").str();
        } else if (const json::Array *changes = params->getArray("contentChanges")) {
            if (!changes->empty())
                if (const json::Object *ch = changes->back().getAsObject()) docs[path] = ch->getString("text").value_or("").str();
        }
        check(path);
    } else if (method == "textDocument/didSave") {
        check(docPath());
    } else if (method == "textDocument/didClose") {
        std::string path = docPath();
        docs.erase(path);
        results.erase(path);
        lastGood.erase(path);
        notify("textDocument/publishDiagnostics", json::Object{{"uri", pathToUri(path)}, {"diagnostics", json::Array{}}});
    } else if (id) {
        int line = 0, col = 0;
        position(line, col);
        std::string path = docPath();
        if (method == "textDocument/hover") reply(*id, hover(path, line, col));
        else if (method == "textDocument/definition") reply(*id, definition(path, line, col));
        else if (method == "textDocument/completion") reply(*id, completion(path, line, col));
        else if (method == "textDocument/documentSymbol") reply(*id, symbols(path));
        else if (method == "textDocument/signatureHelp") reply(*id, signatureHelp(path, line, col));
        else send(json::Object{{"jsonrpc", "2.0"}, {"id", *id},
                               {"error", json::Object{{"code", -32601}, {"message", "not supported: " + method}}}});
    }
}

// Check `path` with the real compiler, keep the index, publish errors.
void Server::check(const std::string &path) {
    auto a = std::make_shared<Analysis>();
    g_files.clear();
    g_throwErrors = true;
    g_index = &a->index;
    try {
        Loader loader;
        loader.overrides = &docs;
        loader.load(path, "", Pos{});
        std::vector<Import> imports;
        std::vector<std::string> dirs;
        std::string key;
        for (const Program &p : loader.progs) {
            for (const Import &im : p.imports) {
                imports.push_back(im);
                if (im.isC) key += im.path + "\n";
                else if (p.module.empty()) a->modules.insert(im.path);
            }
            std::string d = dirOf(p.path);
            if (std::find(dirs.begin(), dirs.end(), d) == dirs.end()) dirs.push_back(d), key += "@" + d + "\n";
        }
        auto &cached = headerCache[key];  // headers are slow to read: once per set of imports
        if (!cached) cached = std::make_unique<CImports>(importHeaders(imports, dirs));
        a->c = cached.get();
        LLVMContext ctx;
        generate(loader.progs, *cached, ctx, *tm, false);
    } catch (const FinchError &e) {
        std::string file = e.file >= 0 && e.file < (int)g_files.size() ? normalizePath(g_files[e.file].path) : path;
        a->diags.push_back({file, e.line, e.col, e.msg});
    }
    g_index = nullptr;
    g_throwErrors = false;
    a->files = g_files;

    // While the code is half-typed (`p.`, `add(1, `) it doesn't compile; completion and signature
    // help keep using what the last good version knew. Hover and definition use the newest refs.
    if (a->diags.empty()) {
        lastGood[path] = a;
    } else if (auto good = lastGood.find(path); good != lastGood.end()) {
        const Analysis &g = *good->second;
        for (const FnInfo &f : g.index.fns) a->index.fns.push_back(f);
        for (const StructIndex &st : g.index.structs) a->index.structs.push_back(st);
        for (const VarInfo &v : g.index.vars) a->index.vars.push_back(v);
        if (a->modules.empty()) a->modules = g.modules;
        if (!a->c) a->c = g.c;
    }
    results[path] = a;

    std::map<std::string, json::Array> byFile;
    byFile[path];
    for (const std::string &p : diagnosed) byFile[p];  // clear errors that went away
    diagnosed.clear();
    for (const Diagnostic &d : a->diags) {
        int line = std::max(0, d.line - 1), col = std::max(0, d.col - 1);
        byFile[d.path].push_back(json::Object{
            {"range", json::Object{{"start", json::Object{{"line", line}, {"character", col}}},
                                   {"end", json::Object{{"line", line}, {"character", col + 1}}}}},
            {"severity", 1},
            {"source", "finch"},
            {"message", d.msg}});
        diagnosed.insert(d.path);
    }
    for (auto &[p, list] : byFile)
        notify("textDocument/publishDiagnostics", json::Object{{"uri", pathToUri(p)}, {"diagnostics", std::move(list)}});
}

const SymRef *Server::refAt(const Analysis &a, const std::string &path, int line, int col) {
    for (const SymRef &r : a.index.refs) {
        if (r.line != line || col < r.col || col > r.col + r.len) continue;
        if (r.file < 0 || r.file >= (int)a.files.size() || normalizePath(a.files[r.file].path) != path) continue;
        return &r;
    }
    return nullptr;
}

json::Value Server::hover(const std::string &path, int line, int col) {
    auto it = results.find(path);
    if (it == results.end()) return nullptr;
    const SymRef *r = refAt(*it->second, path, line, col);
    if (!r) return nullptr;
    return json::Object{{"contents", json::Object{{"kind", "markdown"}, {"value", r->hover}}}};
}

json::Value Server::definition(const std::string &path, int line, int col) {
    auto it = results.find(path);
    if (it == results.end()) return nullptr;
    const Analysis &a = *it->second;
    const SymRef *r = refAt(a, path, line, col);
    if (!r || r->defFile < 0 || r->defFile >= (int)a.files.size()) return nullptr;
    int l = r->defLine - 1, c = r->defCol - 1;
    return json::Object{{"uri", pathToUri(a.files[r->defFile].path)},
                        {"range", json::Object{{"start", json::Object{{"line", l}, {"character", c}}},
                                               {"end", json::Object{{"line", l}, {"character", c}}}}}};
}

std::string Server::lineText(const std::string &path, int line) {
    auto it = docs.find(path);
    if (it == docs.end()) return "";
    size_t start = 0;
    for (int l = 1; l < line && start != std::string::npos; l++) {
        start = it->second.find('\n', start);
        if (start != std::string::npos) start++;
    }
    if (start == std::string::npos) return "";
    return it->second.substr(start, it->second.find('\n', start) - start);
}

// LSP CompletionItemKind
enum { KMethod = 2, KFunction = 3, KField = 5, KVariable = 6, KModule = 9, KKeyword = 14, KSnippet = 15,
       KConstant = 21, KStruct = 22, KTypeParam = 25 };

static json::Value item(const std::string &label, int kind, const std::string &detail = "", const std::string &doc = "") {
    json::Object o{{"label", label}, {"kind", kind}};
    if (!detail.empty()) o["detail"] = detail;
    if (!doc.empty()) o["documentation"] = json::Object{{"kind", "markdown"}, {"value", doc}};
    return o;
}

json::Value Server::completion(const std::string &path, int line, int col) {
    json::Array items;
    auto it = results.find(path);
    std::shared_ptr<Analysis> a = it == results.end() ? nullptr : it->second;

    // what's before the cursor: "name." or "name.partial"?
    std::string text = lineText(path, line);
    // columns count characters, but we only need the ASCII part right before the cursor
    std::string before;
    {
        int c = 1;
        size_t k = 0;
        for (; k < text.size() && c < col; k++)
            if (((unsigned char)text[k] & 0xC0) != 0x80) c++;
        before = text.substr(0, k);
    }
    size_t end = before.size();
    while (end > 0 && (isalnum((unsigned char)before[end - 1]) || before[end - 1] == '_')) end--;
    bool afterDot = end > 0 && before[end - 1] == '.';

    if (afterDot) {
        size_t s = end - 1;
        while (s > 0 && (isalnum((unsigned char)before[s - 1]) || before[s - 1] == '_' || before[s - 1] == ']' || before[s - 1] == '[')) s--;
        std::string owner = before.substr(s, end - 1 - s);
        if (size_t br = owner.find('['); br != std::string::npos) owner = owner.substr(0, br) + "[]";
        if (!a) return json::Object{{"isIncomplete", false}, {"items", std::move(items)}};

        // a module?
        if (a->modules.count(owner)) {
            std::set<std::string> seen;
            for (const FnInfo &f : a->index.fns)
                if (f.module == owner && seen.insert(f.name).second) items.push_back(item(f.name, KFunction, f.signature));
            for (const StructIndex &s2 : a->index.structs)
                if (s2.module == owner && seen.insert(s2.name).second) items.push_back(item(s2.name, KStruct, "struct " + s2.name));
            return json::Object{{"isIncomplete", false}, {"items", std::move(items)}};
        }
        // a variable: find its type (the innermost one declared before the cursor)
        bool element = owner.size() > 2 && owner.compare(owner.size() - 2, 2, "[]") == 0;
        std::string name = element ? owner.substr(0, owner.size() - 2) : owner;
        std::string type;
        for (const VarInfo &v : a->index.vars)
            if (v.name == name && v.line <= line && line <= v.fnEnd + 1) type = v.type;
        if (element && type.rfind("[]", 0) == 0) type = type.substr(2);
        if (element && type == "str") type = "char";
        if (type.rfind("ptr[", 0) == 0) type = type.substr(4, type.size() - 5);  // fields through a pointer
        if (type == "str") {
            for (const BuiltinDoc &d : kStrMethods) items.push_back(item(d.name, d.name[0] == 'l' || d.name[0] == 'p' ? KField : KMethod, d.signature, d.doc));
        } else if (type.rfind("[]", 0) == 0) {
            for (const BuiltinDoc &d : kArrayMethods) items.push_back(item(d.name, std::string(d.name) == "len" || std::string(d.name) == "ptr" ? KField : KMethod, d.signature, d.doc));
        } else if (!type.empty()) {
            std::string mod, sname = type;
            if (size_t dot = type.find('.'); dot != std::string::npos) mod = type.substr(0, dot), sname = type.substr(dot + 1);
            for (const StructIndex &s2 : a->index.structs)
                if (s2.name == sname && (mod.empty() || s2.module == mod))
                    for (auto &[fname, ftype] : s2.fields) items.push_back(item(fname, KField, ftype + " " + fname));
            if (type.rfind("ptr", 0) == 0 || items.empty()) items.push_back(item("value", KField, "the value the pointer points to"));
        }
        return json::Object{{"isIncomplete", false}, {"items", std::move(items)}};
    }

    std::string prefix = before.substr(end);
    for (const char *k : kKeywords) items.push_back(item(k, KKeyword));
    for (const char *t : kTypeNames) items.push_back(item(t, KTypeParam, "type"));
    for (const BuiltinDoc &d : kBuiltins) items.push_back(item(d.name, KFunction, d.signature, d.doc));
    if (a) {
        std::set<std::string> seen;
        for (const VarInfo &v : a->index.vars)  // variables of the function around the cursor
            if (v.fnLine <= line && line <= v.fnEnd && v.line <= line && seen.insert(v.name).second)
                items.push_back(item(v.name, KVariable, v.type + " " + v.name));
        for (const FnInfo &f : a->index.fns)
            if (f.module.empty() && seen.insert(f.name).second) items.push_back(item(f.name, KFunction, f.signature));
        for (const StructIndex &s : a->index.structs)
            if (s.module.empty() && seen.insert(s.name).second) items.push_back(item(s.name, KStruct, "struct " + s.name));
        for (const std::string &m : a->modules) items.push_back(item(m, KModule, "module " + m));
        // C names only once something is typed: headers declare thousands
        if (a->c && !prefix.empty()) {
            for (auto &[name, f] : a->c->fns)
                if (name.rfind(prefix, 0) == 0 && f.unsupported.empty()) items.push_back(item(name, KFunction, "C: " + f.header));
            for (auto &[name, k] : a->c->consts)
                if (name.rfind(prefix, 0) == 0) items.push_back(item(name, KConstant, "C constant"));
            for (auto &[name, s] : a->c->structs)
                if (name.rfind(prefix, 0) == 0) items.push_back(item(name, KStruct, "C struct"));
        }
    }
    return json::Object{{"isIncomplete", !prefix.empty()}, {"items", std::move(items)}};
}

json::Value Server::symbols(const std::string &path) {
    json::Array out;
    auto it = results.find(path);
    if (it == results.end()) return out;
    const Analysis &a = *it->second;
    auto inDoc = [&](int file) { return file >= 0 && file < (int)a.files.size() && normalizePath(a.files[file].path) == path; };
    auto range = [](int l1, int c1, int l2) {
        return json::Object{{"start", json::Object{{"line", l1 - 1}, {"character", c1 - 1}}},
                            {"end", json::Object{{"line", std::max(l1, l2) - 1}, {"character", 0}}}};
    };
    for (const FnInfo &f : a.index.fns)
        if (inDoc(f.file))
            out.push_back(json::Object{{"name", f.name}, {"detail", f.signature}, {"kind", 12},
                                       {"range", range(f.line, f.col, f.endLine + 1)}, {"selectionRange", range(f.line, f.col, f.line)}});
    for (const StructIndex &s : a.index.structs)
        if (inDoc(s.file)) {
            json::Array kids;
            for (auto &[n, t] : s.fields) kids.push_back(json::Object{{"name", n}, {"detail", t}, {"kind", 8},
                                                                      {"range", range(s.line, s.col, s.line)}, {"selectionRange", range(s.line, s.col, s.line)}});
            out.push_back(json::Object{{"name", s.name}, {"kind", 23}, {"range", range(s.line, s.col, s.line)},
                                       {"selectionRange", range(s.line, s.col, s.line)}, {"children", std::move(kids)}});
        }
    return out;
}

// Inside `name(a, b|`: which function, and which argument.
json::Value Server::signatureHelp(const std::string &path, int line, int col) {
    auto it = results.find(path);
    std::shared_ptr<Analysis> a = it == results.end() ? nullptr : it->second;
    std::string text;
    for (int l = std::max(1, line - 5); l < line; l++) text += lineText(path, l) + "\n";
    std::string cur = lineText(path, line);
    text += cur.substr(0, std::min<size_t>(cur.size(), col - 1));

    int depth = 0, commas = 0;
    size_t k = text.size();
    while (k > 0) {
        char ch = text[--k];
        if (ch == ')' || ch == ']') depth++;
        else if (ch == '[') depth--;
        else if (ch == '(') {
            if (depth == 0) break;
            depth--;
        } else if (ch == ',' && depth == 0) commas++;
        if (k == 0) return nullptr;
    }
    size_t e = k;
    while (k > 0 && (isalnum((unsigned char)text[k - 1]) || text[k - 1] == '_' || text[k - 1] == '.')) k--;
    std::string callee = text.substr(k, e - k);
    if (callee.empty()) return nullptr;
    std::string name = callee.substr(callee.rfind('.') == std::string::npos ? 0 : callee.rfind('.') + 1);
    std::string mod = callee.find('.') == std::string::npos ? "" : callee.substr(0, callee.find('.'));

    std::string label, doc;
    std::vector<std::string> params;
    if (a) {
        for (const FnInfo &f : a->index.fns)
            if (f.name == name && (f.module == mod || (mod.empty() && f.module.empty()) || !a->modules.count(mod)))
                label = f.signature, params = f.params;
        if (label.empty())
            for (const StructIndex &s : a->index.structs)
                if (s.name == name) {
                    label = name + "(";
                    for (size_t i = 0; i < s.fields.size(); i++) {
                        params.push_back(s.fields[i].second + " " + s.fields[i].first);
                        label += (i ? ", " : "") + params.back();
                    }
                    label += ")";
                }
        if (label.empty() && a->c)
            if (auto f = a->c->fns.find(name); f != a->c->fns.end()) {
                label = name + "(";
                for (size_t i = 0; i < f->second.params.size(); i++) {
                    params.push_back(f->second.params[i].show());
                    label += (i ? ", " : "") + params.back();
                }
                label += f->second.variadic ? ", ...)" : ")";
                doc = "C function from " + f->second.header;
            }
    }
    if (label.empty()) {
        auto look = [&](const BuiltinDoc *f, const BuiltinDoc *l) {
            for (; f != l && label.empty(); ++f)
                if (name == f->name) label = f->signature, doc = f->doc;
        };
        look(std::begin(kBuiltins), std::end(kBuiltins));
        if (!mod.empty()) {
            look(std::begin(kArrayMethods), std::end(kArrayMethods));
            look(std::begin(kStrMethods), std::end(kStrMethods));
        }
    }
    if (label.empty()) return nullptr;
    json::Array ps;
    for (const std::string &p : params) ps.push_back(json::Object{{"label", p}});
    json::Object sig{{"label", label}, {"parameters", std::move(ps)}};
    if (!doc.empty()) sig["documentation"] = doc;
    return json::Object{{"signatures", json::Array{std::move(sig)}}, {"activeSignature", 0}, {"activeParameter", commas}};
}

}  // namespace

int runLanguageServer() { return Server().run(); }
