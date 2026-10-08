#pragma once
// Reads the main file and, recursively, every Finch module it imports.

#include "ast.h"

#include <map>
#include <string>
#include <vector>

struct Loader {
    std::vector<Program> progs;
    std::map<std::string, bool> loaded;          // module name -> seen
    const std::map<std::string, std::string> *overrides = nullptr;  // path -> text (files open in an editor)

    void load(const std::string &path, const std::string &module, Pos from);
    static std::string findModule(const std::string &dir, const std::string &name);
};

// A path made absolute and tidy, so two spellings of one file compare equal.
std::string normalizePath(const std::string &path);
