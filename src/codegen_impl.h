#pragma once
// Internal to the code generator: the Codegen class, shared by codegen*.cpp and abi.cpp.

#include "codegen.h"
#include "error.h"
#include "index.h"

#include <llvm/IR/Constants.h>
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

#include <functional>
#include <map>
#include <set>
#include <unordered_map>

using FType = ::Type;  // Finch types (llvm::Type is the LLVM one)

struct StructInfo {
    std::string name;
    std::string module;  // "" main file, a module name, or "C" for imported C structs
    bool isC = false;
    const StructDecl *decl = nullptr;  // Finch structs
    const CStruct *cstruct = nullptr;    // C structs
    struct F {
        std::string name;
        FType type;
        const Expr *init = nullptr;  // default value
        bool hidden = false;         // C union/bitfield padding: has no name in Finch
        unsigned llvmIndex = 0;      // C structs carry explicit padding between fields
        Pos pos;                     // Finch structs: where the field is declared
    };
    std::vector<F> fields;
    llvm::StructType *llvm = nullptr;
    int state = 0;  // 0 = not resolved, 1 = resolving (cycle check), 2 = done
    bool owning = false;
    std::string unsupported;  // C structs Finch can't lay out
    Pos pos;
};

namespace finch {

struct Value_ {
    llvm::Value *v;
    FType type;
    bool literal = false;  // a number written in the code (or a C constant): adapts to the type around it
    bool fresh = false;    // an owning value nobody holds yet: it must be stored (moved) or released
};

struct Var {
    llvm::Value *slot;
    FType type;
    bool readonly = false;  // loop variables
    std::string readonlyWhy;
    bool owned = false;     // dropped when its scope ends
    int order = 0;          // declaration order, for defer
    bool moved = false;     // returned out of the function: not dropped on that path
    Pos pos;                // where it was declared
};

struct Cleanup {
    bool isDefer;
    std::string var;            // Drop: the variable
    const Stmt *body = nullptr; // Defer: the statement
    int order = 0;              // Defer: variables declared after it are not visible
};

struct Scope {
    std::unordered_map<std::string, Var> vars;
    std::vector<Cleanup> cleanups;
};

struct Fn {
    llvm::Function *llvm = nullptr;
    const FnDecl *decl = nullptr;
    std::string module;
    std::vector<bool> borrowParam;  // owning params the function never changes: no copy
    llvm::Function *cThunk = nullptr;
    FType ret;                      // resolved types
    std::vector<FType> params;
    StructInfo *recv = nullptr;     // a method of this struct: the first LLVM parameter is `self`'s address
    bool mutatesSelf = false;       // the method changes `self`
    bool fallible = false;          // -> T!: returns { i1 ok, T value, str error }
};

struct Loop {
    llvm::BasicBlock *continueTo, *breakTo;
    size_t scopeDepth;    // scopes opened inside the loop are cleaned up by break/continue
    size_t pendingDepth;  // and so are temporaries held by expressions around it (see Codegen::pending)
};

// A fresh value an expression is holding while it computes another part (f(a + b, try g())): if that
// part leaves early (try, or a return/break/continue in an `or { }` block), it must be dropped.
struct Pending {
    llvm::Value *v;
    FType type;
    bool isAddr;  // v is the value's address
};

struct Place {  // something that can be assigned to
    llvm::Value *addr;
    FType type;
};

struct LRef {
    bool isPlace;
    Place pl;
    Value_ val;
};

struct ModuleScope {
    std::map<std::string, Fn> fns;
    std::map<std::string, StructInfo *> structs;
    std::set<std::string> imports;  // Finch modules visible here
};

// How a C struct travels: System V x86-64 (Linux) or Microsoft x64 (Windows).
struct AbiArg {
    // Direct: as is. Expand: split into register-sized parts. Memory: on the stack (byval), or
    // returned through a hidden pointer (sret). Indirect (Windows): a pointer to a copy the caller made.
    enum Kind { Direct, Expand, Memory, Indirect } kind = Direct;
    std::vector<llvm::Type *> parts;  // Expand: one value per eightbyte
    llvm::Type *coerced = nullptr;    // Expand: { parts... } for returns
    bool stackAlign8 = false;         // AArch64 Linux: HFA arguments that end up on the stack
};

class Codegen {
public:
    Codegen(llvm::LLVMContext &c, const std::vector<Program> &progs, const CImports &ci, llvm::TargetMachine &tm,
            bool debug);
    std::unique_ptr<llvm::Module> run();

    // ---------- state ----------
    llvm::LLVMContext &ctx;
    llvm::IRBuilder<> b;
    std::unique_ptr<llvm::Module> mod;
    const std::vector<Program> &progs;
    const CImports &cimports;
    const llvm::DataLayout *dl;

    std::map<std::string, ModuleScope> modules;
    std::map<std::string, StructInfo *> cStructs;
    std::vector<std::unique_ptr<StructInfo>> structStore;
    std::vector<Scope> scopes;
    std::vector<Loop> loops;
    std::vector<Pending> pending;
    const Fn *curFn = nullptr;
    std::string curModule;
    int varOrder = 0;
    int deferLimit = -1;  // while emitting a defer: hide variables declared after it
    int inDefer = 0;
    std::map<std::string, llvm::Function *> helpers;  // drop/copy/print per type
    std::map<StructInfo *, std::map<std::string, Fn>> methods;
    std::set<std::string> mutatingMethods;  // names of methods that change self (for the mutation analysis)

    // errors as values: the call that `or` / `try` is handling reports here whether it failed
    const Expr *fallibleTarget = nullptr;
    llvm::Value *fallibleFailed = nullptr;  // i1
    llvm::Value *fallibleMsg = nullptr;     // str

    // debug info (-g)
    bool debug;
    std::unique_ptr<llvm::DIBuilder> di;
    std::vector<llvm::DIFile *> diFiles;
    llvm::DICompileUnit *diUnit = nullptr;
    llvm::DISubprogram *diFn = nullptr;
    std::map<std::string, llvm::DIType *> diTypes;

    // ---------- codegen.cpp: program, statements, expressions ----------
    void declareStructs();
    void declareFns();
    void declare(const Program &p, const FnDecl &fn);
    void define(Fn &fn);
    void defineMainWrapper();
    void block(const BlockStmt &blk);
    void blockBody(const BlockStmt &blk);
    void stmt(const Stmt &s);
    void varDecl(const VarDeclStmt &s);
    void assign(const AssignStmt &s);
    void ifStmt(const IfStmt &s);
    void whileStmt(const WhileStmt &s);
    void forStmt(const ForStmt &s);
    void forEachStmt(const ForEachStmt &s);
    void returnStmt(const ReturnStmt &s);
    llvm::Type *retType(const Fn &f);
    void retValue(llvm::Value *v);    // return from the current function (wrapped as success if it can fail)
    void retError(llvm::Value *msg);  // fail the current function with the str `msg`
    void jump(bool isBreak, Pos p);
    llvm::Value *condition(const Expr &e);
    void continueAt(llvm::BasicBlock *bb, llvm::BasicBlock *deadEnd);
    bool terminated();
    llvm::BasicBlock *newBlock(const char *name);

    void hold(const Value_ &v);                   // see Pending
    void holdAddr(llvm::Value *addr, const FType &t);
    void dropPending(size_t downTo);               // emit drops for pending[downTo..]
    struct Held {                                  // forgets what was held when the expression is done
        Codegen &cg;
        size_t mark;
        explicit Held(Codegen &c) : cg(c), mark(c.pending.size()) {}
        ~Held() { cg.pending.erase(cg.pending.begin() + mark, cg.pending.end()); }
    };
    void pushScope();
    void popScope();                      // emits the scope's cleanups if reachable
    void emitCleanups(size_t downTo);     // run cleanups of scopes [downTo, end) without popping
    void emitScopeCleanups(size_t idx);
    llvm::AllocaInst *slot(const FType &t, const std::string &name);
    Var *lookup(const std::string &name);
    Var &addVar(Pos p, const std::string &name, const FType &t, llvm::Value *init, bool owned, bool readonly = false,
                unsigned argNo = 0);

    Value_ expr(const Expr &e);
    Value_ exprWant(const Expr &e, const FType &want);
    Value_ binary(const BinaryExpr &e);
    Value_ unary(const UnaryExpr &e);
    Value_ logic(const BinaryExpr &e);
    Value_ arith(BinOp op, Value_ l, Value_ r, Pos p);
    Value_ arithOp(BinOp op, Value_ l, Value_ r, Pos p);
    Value_ compare(BinOp op, Value_ l, Value_ r, Pos p);
    bool unify(Value_ &l, Value_ &r);
    [[noreturn]] void badOperands(BinOp op, const Value_ &l, const Value_ &r, Pos p);
    LRef ref(const Expr &e, bool forWrite = false);  // an address if `e` has one, else a value
    Place place(const Expr &e, bool forWrite = false);
    LRef member(const MemberExpr &m, bool forWrite);
    LRef index(const IndexExpr &e, bool forWrite);
    Value_ arrayLit(const ArrayLitExpr &a, const FType *want);
    Value_ fallible(const Expr &call, llvm::Value *&failed, llvm::Value *&msg);
    Value_ tryExpr(const TryExpr &t);
    Value_ orElse(const OrElseExpr &e, bool discard = false);  // discard: a statement, the value isn't used
    FType structType(StructInfo *s);
    std::string rootVar(const Expr &e);
    bool mutates(const std::string &name, const Stmt &s);
    bool mutatesExpr(const std::string &name, const Expr &e);

    // ---------- codegen_types.cpp: types, conversions, ownership, printing ----------
    llvm::Type *ty(const FType &t);
    FType resolve(const FType &t, Pos p);
    FType resolveT(const FType &t, Pos p, bool deep);  // deep: structs by value must be complete
    void forN(llvm::Value *n, const std::function<void(llvm::Value *)> &body);  // for i in 0..n
    llvm::Value *tmpOf(llvm::Type *t);  // an uninitialised stack slot
    void resolveStruct(StructInfo *s);
    StructInfo *findStruct(const std::string &module, const std::string &name, bool qualified);
    bool owning(const FType &t);
    llvm::Constant *zero(const FType &t);
    Value_ defaultValue(const FType &t, Pos p);
    static bool widens(const FType &from, const FType &to);
    static bool fits(llvm::ConstantInt *c, const FType &from, const FType &to);
    llvm::Value *intCast(llvm::Value *v, const FType &from, const FType &to);
    llvm::Value *intToFloat(llvm::Value *v, const FType &from, const FType &to);
    Value_ coerce(Value_ v, const FType &want, Pos p, const std::string &what);
    llvm::Value *own(const Value_ &v);  // something that can be stored: moved if fresh, else copied
    void release(const Value_ &v);      // drop a fresh value nobody kept
    llvm::Value *copyValue(llvm::Value *v, const FType &t);
    void dropValue(llvm::Value *v, const FType &t);
    void dropAt(llvm::Value *addr, const FType &t);
    void copyAt(llvm::Value *dst, llvm::Value *src, const FType &t);
    llvm::Function *dropFn(const FType &t);
    llvm::Function *copyFn(const FType &t);
    std::string typeKey(const FType &t);
    llvm::Value *tmp(llvm::Value *v);  // a stack copy of v, to pass by address
    void emitPrint(llvm::Value *v, const FType &t, bool quoted);
    void emitFormat(llvm::Value *buf, llvm::Value *v, const FType &t, bool quoted);  // append v as text to the str at buf
    void bufText(llvm::Value *buf, const std::string &text);
    llvm::Function *formatFn(const FType &t);
    void printf_(const std::string &fmt, std::vector<llvm::Value *> args);
    llvm::Value *strConst(const std::string &s);
    llvm::Value *cstr(llvm::Value *strValue);  // str -> char* for C
    Value_ strFromC(llvm::Value *p);            // char* -> borrowed str
    llvm::Value *elemSize(const FType &t);

    // ---------- codegen_builtins.cpp: calls, built-ins, methods ----------
    Value_ call(const CallExpr &c);
    Value_ method(const MethodExpr &m);
    Value_ callFinch(const Fn &fn, const std::vector<ExprPtr> &args, const std::vector<std::string> &names, Pos p,
                     const std::string &shownName, const Expr *site, llvm::Value *self = nullptr,
                     const std::string &selfRoot = "");
    Value_ construct(StructInfo *s, const std::vector<ExprPtr> &args, const std::vector<std::string> &names, Pos p);
    Value_ callC(const CFunc &f, const std::vector<ExprPtr> &args, Pos p);
    Value_ convert(const FType &to, const std::vector<ExprPtr> &args, Pos p, const std::string &name, bool canFail = false);
    Value_ print(const std::vector<ExprPtr> &args, Pos p);
    Value_ addrOf(const std::vector<ExprPtr> &args, Pos p);
    Value_ arrayMethod(const MethodExpr &m, Value_ *recv);
    Value_ structMethod(const MethodExpr &m, LRef &r, const FType &t);

    // ---------- codegen_map.cpp: maps ----------
    llvm::StructType *mapEntryTy(const FType &t);
    Value_ mapLit(const MapLitExpr &e, const FType *want);
    LRef mapIndex(const IndexExpr &e, LRef &obj, const FType &t, bool forWrite);
    Value_ mapMethod(const MethodExpr &m, Value_ *recv);
    std::vector<llvm::Value *> mapArgs(llvm::Value *map, llvm::Value *keyAddr, const FType &t);
    llvm::Value *mapSlot(llvm::Value *map, const FType &t, const Value_ &key, Pos p, bool fill);
    llvm::Value *mapEntry(llvm::Value *map, const FType &t, llvm::Value *i);
    void forEachEntry(llvm::Value *map, const FType &t, const std::function<void(llvm::Value *i, llvm::Value *entry)> &body);
    llvm::Value *keyText(const Value_ &key);
    bool isBuiltin(const std::string &n);
    std::string headerHint(const std::string &name);
    void checkArgs(const std::vector<ExprPtr> &args, const std::vector<std::string> &names, size_t want, Pos p,
                   const std::string &name, bool variadic = false);
    llvm::Value *cGlobal(const std::string &name);
    llvm::Function *rt(const std::string &name);  // a runtime function by name

    // runtime errors
    llvm::Value *fileName(Pos p);
    void panicIf(llvm::Value *cond, const std::string &msg, Pos p);
    void boundsCheck(llvm::Value *i, llvm::Value *len, Pos p);
    void checkDivisor(const Value_ &l, const Value_ &r, Pos p);
    llvm::Function *libc(const char *name, llvm::Type *ret, std::vector<llvm::Type *> params, bool vararg = false);

    // ---------- abi.cpp: C calling convention for structs by value ----------
    AbiArg classify(const FType &t);
    llvm::Function *declareC(const CFunc &f);
    Value_ emitCCall(const CFunc &f, llvm::Function *fn, std::vector<Value_> args, Pos p);
    llvm::Function *cThunk(Fn &fn, Pos p);

    // ---------- language server index ----------
    void note(Pos at, size_t len, const std::string &hover, Pos def = Pos{});
    std::string signature(const Fn &fn);
    std::string structHover(StructInfo *s);

    // ---------- debug info ----------
    void setLoc(Pos p);
    llvm::DIType *diType(const FType &t);
};

}  // namespace finch
