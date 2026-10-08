#pragma once

#include <string>
#include <vector>

enum class Tok {
    End,
    Ident, Int, Float, String, Char,

    // keywords
    Fn, Return, If, Else, While, For, In, Break, Continue, True, False, Null, Import, Link,

    // punctuation
    LParen, RParen, LBrace, RBrace, LBracket, RBracket, Comma, Dot, DotDot, Arrow,

    // operators
    Plus, Minus, Star, Slash, Percent,
    Amp, Pipe, Caret, Tilde, Shl, Shr,             // & | ^ ~ << >>
    Assign, Declare,                               // =  :=
    PlusEq, MinusEq, StarEq, SlashEq, PercentEq,   // += -= *= /= %=
    Eq, NotEq, Less, LessEq, Greater, GreaterEq,
    And, Or, Not,
};

struct Token {
    Tok kind;
    std::string text;
    int line;
    int col;
    bool newlineBefore;  // token starts a new line: ends the previous statement
};

std::vector<Token> lex(const std::string &src, const std::string &file);
const char *tokName(Tok t);
