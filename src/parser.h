#pragma once

#include "ast.h"
#include "lexer.h"

Program parse(const std::vector<Token> &tokens);
