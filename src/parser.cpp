#include "parser.h"
#include "error.h"

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstring>
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
    Parser(const std::vector<Token> &t, int f) : toks(t), file(f) {}

    Program program() {
        Program p;
        while (!at(Tok::End)) {
            switch (cur().kind) {
            case Tok::Import: p.imports.push_back(import()); break;
            case Tok::Link: p.links.push_back(link()); break;
            case Tok::Struct: p.structs.push_back(structDecl()); break;
            default: p.fns.push_back(function()); break;
            }
        }
        return p;
    }

private:
    const std::vector<Token> &toks;
    int file;
    size_t i = 0;
    int parenDepth = 0;  // inside ( ) and [ ] newlines don't end anything

    const Token &cur() const { return toks[i]; }
    const Token &peekTok(size_t off = 1) const { return toks[std::min(i + off, toks.size() - 1)]; }
    const Token &next() { return toks[i++]; }
    bool at(Tok k) const { return cur().kind == k; }
    Pos pos() const { return {cur().line, cur().col, file}; }

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

    // A token can continue an expression only if it is on the same line (or we're inside brackets).
    bool sameLine() const { return parenDepth > 0 || !cur().newlineBefore; }

    // int, u8, ptr, ptr[int], []str, map[str]int, Point, math.Vec ...
    Type type() {
        if (accept(Tok::LBracket)) {
            expect(Tok::RBracket, "']' (an array type looks like []int)");
            return Type::arrayOf(type());
        }
        if (!at(Tok::Ident)) unexpected("(expected a type like int, str, []int, ptr[int] or a struct name)");
        if (cur().text == "map" && peekTok().kind == Tok::LBracket) {
            next();
            next();
            Type k = type();
            expect(Tok::RBracket, "']' (a map type looks like map[str]int)");
            return Type::mapOf(k, type());
        }
        std::string name = next().text;
        Type t;
        if (typeFromName(name, t)) {
            if (t.kind == Type::Ptr && accept(Tok::LBracket)) {
                t = Type::ptrTo(type());
                expect(Tok::RBracket);
            }
            return t;
        }
        t = Type(Type::Named);
        t.name = name;
        if (at(Tok::Dot) && peekTok().kind == Tok::Ident) {  // module.Struct
            next();
            t.module = name;
            t.name = next().text;
        }
        return t;
    }

    // Does a declaration like `int x`, `ptr[int] p`, `[]int a`, `Point p` or `math.Vec v` start here?
    bool atDeclaration() const {
        if (at(Tok::LBracket)) return peekTok().kind == Tok::RBracket;
        if (!at(Tok::Ident)) return false;
        Tok n = peekTok().kind;
        if (n == Tok::Ident) return !peekTok().newlineBefore;
        if ((cur().text == "ptr" || cur().text == "map") && n == Tok::LBracket) return true;
        return n == Tok::Dot && peekTok(2).kind == Tok::Ident && peekTok(3).kind == Tok::Ident &&
               !peekTok(3).newlineBefore;
    }

    // ---------- top level ----------

    Import import() {
        Import im;
        im.pos = pos();
        expect(Tok::Import);
        if (at(Tok::Ident)) {
            im.isC = false;
            im.path = next().text;
        } else {
            im.path = expect(Tok::String, "a module name (import math) or a C header in quotes (import \"stdio.h\")").text;
        }
        return im;
    }

    Link link() {
        Link l;
        l.pos = pos();
        expect(Tok::Link);
        l.lib = expect(Tok::String, "a library name in quotes, like link \"glfw\"").text;
        auto ends = [&](const char *x) { size_t n = std::strlen(x); return l.lib.size() > n && l.lib.compare(l.lib.size() - n, n, x) == 0; };
        if (ends(".c") || ends(".o") || ends(".a")) return l;  // your own C code, next to the .fch file
        if (l.lib.rfind("lib", 0) == 0 || l.lib.find(".so") != std::string::npos || l.lib.find('/') != std::string::npos)
            fail(l.pos.line, l.pos.col, "write just the library's name: for libglfw.so that's  link \"glfw\"");
        return l;
    }

    StructDecl structDecl() {
        StructDecl s;
        s.pos = pos();
        expect(Tok::Struct);
        s.namePos = pos();
        s.name = expect(Tok::Ident, "a struct name").text;
        if (isTypeName(s.name)) fail(s.pos.line, s.pos.col, "'" + s.name + "' is a built-in type name, pick another");
        Pos open = pos();
        expect(Tok::LBrace);
        while (!at(Tok::RBrace)) {
            if (at(Tok::End)) fail(open.line, open.col, "this '{' is never closed (missing '}')");
            Field f;
            f.pos = pos();
            if (!atDeclaration()) unexpected("(a field looks like: int x  or  str name = \"default\")");
            f.type = type();
            f.namePos = pos();
            f.name = expect(Tok::Ident, "a field name").text;
            for (const Field &other : s.fields)
                if (other.name == f.name) fail(f.pos.line, f.pos.col, "the field '" + f.name + "' is listed twice");
            if (accept(Tok::Assign)) f.init = expr();
            s.fields.push_back(std::move(f));
            endOfStatement();
        }
        next();
        return s;
    }

    FnDecl function() {
        FnDecl fn;
        fn.pos = pos();
        if (at(Tok::Ident) && isTypeName(cur().text))
            fail(fn.pos.line, fn.pos.col, "functions start with 'fn', like: fn add(int a, int b) -> int { ... }");
        expect(Tok::Fn, "'fn', 'struct', 'import' or 'link' (every function starts with fn)");

        fn.namePos = pos();
        fn.name = expect(Tok::Ident, "a function name").text;
        if (accept(Tok::Dot)) {  // fn Point.move(...): a method
            fn.recv = fn.name;
            fn.recvPos = fn.namePos;
            fn.namePos = pos();
            fn.name = expect(Tok::Ident, "a method name (fn Point.name(...))").text;
        }
        expect(Tok::LParen);
        parenDepth++;
        if (!at(Tok::RParen)) {
            do {
                Param p;
                p.pos = pos();
                p.type = type();
                p.namePos = pos();
                p.name = expect(Tok::Ident, "a parameter name").text;
                fn.params.push_back(p);
            } while (accept(Tok::Comma));
        }
        parenDepth--;
        expect(Tok::RParen);
        fn.ret = Type(Type::Void);
        if (accept(Tok::Arrow)) {
            if (!at(Tok::Not)) fn.ret = type();
            fn.fallible = accept(Tok::Not);  // -> int!  or  -> !
            if (!fn.fallible && fn.ret.kind == Type::Void) unexpected("(after '->' comes a type, or ! for a function that can fail)");
        }
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
        b->end = pos();
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
            Pos varPos = pos();
            std::string var = expect(Tok::Ident, "a loop variable name").text;
            std::string var2;
            Pos var2Pos;
            if (accept(Tok::Comma)) {  // for i, x in list  /  for key, value in map
                var2Pos = pos();
                var2 = expect(Tok::Ident, "a second loop variable name").text;
            }
            expect(Tok::In);
            ExprPtr first = expr();
            if (!var2.empty() && at(Tok::DotDot)) fail(var2Pos.line, var2Pos.col, "a range (for i in 0..n) has one loop variable");
            if (accept(Tok::DotDot)) {  // for i in 0..10
                auto s = std::make_unique<ForStmt>(p);
                s->var = var;
                s->varPos = varPos;
                s->from = std::move(first);
                s->to = expr();
                s->body = block();
                return s;
            }
            auto s = std::make_unique<ForEachStmt>(p);  // for x in list
            s->var = var;
            s->varPos = varPos;
            s->var2 = var2;
            s->var2Pos = var2Pos;
            s->list = std::move(first);
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
        case Tok::Defer: {
            next();
            if (at(Tok::Defer)) fail(p.line, p.col, "defer inside defer is not allowed");
            return std::make_unique<DeferStmt>(p, statement());
        }
        case Tok::LBrace: return block();
        case Tok::Fn: fail(p.line, p.col, "functions can't be inside other functions (move it out)");
        case Tok::Struct: fail(p.line, p.col, "structs go outside functions, at the top level of the file");
        case Tok::Import:
        case Tok::Link: fail(p.line, p.col, "import and link go at the top of the file, outside functions");
        default: break;
        }

        // int x = 5, Point p, []int a
        if (atDeclaration()) {
            auto s = std::make_unique<VarDeclStmt>(p);
            s->hasType = true;
            s->type = type();
            s->namePos = pos();
            s->name = expect(Tok::Ident, "a variable name").text;
            if (accept(Tok::Assign)) s->init = expr();
            return s;
        }

        // x := 5
        if (at(Tok::Ident) && peekTok().kind == Tok::Declare) {
            auto s = std::make_unique<VarDeclStmt>(p);
            s->hasType = false;
            s->namePos = pos();
            s->name = next().text;
            next();
            s->init = expr();
            return s;
        }

        ExprPtr e = expr();

        // x = 5, p.value += 1, a[i] = 2, ...
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
            if (e->kind != ExprKind::Var && e->kind != ExprKind::Member && e->kind != ExprKind::Index)
                fail(p.line, p.col, "only a variable, a field, an element or p.value can be assigned to");
            auto s = std::make_unique<AssignStmt>(p);
            s->target = std::move(e);
            s->op = op;
            next();
            s->value = expr();
            return s;
        }

        if (e->kind != ExprKind::Call && e->kind != ExprKind::Method && e->kind != ExprKind::Try && e->kind != ExprKind::OrElse)
            fail(p.line, p.col, "this value is computed but never used");
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

    // f(x) or fallback  /  f(x) or { ... }: the lowest precedence, right to left
    ExprPtr expr() {
        ExprPtr l = orExpr();
        if (!sameLine() || !at(Tok::OrElse)) return l;
        Pos p = pos();
        next();
        auto e = std::make_unique<OrElseExpr>(p, std::move(l));
        if (at(Tok::LBrace)) e->block = block();
        else e->fallback = expr();
        return e;
    }

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
        if (accept(Tok::Try)) return std::make_unique<TryExpr>(p, postfix());
        return postfix();
    }

    // p.value, a.len, a[i], a.push(x), math.sqrt(2)
    ExprPtr postfix() {
        ExprPtr e = primary();
        while (sameLine()) {
            Pos p = pos();
            if (accept(Tok::Dot)) {
                std::string name = expect(Tok::Ident, "a name after '.'").text;
                if (at(Tok::LParen) && !cur().newlineBefore) {
                    auto m = std::make_unique<MethodExpr>(p, std::move(e), name);
                    args(m->args, m->argNames);
                    e = std::move(m);
                } else {
                    e = std::make_unique<MemberExpr>(p, std::move(e), name);
                }
            } else if (at(Tok::LBracket)) {
                next();
                parenDepth++;
                ExprPtr index = expr();
                parenDepth--;
                expect(Tok::RBracket);
                e = std::make_unique<IndexExpr>(p, std::move(e), std::move(index));
            } else {
                return e;
            }
        }
        return e;
    }

    // ( arg, name: arg, ... ) -- we are at the '('
    void args(std::vector<ExprPtr> &out, std::vector<std::string> &names) {
        expect(Tok::LParen);
        parenDepth++;
        if (!at(Tok::RParen)) {
            do {
                if (at(Tok::RParen)) break;  // a trailing comma is fine
                std::string name;
                if (at(Tok::Ident) && peekTok().kind == Tok::Colon) {
                    name = next().text;
                    next();
                }
                names.push_back(name);
                out.push_back(expr());
            } while (accept(Tok::Comma));
        }
        parenDepth--;
        expect(Tok::RParen);
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
        case Tok::LBracket: {  // [1, 2, 3]  or a map: ["a": 1, "b": 2], [:]
            next();
            parenDepth++;
            if (accept(Tok::Colon)) {
                parenDepth--;
                expect(Tok::RBracket, "']' (an empty map is [:])");
                return std::make_unique<MapLitExpr>(p);
            }
            if (!at(Tok::RBracket)) {
                ExprPtr first = expr();
                if (accept(Tok::Colon)) {
                    auto m = std::make_unique<MapLitExpr>(p);
                    m->keys.push_back(std::move(first));
                    m->values.push_back(expr());
                    while (accept(Tok::Comma) && !at(Tok::RBracket)) {  // a trailing comma is fine
                        m->keys.push_back(expr());
                        expect(Tok::Colon, "':' (every map entry is key: value)");
                        m->values.push_back(expr());
                    }
                    parenDepth--;
                    expect(Tok::RBracket);
                    return m;
                }
                auto a = std::make_unique<ArrayLitExpr>(p);
                a->elems.push_back(std::move(first));
                while (accept(Tok::Comma) && !at(Tok::RBracket)) a->elems.push_back(expr());
                parenDepth--;
                expect(Tok::RBracket);
                return a;
            }
            auto a = std::make_unique<ArrayLitExpr>(p);
            parenDepth--;
            expect(Tok::RBracket);
            return a;
        }
        case Tok::Ident: {
            next();
            if (at(Tok::LParen) && !cur().newlineBefore) {
                auto call = std::make_unique<CallExpr>(p, t.text);
                args(call->args, call->argNames);
                return call;
            }
            if (isTypeName(t.text))
                fail(p.line, p.col, "a type name alone is not a value (did you mean " + t.text + "(...)?)");
            return std::make_unique<VarExpr>(p, t.text);
        }
        default: unexpected("(expected a value)");
        }
    }
};

}  // namespace

Program parse(const std::vector<Token> &tokens, int file) {
    return Parser(tokens, file).program();
}
