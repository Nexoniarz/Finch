#pragma once

#include "ast.h"
#include "lexer.h"

// Parses one file's tokens; `file` is its index in g_files.
Program parse(const std::vector<Token> &tokens, int file);
