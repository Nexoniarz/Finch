#pragma once

#include <string>
#include <vector>

enum class Tok {
    End,
    Ident, Int, Float, String, Char,

    // keywords
    Fn, Return, If, Else, While, For, In, Break, Continue, True, False, Null, Import, Link, Struct, Defer,

    // punctuation
    LParen, RParen, LBrace, RBrace, LBracket, RBracket, Comma, Colon, Dot, DotDot, Arrow,

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

// Lexes g_files[file].text.
std::vector<Token> lex(int file);
const char *tokName(Tok t);
