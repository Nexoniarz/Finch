#include "loader.h"
#include "error.h"
#include "lexer.h"
#include "parser.h"
#include "target.h"

#include <llvm/ADT/SmallString.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

using namespace llvm;

static bool readFile(const std::string &path, std::string &out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::stringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

std::string normalizePath(const std::string &path) {
    SmallString<256> p(path);
    sys::fs::make_absolute(p);
    sys::path::remove_dots(p, true);
    sys::path::native(p);
    return std::string(p.str());
}

void Loader::load(const std::string &path, const std::string &module, Pos from) {
    std::string text;
    bool found = false;
    if (overrides) {
        auto it = overrides->find(normalizePath(path));
        if (it != overrides->end()) text = it->second, found = true;
    }
    if (!found && !readFile(path, text)) {
        if (from.line) failAt(from.file, from.line, from.col, "can't find the module '" + module + "' (looked for " + path + ")");
        std::fprintf(stderr, "error: can't open '%s'\n", path.c_str());
        std::exit(1);
    }
    int idx = (int)g_files.size();
    g_files.push_back({path, text});
    Program p = parse(lex(idx), idx);
    p.module = module;
    p.path = path;
    std::vector<Import> mods;
    for (const Import &im : p.imports)
        if (!im.isC) mods.push_back(im);
    progs.push_back(std::move(p));
    for (const Import &im : mods) {
        if (im.path == "main") failAt(im.pos.file, im.pos.line, im.pos.col, "'main' can't be imported");
        if (loaded[im.path]) continue;
        loaded[im.path] = true;
        load(findModule(dirOf(path), im.path), im.path, im.pos);
    }
}

// next to the importing file, then in $FINCH_PATH folders
std::string Loader::findModule(const std::string &dir, const std::string &name) {
    std::string here = dir + "/" + name + ".fch";
    if (sys::fs::exists(here)) return here;
    if (const char *fp = std::getenv("FINCH_PATH")) {
        std::stringstream ss(fp);
        for (std::string d; std::getline(ss, d, pathListSeparator());)
            if (!d.empty() && sys::fs::exists(d + "/" + name + ".fch")) return d + "/" + name + ".fch";
    }
    return here;
}
