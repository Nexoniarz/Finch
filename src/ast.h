#pragma once

#include <memory>
#include <string>
#include <vector>

struct StructInfo;  // filled in by codegen: fields, LLVM type, ownership

struct Type {
    enum Kind {
        Void, Bool, Char, I8, I16, I32, I64, U8, U16, U32, U64, F32, F64,
        Str, Ptr, Null,
        Array,   // []T: growable, owns its elements
        Fixed,   // [N]T: only inside C structs
        Struct,  // a resolved struct
        Named,   // a struct name as written, before codegen resolves it
    };
    Kind kind = Void;
    std::shared_ptr<Type> elem;  // Ptr (null = untyped `ptr`), Array, Fixed
    long long count = 0;         // Fixed
    std::string name;            // Named / Struct
    std::string module;          // Named: written qualifier ("" = none); Struct: defining module
    StructInfo *info = nullptr;  // Struct

    Type() = default;
    Type(Kind k) : kind(k) {}
    static Type ptrTo(const Type &t) {
        Type p(Ptr);
        p.elem = std::make_shared<Type>(t);
        return p;
    }
    static Type arrayOf(const Type &t) {
        Type a(Array);
        a.elem = std::make_shared<Type>(t);
        return a;
    }

    bool isInt() const { return kind >= I8 && kind <= U64; }
    bool isSigned() const { return kind >= I8 && kind <= I64; }
    bool isUnsigned() const { return kind >= U8 && kind <= U64; }
    bool isFloat() const { return kind == F32 || kind == F64; }
    bool isNumber() const { return isInt() || isFloat(); }
    bool isPtr() const { return kind == Ptr; }
    int bits() const {
        switch (kind) {
        case Bool: return 1;
        case Char: case I8: case U8: return 8;
        case I16: case U16: return 16;
        case I32: case U32: case F32: return 32;
        default: return 64;
        }
    }

    bool operator==(const Type &o) const {
        if (kind != o.kind) return false;
        switch (kind) {
        case Ptr:
            if (!elem || !o.elem) return !elem && !o.elem;
            return *elem == *o.elem;
        case Array: return *elem == *o.elem;
        case Fixed: return count == o.count && *elem == *o.elem;
        case Struct: return info == o.info;
        case Named: return name == o.name && module == o.module;
        default: return true;
        }
    }
    bool operator!=(const Type &o) const { return !(*this == o); }

    // How the type is written in Finch, for error messages.
    std::string show() const {
        switch (kind) {
        case Void: return "nothing";
        case Bool: return "bool";
        case Char: return "char";
        case I8: return "i8";
        case I16: return "i16";
        case I32: return "i32";
        case I64: return "int";
        case U8: return "u8";
        case U16: return "u16";
        case U32: return "u32";
        case U64: return "u64";
        case F32: return "f32";
        case F64: return "float";
        case Str: return "str";
        case Ptr: return elem ? "ptr[" + elem->show() + "]" : "ptr";
        case Null: return "null";
        case Array: return "[]" + elem->show();
        case Fixed: return "[" + std::to_string(count) + "]" + elem->show();
        case Struct:
        case Named: return module.empty() ? name : module + "." + name;
        }
        return "?";
    }
};

// Built-in type names. `int` is i64 and `float` is f64: the everyday defaults.
bool typeFromName(const std::string &name, Type &out);

struct Pos {
    int line = 0, col = 0;
    int file = 0;  // index into the list of source files (see error.h)
};

// ---------- expressions ----------

enum class ExprKind { Int, Float, Bool, Char, Str, Null, Var, Unary, Binary, Call, Member, Index, ArrayLit, Method };

struct Expr {
    ExprKind kind;
    Pos pos;
    Expr(ExprKind k, Pos p) : kind(k), pos(p) {}
    virtual ~Expr() = default;
};
using ExprPtr = std::unique_ptr<Expr>;

struct IntExpr : Expr {
    long long value;
    IntExpr(Pos p, long long v) : Expr(ExprKind::Int, p), value(v) {}
};
struct FloatExpr : Expr {
    double value;
    FloatExpr(Pos p, double v) : Expr(ExprKind::Float, p), value(v) {}
};
struct BoolExpr : Expr {
    bool value;
    BoolExpr(Pos p, bool v) : Expr(ExprKind::Bool, p), value(v) {}
};
struct CharExpr : Expr {
    char value;
    CharExpr(Pos p, char v) : Expr(ExprKind::Char, p), value(v) {}
};
struct StrExpr : Expr {
    std::string value;
    StrExpr(Pos p, std::string v) : Expr(ExprKind::Str, p), value(std::move(v)) {}
};
struct NullExpr : Expr {
    explicit NullExpr(Pos p) : Expr(ExprKind::Null, p) {}
};
struct VarExpr : Expr {
    std::string name;
    VarExpr(Pos p, std::string n) : Expr(ExprKind::Var, p), name(std::move(n)) {}
};
struct UnaryExpr : Expr {
    char op;  // '-', '!' or '~'
    ExprPtr operand;
    UnaryExpr(Pos p, char o, ExprPtr e) : Expr(ExprKind::Unary, p), op(o), operand(std::move(e)) {}
};
enum class BinOp {
    Add, Sub, Mul, Div, Mod,
    BitAnd, BitOr, BitXor, Shl, Shr,
    Eq, NotEq, Less, LessEq, Greater, GreaterEq,
    And, Or,
};
struct BinaryExpr : Expr {
    BinOp op;
    ExprPtr lhs, rhs;
    BinaryExpr(Pos p, BinOp o, ExprPtr l, ExprPtr r)
        : Expr(ExprKind::Binary, p), op(o), lhs(std::move(l)), rhs(std::move(r)) {}
};
// name(args): a function, a built-in, a conversion or a struct constructor.
struct CallExpr : Expr {
    std::string callee;
    std::vector<ExprPtr> args;
    std::vector<std::string> argNames;  // "" for positional; Point(x: 1, y: 2)
    CallExpr(Pos p, std::string c) : Expr(ExprKind::Call, p), callee(std::move(c)) {}
};
struct MemberExpr : Expr {  // obj.field, p.value, a.len
    ExprPtr obj;
    std::string field;
    MemberExpr(Pos p, ExprPtr o, std::string f)
        : Expr(ExprKind::Member, p), obj(std::move(o)), field(std::move(f)) {}
};
struct IndexExpr : Expr {  // a[i]
    ExprPtr obj, index;
    IndexExpr(Pos p, ExprPtr o, ExprPtr i) : Expr(ExprKind::Index, p), obj(std::move(o)), index(std::move(i)) {}
};
struct ArrayLitExpr : Expr {  // [1, 2, 3]
    std::vector<ExprPtr> elems;
    explicit ArrayLitExpr(Pos p) : Expr(ExprKind::ArrayLit, p) {}
};
// obj.name(args): a.push(x), s.find("x"), and math.sqrt(2) / math.Vec(1, 2) for modules
struct MethodExpr : Expr {
    ExprPtr obj;
    std::string name;
    std::vector<ExprPtr> args;
    std::vector<std::string> argNames;
    MethodExpr(Pos p, ExprPtr o, std::string n) : Expr(ExprKind::Method, p), obj(std::move(o)), name(std::move(n)) {}
};

// ---------- statements ----------

enum class StmtKind { VarDecl, Assign, Expr, If, While, For, ForEach, Return, Break, Continue, Block, Defer };

struct Stmt {
    StmtKind kind;
    Pos pos;
    Stmt(StmtKind k, Pos p) : kind(k), pos(p) {}
    virtual ~Stmt() = default;
};
using StmtPtr = std::unique_ptr<Stmt>;

struct BlockStmt : Stmt {
    std::vector<StmtPtr> body;
    explicit BlockStmt(Pos p) : Stmt(StmtKind::Block, p) {}
};
using BlockPtr = std::unique_ptr<BlockStmt>;

struct VarDeclStmt : Stmt {
    bool hasType;  // `int x = 5` vs `x := 5`
    Type type;
    std::string name;
    ExprPtr init;  // may be null only when hasType
    VarDeclStmt(Pos p) : Stmt(StmtKind::VarDecl, p) {}
};
struct AssignStmt : Stmt {
    ExprPtr target;  // a variable, field, element or p.value
    char op;         // '=', '+', '-', '*', '/', '%'
    ExprPtr value;
    AssignStmt(Pos p) : Stmt(StmtKind::Assign, p) {}
};
struct ExprStmt : Stmt {
    ExprPtr expr;
    ExprStmt(Pos p, ExprPtr e) : Stmt(StmtKind::Expr, p), expr(std::move(e)) {}
};
struct IfStmt : Stmt {
    ExprPtr cond;
    BlockPtr then;
    StmtPtr otherwise;  // null, BlockStmt, or IfStmt (else if)
    IfStmt(Pos p) : Stmt(StmtKind::If, p) {}
};
struct WhileStmt : Stmt {
    ExprPtr cond;
    BlockPtr body;
    WhileStmt(Pos p) : Stmt(StmtKind::While, p) {}
};
struct ForStmt : Stmt {
    std::string var;
    ExprPtr from, to;  // for var in from..to  (to is exclusive)
    BlockPtr body;
    ForStmt(Pos p) : Stmt(StmtKind::For, p) {}
};
struct ForEachStmt : Stmt {
    std::string var;
    ExprPtr list;  // for var in list  (an array or a str)
    BlockPtr body;
    ForEachStmt(Pos p) : Stmt(StmtKind::ForEach, p) {}
};
struct ReturnStmt : Stmt {
    ExprPtr value;  // may be null
    ReturnStmt(Pos p) : Stmt(StmtKind::Return, p) {}
};
struct BreakStmt : Stmt {
    explicit BreakStmt(Pos p) : Stmt(StmtKind::Break, p) {}
};
struct ContinueStmt : Stmt {
    explicit ContinueStmt(Pos p) : Stmt(StmtKind::Continue, p) {}
};
struct DeferStmt : Stmt {  // defer <statement>: runs when the block ends
    StmtPtr body;
    DeferStmt(Pos p, StmtPtr s) : Stmt(StmtKind::Defer, p), body(std::move(s)) {}
};

// ---------- top level ----------

struct Param {
    Type type;
    std::string name;
    Pos pos;
};

struct FnDecl {
    Pos pos;
    std::string name;
    Type ret;  // Void when there is no `-> type`
    std::vector<Param> params;
    BlockPtr body;
};

struct Field {
    Type type;
    std::string name;
    ExprPtr init;  // default value, may be null
    Pos pos;
};

struct StructDecl {
    Pos pos;
    std::string name;
    std::vector<Field> fields;
};

struct Import {
    Pos pos;
    std::string path;  // "stdio.h" (C header, isC) or "math" (Finch module)
    bool isC = true;
};

struct Link {
    Pos pos;
    std::string lib;  // "glfw" -> libglfw.so; "helpers.c" / ".o" / ".a" -> a file next to the .fch file
};

// One .fch file.
struct Program {
    std::string module;  // "" for the main file, else the module's name
    std::string path;
    std::vector<Import> imports;
    std::vector<Link> links;
    std::vector<StructDecl> structs;
    std::vector<FnDecl> fns;
};
