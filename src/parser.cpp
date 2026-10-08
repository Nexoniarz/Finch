#include "parser.h"
#include "error.h"

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <unordered_map>

bool typeFromName(const std::string &name, Type &out) {
    static const std::unordered_map<std::string, Type::Kind> names = {
        {"int", Type::I64},  {"float", Type::F64}, {"bool", Type::Bool}, {"char", Type::Char},
        {"str", Type::Str},  {"ptr", Type::Ptr},   {"i8", Type::I8},     {"i16", Type::I16},
        {"i32", Type::I32},  {"i64", Type::I64},   {"u8", Type::U8},     {"u16", Type::U16},
        {"u32", Type::U32},  {"u64", Type::U64},   {"f32", Type::F32},   {"f64", Type::F64},
    };
    auto it = names.find(name);
    if (it == names.end()) return false;
    out = Type(it->second);
    return true;
}

namespace {

bool isTypeName(const std::string &name) {
    Type t;
    return typeFromName(name, t);
}

class Parser {
public:
    explicit Parser(const std::vector<Token> &t) : toks(t) {}

    Program program() {
        Program p;
        while (!at(Tok::End)) {
            if (at(Tok::Import)) p.imports.push_back(import());
            else if (at(Tok::Link)) p.links.push_back(link());
            else p.fns.push_back(function());
        }
        return p;
    }

private:
    const std::vector<Token> &toks;
    size_t i = 0;
    int parenDepth = 0;  // inside ( ) newlines don't end anything

    const Token &cur() const { return toks[i]; }
    const Token &peekTok(size_t off = 1) const { return toks[std::min(i + off, toks.size() - 1)]; }
    const Token &next() { return toks[i++]; }
    bool at(Tok k) const { return cur().kind == k; }
    Pos pos() const { return {cur().line, cur().col}; }

    std::string describe(const Token &t) const {
        return t.kind == Tok::Ident ? "'" + t.text + "'" : tokName(t.kind);
    }

    bool accept(Tok k) {
        if (!at(k)) return false;
        i++;
        return true;
    }
    const Token &expect(Tok k, const char *what = nullptr) {
        if (!at(k))
            fail(cur().line, cur().col,
                 std::string("expected ") + (what ? what : tokName(k)) + ", found " + describe(cur()));
        return next();
    }
    [[noreturn]] void unexpected(const char *ctx) {
        fail(cur().line, cur().col, "unexpected " + describe(cur()) + " " + ctx);
    }

    // A token can continue an expression only if it is on the same line (or we're inside parens).
    bool sameLine() const { return parenDepth > 0 || !cur().newlineBefore; }

    // int, u8, ptr, ptr[int], ptr[ptr[u8]] ...
    Type type() {
        if (!at(Tok::Ident) || !isTypeName(cur().text))
            unexpected("(expected a type like int, float, u8, str or ptr[int])");
        Type t;
        typeFromName(next().text, t);
        if (t.kind == Type::Ptr && accept(Tok::LBracket)) {
            t = Type::ptrTo(type());
            expect(Tok::RBracket);
        }
        return t;
    }

    // Does a declaration like `int x` or `ptr[int] p` start here?
    bool atDeclaration() const {
        if (!at(Tok::Ident) || !isTypeName(cur().text)) return false;
        Tok n = peekTok().kind;
        return n == Tok::Ident || (cur().text == "ptr" && n == Tok::LBracket);
    }

    // ---------- top level ----------

    Import import() {
        Import im;
        im.pos = pos();
        expect(Tok::Import);
        if (at(Tok::Ident))
            fail(im.pos.line, im.pos.col, "Finch modules are not ready yet; for C headers write import \"stdio.h\"");
        im.path = expect(Tok::String, "a header name in quotes, like \"stdio.h\"").text;
        return im;
    }

    Link link() {
        Link l;
        l.pos = pos();
        expect(Tok::Link);
        l.lib = expect(Tok::String, "a library name in quotes, like link \"glfw\"").text;
        if (l.lib.rfind("lib", 0) == 0 || l.lib.find(".so") != std::string::npos || l.lib.find('/') != std::string::npos)
            fail(l.pos.line, l.pos.col, "write just the library's name: for libglfw.so that's  link \"glfw\"");
        return l;
    }

    FnDecl function() {
        FnDecl fn;
        fn.pos = pos();
        if (at(Tok::Ident) && isTypeName(cur().text))
            fail(fn.pos.line, fn.pos.col, "functions start with 'fn', like: fn add(int a, int b) -> int { ... }");
        expect(Tok::Fn, "'fn' (every function starts with fn)");

        fn.name = expect(Tok::Ident, "a function name").text;
        expect(Tok::LParen);
        parenDepth++;
        if (!at(Tok::RParen)) {
            do {
                Param p;
                p.pos = pos();
                p.type = type();
                p.name = expect(Tok::Ident, "a parameter name").text;
                fn.params.push_back(p);
            } while (accept(Tok::Comma));
        }
        parenDepth--;
        expect(Tok::RParen);
        fn.ret = accept(Tok::Arrow) ? type() : Type(Type::Void);
        fn.body = block();
        return fn;
    }

    // ---------- statements ----------

    BlockPtr block() {
        auto b = std::make_unique<BlockStmt>(pos());
        expect(Tok::LBrace);
        int saved = parenDepth;
        parenDepth = 0;
        while (!at(Tok::RBrace)) {
            if (at(Tok::End)) fail(b->pos.line, b->pos.col, "this '{' is never closed (missing '}')");
            b->body.push_back(statement());
            endOfStatement();
        }
        parenDepth = saved;
        next();
        return b;
    }

    void endOfStatement() {
        if (at(Tok::RBrace) || at(Tok::End) || cur().newlineBefore) return;
        unexpected("(put each statement on its own line)");
    }

    StmtPtr statement() {
        Pos p = pos();
        switch (cur().kind) {
        case Tok::If: return ifStmt();
        case Tok::While: {
            next();
            auto s = std::make_unique<WhileStmt>(p);
            s->cond = expr();
            s->body = block();
            return s;
        }
        case Tok::For: {
            next();
            auto s = std::make_unique<ForStmt>(p);
            s->var = expect(Tok::Ident, "a loop variable name").text;
            expect(Tok::In);
            s->from = expr();
            expect(Tok::DotDot, "'..' (like 'for i in 0..10')");
            s->to = expr();
            s->body = block();
            return s;
        }
        case Tok::Return: {
            next();
            auto s = std::make_unique<ReturnStmt>(p);
            if (!at(Tok::RBrace) && !cur().newlineBefore) s->value = expr();
            return s;
        }
        case Tok::Break: next(); return std::make_unique<BreakStmt>(p);
        case Tok::Continue: next(); return std::make_unique<ContinueStmt>(p);
        case Tok::LBrace: return block();
        case Tok::Fn: fail(p.line, p.col, "functions can't be inside other functions (move it out)");
        case Tok::Import:
        case Tok::Link: fail(p.line, p.col, "import and link go at the top of the file, outside functions");
        default: break;
        }

        // int x = 5
        if (atDeclaration()) {
            auto s = std::make_unique<VarDeclStmt>(p);
            s->hasType = true;
            s->type = type();
            s->name = next().text;
            if (accept(Tok::Assign)) s->init = expr();
            return s;
        }

        // x := 5
        if (at(Tok::Ident) && peekTok().kind == Tok::Declare) {
            auto s = std::make_unique<VarDeclStmt>(p);
            s->hasType = false;
            s->name = next().text;
            next();
            s->init = expr();
            return s;
        }

        ExprPtr e = expr();

        // x = 5, p.value += 1, ...
        char op = 0;
        switch (cur().kind) {
        case Tok::Assign: op = '='; break;
        case Tok::PlusEq: op = '+'; break;
        case Tok::MinusEq: op = '-'; break;
        case Tok::StarEq: op = '*'; break;
        case Tok::SlashEq: op = '/'; break;
        case Tok::PercentEq: op = '%'; break;
        case Tok::Declare:
            fail(cur().line, cur().col, "':=' creates a new variable, so the left side must be a plain name");
        default: break;
        }
        if (op && sameLine()) {
            if (e->kind != ExprKind::Var && e->kind != ExprKind::Member)
                fail(p.line, p.col, "only a variable or p.value can be assigned to");
            auto s = std::make_unique<AssignStmt>(p);
            s->target = std::move(e);
            s->op = op;
            next();
            s->value = expr();
            return s;
        }

        if (e->kind != ExprKind::Call) fail(p.line, p.col, "this value is computed but never used");
        return std::make_unique<ExprStmt>(p, std::move(e));
    }

    StmtPtr ifStmt() {
        auto s = std::make_unique<IfStmt>(pos());
        expect(Tok::If);
        s->cond = expr();
        s->then = block();
        if (accept(Tok::Else)) {
            if (at(Tok::If)) s->otherwise = ifStmt();
            else s->otherwise = block();
        }
        return s;
    }

    // ---------- expressions (lowest to highest precedence) ----------

    ExprPtr expr() { return orExpr(); }

    ExprPtr orExpr() {
        ExprPtr l = andExpr();
        while (sameLine() && at(Tok::Or)) {
            Pos p = pos();
            next();
            l = std::make_unique<BinaryExpr>(p, BinOp::Or, std::move(l), andExpr());
        }
        return l;
    }

    ExprPtr andExpr() {
        ExprPtr l = compare();
        while (sameLine() && at(Tok::And)) {
            Pos p = pos();
            next();
            l = std::make_unique<BinaryExpr>(p, BinOp::And, std::move(l), compare());
        }
        return l;
    }

    ExprPtr compare() {
        ExprPtr l = additive();
        while (sameLine()) {
            BinOp op;
            switch (cur().kind) {
            case Tok::Eq: op = BinOp::Eq; break;
            case Tok::NotEq: op = BinOp::NotEq; break;
            case Tok::Less: op = BinOp::Less; break;
            case Tok::LessEq: op = BinOp::LessEq; break;
            case Tok::Greater: op = BinOp::Greater; break;
            case Tok::GreaterEq: op = BinOp::GreaterEq; break;
            default: return l;
            }
            Pos p = pos();
            next();
            l = std::make_unique<BinaryExpr>(p, op, std::move(l), additive());
        }
        return l;
    }

    // Precedence is Go's, not C's: bit operators bind tighter than comparisons,
    // so `x & 1 == 0` means `(x & 1) == 0`.
    ExprPtr additive() {  // + - | ^
        ExprPtr l = multiplicative();
        while (sameLine()) {
            BinOp op;
            switch (cur().kind) {
            case Tok::Plus: op = BinOp::Add; break;
            case Tok::Minus: op = BinOp::Sub; break;
            case Tok::Pipe: op = BinOp::BitOr; break;
            case Tok::Caret: op = BinOp::BitXor; break;
            default: return l;
            }
            Pos p = pos();
            next();
            l = std::make_unique<BinaryExpr>(p, op, std::move(l), multiplicative());
        }
        return l;
    }

    ExprPtr multiplicative() {  // * / % << >> &
        ExprPtr l = unary();
        while (sameLine()) {
            BinOp op;
            switch (cur().kind) {
            case Tok::Star: op = BinOp::Mul; break;
            case Tok::Slash: op = BinOp::Div; break;
            case Tok::Percent: op = BinOp::Mod; break;
            case Tok::Shl: op = BinOp::Shl; break;
            case Tok::Shr: op = BinOp::Shr; break;
            case Tok::Amp: op = BinOp::BitAnd; break;
            default: return l;
            }
            Pos p = pos();
            next();
            l = std::make_unique<BinaryExpr>(p, op, std::move(l), unary());
        }
        return l;
    }

    ExprPtr unary() {
        Pos p = pos();
        if (accept(Tok::Minus)) return std::make_unique<UnaryExpr>(p, '-', unary());
        if (accept(Tok::Not)) return std::make_unique<UnaryExpr>(p, '!', unary());
        if (accept(Tok::Tilde)) return std::make_unique<UnaryExpr>(p, '~', unary());
        return postfix();
    }

    // p.value
    ExprPtr postfix() {
        ExprPtr e = primary();
        while (sameLine() && at(Tok::Dot)) {
            Pos p = pos();
            next();
            e = std::make_unique<MemberExpr>(p, std::move(e), expect(Tok::Ident, "a name after '.'").text);
        }
        return e;
    }

    // name(arg, arg, ...) -- we are at the '('
    ExprPtr callArgs(Pos p, const std::string &name) {
        auto call = std::make_unique<CallExpr>(p, name);
        expect(Tok::LParen);
        parenDepth++;
        if (!at(Tok::RParen)) {
            do call->args.push_back(expr());
            while (accept(Tok::Comma));
        }
        parenDepth--;
        expect(Tok::RParen);
        return call;
    }

    ExprPtr primary() {
        Pos p = pos();
        const Token &t = cur();
        switch (t.kind) {
        case Tok::Int: {
            next();
            errno = 0;
            unsigned long long v = std::strtoull(t.text.c_str(), nullptr, t.text.size() > 1 && (t.text[1] == 'x' || t.text[1] == 'X') ? 16 : 10);
            if (errno == ERANGE || v > (unsigned long long)LLONG_MAX)
                fail(p.line, p.col, "number is too big for int");
            return std::make_unique<IntExpr>(p, (long long)v);
        }
        case Tok::Float: next(); return std::make_unique<FloatExpr>(p, std::strtod(t.text.c_str(), nullptr));
        case Tok::True: next(); return std::make_unique<BoolExpr>(p, true);
        case Tok::False: next(); return std::make_unique<BoolExpr>(p, false);
        case Tok::Null: next(); return std::make_unique<NullExpr>(p);
        case Tok::Char: next(); return std::make_unique<CharExpr>(p, t.text[0]);
        case Tok::String: next(); return std::make_unique<StrExpr>(p, t.text);
        case Tok::LParen: {
            next();
            parenDepth++;
            ExprPtr e = expr();
            parenDepth--;
            expect(Tok::RParen);
            return e;
        }
        case Tok::Ident: {
            next();
            if (at(Tok::LParen) && !cur().newlineBefore) return callArgs(p, t.text);
            if (isTypeName(t.text))
                fail(p.line, p.col, "a type name alone is not a value (did you mean " + t.text + "(...)?)");
            return std::make_unique<VarExpr>(p, t.text);
        }
        default: unexpected("(expected a value)");
        }
    }
};

}  // namespace

Program parse(const std::vector<Token> &tokens) {
    return Parser(tokens).program();
}
