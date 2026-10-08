#include "lexer.h"
#include "error.h"

#include <cctype>
#include <unordered_map>

static const std::unordered_map<std::string, Tok> keywords = {
    {"fn", Tok::Fn},       {"return", Tok::Return}, {"if", Tok::If},
    {"else", Tok::Else},   {"while", Tok::While},   {"for", Tok::For},
    {"in", Tok::In},       {"break", Tok::Break},   {"continue", Tok::Continue},
    {"true", Tok::True},   {"false", Tok::False},   {"null", Tok::Null},
    {"import", Tok::Import}, {"link", Tok::Link},
};

std::vector<Token> lex(const std::string &src, const std::string &file) {
    g_file = file;
    g_source = src;
    std::vector<Token> out;
    size_t i = 0;
    int line = 1, col = 1;
    bool newline = true;

    auto advance = [&]() {
        if (src[i] == '\n') { line++; col = 1; newline = true; }
        else if (((unsigned char)src[i] & 0xC0) != 0x80) col++;  // count characters, not UTF-8 bytes
        i++;
    };
    auto peek = [&](size_t off = 0) -> char {
        return i + off < src.size() ? src[i + off] : '\0';
    };

    while (i < src.size()) {
        char c = src[i];

        if (std::isspace((unsigned char)c)) { advance(); continue; }
        if (c == '/' && peek(1) == '/') {
            while (i < src.size() && src[i] != '\n') advance();
            continue;
        }
        if (c == '/' && peek(1) == '*') {
            int l = line, cl = col;
            advance(); advance();
            while (i < src.size() && !(src[i] == '*' && peek(1) == '/')) advance();
            if (i >= src.size()) fail(l, cl, "comment is never closed (missing */)");
            advance(); advance();
            continue;
        }

        Token t{Tok::End, "", line, col, newline};
        newline = false;

        if (std::isalpha((unsigned char)c) || c == '_') {
            size_t start = i;
            while (std::isalnum((unsigned char)peek()) || peek() == '_') advance();
            t.text = src.substr(start, i - start);
            auto kw = keywords.find(t.text);
            t.kind = kw != keywords.end() ? kw->second : Tok::Ident;
        } else if (std::isdigit((unsigned char)c)) {
            size_t start = i;
            t.kind = Tok::Int;
            if (c == '0' && (peek(1) == 'x' || peek(1) == 'X')) {  // 0xFF
                advance(); advance();
                while (std::isxdigit((unsigned char)peek()) || peek() == '_') advance();
                if (i - start == 2) fail(t.line, t.col, "'0x' needs hex digits after it, like 0xFF");
            } else {
                while (std::isdigit((unsigned char)peek()) || peek() == '_') advance();
                // "1..5" is a range, not the float "1."
                if (peek() == '.' && std::isdigit((unsigned char)peek(1))) {
                    t.kind = Tok::Float;
                    advance();
                    while (std::isdigit((unsigned char)peek()) || peek() == '_') advance();
                }
            }
            for (size_t k = start; k < i; k++)
                if (src[k] != '_') t.text += src[k];
        } else if (c == '"' || c == '\'') {
            char quote = c;
            t.kind = quote == '"' ? Tok::String : Tok::Char;
            advance();
            while (peek() != quote) {
                if (peek() == '\0' || peek() == '\n')
                    fail(t.line, t.col, quote == '"' ? "string is never closed (missing \")"
                                                     : "char is never closed (missing ')");
                char ch = peek();
                advance();
                if (ch == '\\') {
                    char e = peek();
                    advance();
                    switch (e) {
                    case 'n': ch = '\n'; break;
                    case 't': ch = '\t'; break;
                    case 'r': ch = '\r'; break;
                    case '0': ch = '\0'; break;
                    case '\\': ch = '\\'; break;
                    case '"': ch = '"'; break;
                    case '\'': ch = '\''; break;
                    default: fail(line, col - 1, std::string("unknown escape \\") + e);
                    }
                }
                t.text += ch;
            }
            advance();
            if (t.kind == Tok::Char && t.text.size() != 1)
                fail(t.line, t.col, "a char holds exactly one character, like 'a'");
        } else {
            char n = peek(1);
            auto two = [&](Tok k) { t.kind = k; advance(); advance(); };
            auto one = [&](Tok k) { t.kind = k; advance(); };
            switch (c) {
            case '(': one(Tok::LParen); break;
            case ')': one(Tok::RParen); break;
            case '{': one(Tok::LBrace); break;
            case '}': one(Tok::RBrace); break;
            case '[': one(Tok::LBracket); break;
            case ']': one(Tok::RBracket); break;
            case ',': one(Tok::Comma); break;
            case '%': n == '=' ? two(Tok::PercentEq) : one(Tok::Percent); break;
            case '^': one(Tok::Caret); break;
            case '~': one(Tok::Tilde); break;
            case '.': n == '.' ? two(Tok::DotDot) : one(Tok::Dot); break;
            case '+': n == '=' ? two(Tok::PlusEq) : one(Tok::Plus); break;
            case '-': n == '=' ? two(Tok::MinusEq) : n == '>' ? two(Tok::Arrow) : one(Tok::Minus); break;
            case '*': n == '=' ? two(Tok::StarEq) : one(Tok::Star); break;
            case '/': n == '=' ? two(Tok::SlashEq) : one(Tok::Slash); break;
            case '=': n == '=' ? two(Tok::Eq) : one(Tok::Assign); break;
            case '!': n == '=' ? two(Tok::NotEq) : one(Tok::Not); break;
            case '<': n == '=' ? two(Tok::LessEq) : n == '<' ? two(Tok::Shl) : one(Tok::Less); break;
            case '>': n == '=' ? two(Tok::GreaterEq) : n == '>' ? two(Tok::Shr) : one(Tok::Greater); break;
            case ':':
                if (n != '=') fail(line, col, "unexpected ':' (did you mean ':='?)");
                two(Tok::Declare);
                break;
            case '&': n == '&' ? two(Tok::And) : one(Tok::Amp); break;
            case '|': n == '|' ? two(Tok::Or) : one(Tok::Pipe); break;
            default: {
                // show the whole UTF-8 character, not just its first byte
                size_t len = (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 1;
                std::string ch = src.substr(i, len);
                std::string hint = (unsigned char)c >= 0x80 ? " (names use only a-z, A-Z, 0-9 and _; other letters only go inside \"text\")" : "";
                fail(line, col, "unexpected character '" + ch + "'" + hint);
            }
            }
        }
        out.push_back(t);
    }
    out.push_back({Tok::End, "", line, col, true});
    return out;
}

const char *tokName(Tok t) {
    switch (t) {
    case Tok::End: return "end of file";
    case Tok::Ident: return "a name";
    case Tok::Int: return "a number";
    case Tok::Float: return "a number";
    case Tok::String: return "a string";
    case Tok::Char: return "a char";
    case Tok::Fn: return "'fn'";
    case Tok::Return: return "'return'";
    case Tok::If: return "'if'";
    case Tok::Else: return "'else'";
    case Tok::While: return "'while'";
    case Tok::For: return "'for'";
    case Tok::In: return "'in'";
    case Tok::Break: return "'break'";
    case Tok::Continue: return "'continue'";
    case Tok::True: return "'true'";
    case Tok::False: return "'false'";
    case Tok::Null: return "'null'";
    case Tok::Import: return "'import'";
    case Tok::Link: return "'link'";
    case Tok::LParen: return "'('";
    case Tok::RParen: return "')'";
    case Tok::LBrace: return "'{'";
    case Tok::RBrace: return "'}'";
    case Tok::LBracket: return "'['";
    case Tok::RBracket: return "']'";
    case Tok::Comma: return "','";
    case Tok::Dot: return "'.'";
    case Tok::Arrow: return "'->'";
    case Tok::DotDot: return "'..'";
    case Tok::Plus: return "'+'";
    case Tok::Minus: return "'-'";
    case Tok::Star: return "'*'";
    case Tok::Slash: return "'/'";
    case Tok::Percent: return "'%'";
    case Tok::Amp: return "'&'";
    case Tok::Pipe: return "'|'";
    case Tok::Caret: return "'^'";
    case Tok::Tilde: return "'~'";
    case Tok::Shl: return "'<<'";
    case Tok::Shr: return "'>>'";
    case Tok::PercentEq: return "'%='";
    case Tok::Assign: return "'='";
    case Tok::Declare: return "':='";
    case Tok::PlusEq: return "'+='";
    case Tok::MinusEq: return "'-='";
    case Tok::StarEq: return "'*='";
    case Tok::SlashEq: return "'/='";
    case Tok::Eq: return "'=='";
    case Tok::NotEq: return "'!='";
    case Tok::Less: return "'<'";
    case Tok::LessEq: return "'<='";
    case Tok::Greater: return "'>'";
    case Tok::GreaterEq: return "'>='";
    case Tok::And: return "'&&'";
    case Tok::Or: return "'||'";
    case Tok::Not: return "'!'";
    }
    return "?";
}
