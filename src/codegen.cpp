#include "codegen.h"
#include "error.h"

#include <llvm/IR/Constants.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

#include <unordered_map>

using namespace llvm;
using FType = ::Type;  // Finch types (llvm::Type is the LLVM one)

namespace {

struct Value_ {
    Value *v;
    FType type;
    bool literal = false;  // a number written in the code (or a C constant): adapts to the type around it
};

struct Var {
    AllocaInst *slot;
    FType type;
    bool readonly;  // loop variables
};

struct Fn {
    Function *llvm;
    const FnDecl *decl;
};

struct Loop {
    BasicBlock *continueTo, *breakTo;
};

struct Place {  // something that can be assigned to: a variable or p.value
    Value *addr;
    FType type;
};

class Codegen {
public:
    Codegen(LLVMContext &c, const CImports &ci, TargetMachine &tm)
        : ctx(c), b(c), mod(std::make_unique<Module>("finch", c)), cimports(ci) {
        mod->setTargetTriple(tm.getTargetTriple());
        mod->setDataLayout(tm.createDataLayout());
    }

    std::unique_ptr<Module> run(const Program &prog) {
        // Declare every function first, so order in the file doesn't matter.
        for (const FnDecl &fn : prog.fns) declare(fn);
        if (!fns.count("main")) fail(1, 1, "there is no main function (add 'fn main() { ... }')");
        for (const FnDecl &fn : prog.fns) define(fn);

        std::string err;
        raw_string_ostream os(err);
        if (verifyModule(*mod, &os)) {
            std::fprintf(stderr, "internal compiler error (please report):\n%s\n", err.c_str());
            std::exit(2);
        }
        return std::move(mod);
    }

private:
    LLVMContext &ctx;
    IRBuilder<> b;
    std::unique_ptr<Module> mod;
    const CImports &cimports;

    std::unordered_map<std::string, Fn> fns;
    std::vector<std::unordered_map<std::string, Var>> scopes;
    std::vector<Loop> loops;
    const FnDecl *curFn = nullptr;

    // ---------- types ----------

    llvm::Type *ty(const FType &t) {
        switch (t.kind) {
        case FType::Void: return b.getVoidTy();
        case FType::Bool: return b.getInt1Ty();
        case FType::Char:
        case FType::I8:
        case FType::U8: return b.getInt8Ty();
        case FType::I16:
        case FType::U16: return b.getInt16Ty();
        case FType::I32:
        case FType::U32: return b.getInt32Ty();
        case FType::I64:
        case FType::U64: return b.getInt64Ty();
        case FType::F32: return b.getFloatTy();
        case FType::F64: return b.getDoubleTy();
        case FType::Str:
        case FType::Ptr:
        case FType::Null: return b.getPtrTy();
        }
        return nullptr;
    }

    Constant *zero(const FType &t) {
        if (t.kind == FType::Str) return cast<Constant>(b.CreateGlobalString("", "empty"));
        return Constant::getNullValue(ty(t));
    }

    // Can every value of `from` be stored in `to`? (i8 -> i32 yes, i32 -> u32 no)
    static bool widens(const FType &from, const FType &to) {
        if (!from.isInt() || !to.isInt()) return false;
        if (from.isSigned() && to.isUnsigned()) return false;
        if (from.isSigned() == to.isSigned()) return to.bits() > from.bits();
        return to.bits() > from.bits();  // unsigned -> bigger signed
    }

    // Does the constant number `c` (of type `from`) fit into `to`?
    static bool fits(ConstantInt *c, const FType &from, const FType &to) {
        if (!to.isInt()) return false;
        bool negative = from.isSigned() && c->isNegative();
        if (negative && to.isUnsigned()) return false;
        if (negative) return c->getSExtValue() >= -(1LL << (to.bits() - 1));
        unsigned long long v = c->getZExtValue();
        if (to.bits() == 64) return to.isUnsigned() || v <= (unsigned long long)INT64_MAX;
        unsigned long long max = to.isUnsigned() ? (1ULL << to.bits()) - 1 : (1ULL << (to.bits() - 1)) - 1;
        return v <= max;
    }

    Value *intCast(Value *v, const FType &from, const FType &to) {
        return b.CreateIntCast(v, ty(to), from.isSigned());
    }

    Value *intToFloat(Value *v, const FType &from, const FType &to) {
        return from.isSigned() ? b.CreateSIToFP(v, ty(to)) : b.CreateUIToFP(v, ty(to));
    }

    // Implicit conversions: only the ones that can't surprise you.
    Value *coerce(Value_ v, const FType &want, Pos p, const std::string &what) {
        const FType &from = v.type;
        if (from == want) return v.v;
        if (from.kind == FType::Null && (want.isPtr() || want.kind == FType::Str))
            return ConstantPointerNull::get(b.getPtrTy());
        // any pointer (or str) fits an untyped `ptr`; an untyped ptr fits any pointer
        if (want.isPtr() && !want.elem && (from.isPtr() || from.kind == FType::Str)) return v.v;
        if (from.isPtr() && !from.elem && want.isPtr()) return v.v;

        if (from.isInt() && want.isInt()) {
            auto *c = v.literal ? dyn_cast<ConstantInt>(v.v) : nullptr;
            if ((c && fits(c, from, want)) || widens(from, want)) return intCast(v.v, from, want);
            if (c) fail(p.line, p.col, "the number " + std::to_string(from.isSigned() ? c->getSExtValue() : (long long)c->getZExtValue()) +
                                           " doesn't fit in " + want.name());
        }
        if (from.isInt() && want.isFloat()) return intToFloat(v.v, from, want);
        if (from.kind == FType::F32 && want.kind == FType::F64) return b.CreateFPExt(v.v, ty(want));
        if (from.kind == FType::F64 && want.kind == FType::F32 && v.literal)
            return b.CreateFPTrunc(v.v, ty(want));

        std::string msg = what + " must be " + want.name() + ", but this is " + from.name();
        if (from.isNumber() && want.isNumber())
            msg += " (use " + want.name() + "(...) to convert" + (from.isFloat() && want.isInt() ? ", it cuts off the fraction)" : ")");
        if (from.kind == FType::Null) msg = what + " can't be null (only pointers can)";
        fail(p.line, p.col, msg);
    }

    // ---------- helpers ----------

    bool terminated() { return b.GetInsertBlock()->getTerminator() != nullptr; }

    BasicBlock *newBlock(const char *name) {
        return BasicBlock::Create(ctx, name, b.GetInsertBlock()->getParent());
    }

    // Variables live in stack slots created at the top of the function; LLVM turns them into registers.
    AllocaInst *slot(const FType &t, const std::string &name) {
        Function *f = b.GetInsertBlock()->getParent();
        IRBuilder<> entry(&f->getEntryBlock(), f->getEntryBlock().begin());
        return entry.CreateAlloca(ty(t), nullptr, name);
    }

    Var *lookup(const std::string &name) {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) return &found->second;
        }
        return nullptr;
    }

    void addVar(Pos p, const std::string &name, const FType &t, Value *init, bool readonly = false) {
        if (lookup(name)) fail(p.line, p.col, "a variable named '" + name + "' already exists here");
        if (fns.count(name)) fail(p.line, p.col, "'" + name + "' is already the name of a function");
        if (isBuiltin(name)) fail(p.line, p.col, "'" + name + "' is a built-in name, pick another one");
        AllocaInst *s = slot(t, name);
        b.CreateStore(init, s);
        scopes.back()[name] = {s, t, readonly};
    }

    Function *libc(const char *name, llvm::Type *ret, std::vector<llvm::Type *> params, bool vararg = false) {
        return cast<Function>(mod->getOrInsertFunction(name, FunctionType::get(ret, params, vararg)).getCallee());
    }

    // ---------- functions ----------

    void declare(const FnDecl &fn) {
        if (fns.count(fn.name)) fail(fn.pos.line, fn.pos.col, "function '" + fn.name + "' is defined twice");
        if (isBuiltin(fn.name)) fail(fn.pos.line, fn.pos.col, "'" + fn.name + "' is a built-in function, pick another name");

        bool isMain = fn.name == "main";
        if (isMain && !fn.params.empty()) fail(fn.pos.line, fn.pos.col, "main takes no parameters");
        if (isMain && fn.ret.kind != FType::Void && fn.ret.kind != FType::I64 && fn.ret.kind != FType::I32)
            fail(fn.pos.line, fn.pos.col, "main returns nothing (fn main()) or an exit code (fn main() -> int)");

        std::vector<llvm::Type *> params;
        for (const Param &p : fn.params) params.push_back(ty(p.type));
        llvm::Type *ret = isMain ? b.getInt32Ty() : ty(fn.ret);

        // User functions get a prefix so they never clash with C library names.
        Function *f = Function::Create(FunctionType::get(ret, params, false),
                                       isMain ? Function::ExternalLinkage : Function::InternalLinkage,
                                       isMain ? "main" : "finch." + fn.name, mod.get());
        fns[fn.name] = {f, &fn};
    }

    void define(const FnDecl &fn) {
        Function *f = fns[fn.name].llvm;
        curFn = &fn;
        b.SetInsertPoint(BasicBlock::Create(ctx, "entry", f));

        scopes.push_back({});
        unsigned i = 0;
        for (Argument &arg : f->args()) {
            const Param &p = fn.params[i++];
            arg.setName(p.name);
            addVar(p.pos, p.name, p.type, &arg);
        }
        blockBody(*fn.body);
        scopes.pop_back();

        if (!terminated()) {
            if (fn.name == "main") b.CreateRet(b.getInt32(0));
            else if (fn.ret.kind == FType::Void) b.CreateRetVoid();
            else fail(fn.pos.line, fn.pos.col, "function '" + fn.name + "' can reach its end without returning " + fn.ret.name());
        }
    }

    // ---------- statements ----------

    void block(const BlockStmt &blk) {
        scopes.push_back({});
        blockBody(blk);
        scopes.pop_back();
    }

    void blockBody(const BlockStmt &blk) {
        for (const StmtPtr &s : blk.body) {
            if (terminated()) fail(s->pos.line, s->pos.col, "this code can never run (it comes after return, break or continue)");
            stmt(*s);
        }
    }

    void stmt(const Stmt &s) {
        switch (s.kind) {
        case StmtKind::Block: block(static_cast<const BlockStmt &>(s)); break;
        case StmtKind::VarDecl: varDecl(static_cast<const VarDeclStmt &>(s)); break;
        case StmtKind::Assign: assign(static_cast<const AssignStmt &>(s)); break;
        case StmtKind::Expr: expr(*static_cast<const ExprStmt &>(s).expr); break;
        case StmtKind::If: ifStmt(static_cast<const IfStmt &>(s)); break;
        case StmtKind::While: whileStmt(static_cast<const WhileStmt &>(s)); break;
        case StmtKind::For: forStmt(static_cast<const ForStmt &>(s)); break;
        case StmtKind::Return: returnStmt(static_cast<const ReturnStmt &>(s)); break;
        case StmtKind::Break:
        case StmtKind::Continue: {
            bool isBreak = s.kind == StmtKind::Break;
            if (loops.empty()) fail(s.pos.line, s.pos.col, std::string(isBreak ? "break" : "continue") + " can only be used inside a loop");
            b.CreateBr(isBreak ? loops.back().breakTo : loops.back().continueTo);
            break;
        }
        }
    }

    void varDecl(const VarDeclStmt &s) {
        if (!s.hasType) {
            Value_ v = expr(*s.init);
            if (v.type.kind == FType::Void) fail(s.init->pos.line, s.init->pos.col, "this gives back nothing, so it can't be stored");
            if (v.type.kind == FType::Null) fail(s.init->pos.line, s.init->pos.col, "null needs a type, like: ptr[int] " + s.name + " = null");
            addVar(s.pos, s.name, v.type, v.v);
            return;
        }
        Value *init = s.init ? coerce(expr(*s.init), s.type, s.init->pos, "'" + s.name + "'") : zero(s.type);
        addVar(s.pos, s.name, s.type, init);
    }

    // Where does `x` or `p.value` live in memory?
    Place place(const Expr &e) {
        if (e.kind == ExprKind::Var) {
            auto &v = static_cast<const VarExpr &>(e);
            Var *var = lookup(v.name);
            if (!var) {
                if (cimports.globals.count(v.name)) return {cGlobal(v.name), cimports.globals.at(v.name).type};
                fail(e.pos.line, e.pos.col, "there is no variable named '" + v.name + "'");
            }
            return {var->slot, var->type};
        }
        if (e.kind == ExprKind::Member) {
            auto &m = static_cast<const MemberExpr &>(e);
            Value_ obj = expr(*m.obj);
            if (!obj.type.isPtr())
                fail(m.pos.line, m.pos.col, obj.type.name() + " has no '." + m.field + "'");
            if (m.field != "value")
                fail(m.pos.line, m.pos.col, "a pointer only has '.value' (the thing it points to)");
            if (!obj.type.elem)
                fail(m.pos.line, m.pos.col, "this ptr has no type, so Finch doesn't know what's inside (use ptr[int] etc.)");
            // reading or writing through null is undefined in C; in Finch it stops the program
            if (!isa<AllocaInst>(obj.v) && !isa<GlobalValue>(obj.v))
                panicIf(b.CreateIsNull(obj.v), "used .value on a null pointer", m.pos);
            return {obj.v, *obj.type.elem};
        }
        fail(e.pos.line, e.pos.col, "this can't be assigned to");
    }

    void assign(const AssignStmt &s) {
        if (s.target->kind == ExprKind::Var) {
            auto &name = static_cast<const VarExpr &>(*s.target).name;
            Var *var = lookup(name);
            if (!var && !cimports.globals.count(name))
                fail(s.pos.line, s.pos.col, "there is no variable named '" + name + "' (create it with " + name + " := ...)");
            if (var && var->readonly) fail(s.pos.line, s.pos.col, "the loop variable '" + name + "' can't be changed");
        }
        Place dst = place(*s.target);
        std::string what = s.target->kind == ExprKind::Var ? "'" + static_cast<const VarExpr &>(*s.target).name + "'" : "the value";

        Value *v;
        if (s.op == '=') {
            v = coerce(expr(*s.value), dst.type, s.value->pos, what);
        } else {
            BinOp op = s.op == '+' ? BinOp::Add : s.op == '-' ? BinOp::Sub : s.op == '*' ? BinOp::Mul
                     : s.op == '/' ? BinOp::Div : BinOp::Mod;
            Value_ cur{b.CreateLoad(ty(dst.type), dst.addr), dst.type};
            v = coerce(arith(op, cur, expr(*s.value), s.pos), dst.type, s.pos, what);
        }
        b.CreateStore(v, dst.addr);
    }

    Value *condition(const Expr &e) { return coerce(expr(e), FType::Bool, e.pos, "a condition"); }

    void ifStmt(const IfStmt &s) {
        Value *cond = condition(*s.cond);
        BasicBlock *thenBB = newBlock("then");
        BasicBlock *elseBB = s.otherwise ? newBlock("else") : nullptr;
        BasicBlock *end = newBlock("endif");
        b.CreateCondBr(cond, thenBB, elseBB ? elseBB : end);

        b.SetInsertPoint(thenBB);
        block(*s.then);
        BasicBlock *last = b.GetInsertBlock();
        if (!terminated()) b.CreateBr(end);

        if (elseBB) {
            b.SetInsertPoint(elseBB);
            stmt(*s.otherwise);
            last = b.GetInsertBlock();
            if (!terminated()) b.CreateBr(end);
        }
        continueAt(end, last);
    }

    // Continue emitting into `bb`; if nothing jumps there, drop it and stay "after a return".
    void continueAt(BasicBlock *bb, BasicBlock *deadEnd) {
        if (pred_empty(bb)) {
            bb->eraseFromParent();
            b.SetInsertPoint(deadEnd);
        } else {
            b.SetInsertPoint(bb);
        }
    }

    void whileStmt(const WhileStmt &s) {
        BasicBlock *condBB = newBlock("while.cond");
        BasicBlock *body = newBlock("while.body");
        BasicBlock *end = newBlock("while.end");
        b.CreateBr(condBB);

        b.SetInsertPoint(condBB);
        Value *cond = condition(*s.cond);
        if (auto *c = dyn_cast<ConstantInt>(cond); c && c->isOne()) b.CreateBr(body);  // while true
        else b.CreateCondBr(cond, body, end);

        b.SetInsertPoint(body);
        loops.push_back({condBB, end});
        block(*s.body);
        loops.pop_back();
        if (!terminated()) b.CreateBr(condBB);
        continueAt(end, b.GetInsertBlock());
    }

    void forStmt(const ForStmt &s) {
        Value *from = coerce(expr(*s.from), FType::I64, s.from->pos, "the start of a range");
        Value *to = coerce(expr(*s.to), FType::I64, s.to->pos, "the end of a range");

        scopes.push_back({});
        addVar(s.pos, s.var, FType::I64, from, true);
        AllocaInst *i = scopes.back()[s.var].slot;

        BasicBlock *condBB = newBlock("for.cond");
        BasicBlock *body = newBlock("for.body");
        BasicBlock *step = newBlock("for.step");
        BasicBlock *end = newBlock("for.end");
        b.CreateBr(condBB);

        b.SetInsertPoint(condBB);
        b.CreateCondBr(b.CreateICmpSLT(b.CreateLoad(b.getInt64Ty(), i), to), body, end);

        b.SetInsertPoint(body);
        loops.push_back({step, end});
        block(*s.body);
        loops.pop_back();
        if (!terminated()) b.CreateBr(step);

        b.SetInsertPoint(step);
        b.CreateStore(b.CreateAdd(b.CreateLoad(b.getInt64Ty(), i), b.getInt64(1)), i);
        b.CreateBr(condBB);

        scopes.pop_back();
        b.SetInsertPoint(end);
    }

    void returnStmt(const ReturnStmt &s) {
        bool isMain = curFn->name == "main";
        if (curFn->ret.kind == FType::Void) {
            if (s.value) fail(s.value->pos.line, s.value->pos.col, "'" + curFn->name + "' doesn't return a value (it has no '-> type')");
            if (isMain) b.CreateRet(b.getInt32(0));
            else b.CreateRetVoid();
            return;
        }
        if (!s.value) fail(s.pos.line, s.pos.col, "'" + curFn->name + "' must return " + curFn->ret.name());
        Value *v = coerce(expr(*s.value), curFn->ret, s.value->pos, "the returned value");
        b.CreateRet(isMain ? b.CreateIntCast(v, b.getInt32Ty(), true) : v);
    }

    // ---------- expressions ----------

    Value_ expr(const Expr &e) {
        switch (e.kind) {
        case ExprKind::Int: return {b.getInt64(static_cast<const IntExpr &>(e).value), FType::I64, true};
        case ExprKind::Float: return {ConstantFP::get(b.getDoubleTy(), static_cast<const FloatExpr &>(e).value), FType::F64, true};
        case ExprKind::Bool: return {b.getInt1(static_cast<const BoolExpr &>(e).value), FType::Bool};
        case ExprKind::Char: return {b.getInt8(static_cast<const CharExpr &>(e).value), FType::Char};
        case ExprKind::Str: return {b.CreateGlobalString(static_cast<const StrExpr &>(e).value, "str"), FType::Str};
        case ExprKind::Null: return {ConstantPointerNull::get(b.getPtrTy()), FType::Null};
        case ExprKind::Var: {
            auto &v = static_cast<const VarExpr &>(e);
            if (!lookup(v.name)) {
                auto c = cimports.consts.find(v.name);
                if (c != cimports.consts.end()) {
                    if (c->second.type.isFloat()) return {ConstantFP::get(b.getDoubleTy(), c->second.f), FType::F64, true};
                    return {b.getInt64(c->second.i), FType::I64, true};
                }
            }
            Place pl = place(e);
            return {b.CreateLoad(ty(pl.type), pl.addr, v.name), pl.type};
        }
        case ExprKind::Member: {
            Place pl = place(e);
            return {b.CreateLoad(ty(pl.type), pl.addr), pl.type};
        }
        case ExprKind::Unary: {
            auto &u = static_cast<const UnaryExpr &>(e);
            Value_ v = expr(*u.operand);
            if (u.op == '!') return {b.CreateNot(coerce(v, FType::Bool, u.operand->pos, "the value after '!'")), FType::Bool};
            if (u.op == '~') {
                if (!v.type.isInt()) fail(e.pos.line, e.pos.col, "'~' flips the bits of a whole number, not of " + v.type.name());
                return {b.CreateNot(v.v), v.type};
            }
            if (v.type.isSigned()) return {b.CreateNeg(v.v), v.type, v.literal};
            if (v.type.isFloat()) return {b.CreateFNeg(v.v), v.type, v.literal};
            if (v.type.isUnsigned()) fail(e.pos.line, e.pos.col, v.type.name() + " can't be negative (it's unsigned)");
            fail(e.pos.line, e.pos.col, "can't put '-' in front of " + v.type.name());
        }
        case ExprKind::Binary: return binary(static_cast<const BinaryExpr &>(e));
        case ExprKind::Call: return call(static_cast<const CallExpr &>(e));
        }
        return {nullptr, FType::Void};
    }

    static const char *opName(BinOp op) {
        switch (op) {
        case BinOp::Add: return "+";
        case BinOp::Sub: return "-";
        case BinOp::Mul: return "*";
        case BinOp::Div: return "/";
        case BinOp::Mod: return "%";
        case BinOp::BitAnd: return "&";
        case BinOp::BitOr: return "|";
        case BinOp::BitXor: return "^";
        case BinOp::Shl: return "<<";
        case BinOp::Shr: return ">>";
        case BinOp::Eq: return "==";
        case BinOp::NotEq: return "!=";
        case BinOp::Less: return "<";
        case BinOp::LessEq: return "<=";
        case BinOp::Greater: return ">";
        case BinOp::GreaterEq: return ">=";
        case BinOp::And: return "&&";
        case BinOp::Or: return "||";
        }
        return "?";
    }

    [[noreturn]] void badOperands(BinOp op, const Value_ &l, const Value_ &r, Pos p) {
        std::string msg = std::string("can't use '") + opName(op) + "' on " + l.type.name() +
                          (l.type == r.type ? "" : " and " + r.type.name());
        if (l.type.isInt() && r.type.isInt() && l.type != r.type) msg += " (convert one side, like " + l.type.name() + "(...))";
        if (l.type.kind == FType::Bool && (op == BinOp::BitAnd || op == BinOp::BitOr))
            msg += std::string(" (for true/false use '") + (op == BinOp::BitAnd ? "&&" : "||") + "')";
        else if ((l.type.isFloat() || r.type.isFloat()) && op >= BinOp::BitAnd && op <= BinOp::Shr)
            msg += " (bit operators work on whole numbers only)";
        fail(p.line, p.col, msg);
    }

    // Bring both sides to one type: a number literal adapts to the other side,
    // otherwise the smaller type grows into the bigger one.
    bool unify(Value_ &l, Value_ &r) {
        if (l.type == r.type) return true;
        if (l.type.isInt() && r.type.isInt()) {
            auto *lc = l.literal ? dyn_cast<ConstantInt>(l.v) : nullptr;
            auto *rc = r.literal ? dyn_cast<ConstantInt>(r.v) : nullptr;
            if (lc && !rc && fits(lc, l.type, r.type)) { l = {intCast(l.v, l.type, r.type), r.type, true}; return true; }
            if (rc && !lc && fits(rc, r.type, l.type)) { r = {intCast(r.v, r.type, l.type), l.type, true}; return true; }
            if (widens(l.type, r.type)) { l = {intCast(l.v, l.type, r.type), r.type}; return true; }
            if (widens(r.type, l.type)) { r = {intCast(r.v, r.type, l.type), l.type}; return true; }
            return false;
        }
        if (l.type.isInt() && r.type.isFloat()) { l = {intToFloat(l.v, l.type, r.type), r.type, l.literal}; return true; }
        if (l.type.isFloat() && r.type.isInt()) { r = {intToFloat(r.v, r.type, l.type), l.type, r.literal}; return true; }
        if (l.type.isFloat() && r.type.isFloat()) {
            if (l.type.kind == FType::F32) l = {b.CreateFPExt(l.v, b.getDoubleTy()), FType::F64};
            if (r.type.kind == FType::F32) r = {b.CreateFPExt(r.v, b.getDoubleTy()), FType::F64};
            return true;
        }
        return false;
    }

    Value_ arith(BinOp op, Value_ l, Value_ r, Pos p) {
        Value_ v = arithOp(op, l, r, p);
        v.literal = l.literal && r.literal && isa<Constant>(v.v);
        return v;
    }

    Value_ arithOp(BinOp op, Value_ l, Value_ r, Pos p) {
        if (!unify(l, r)) badOperands(op, l, r, p);
        if (l.type.isInt()) {
            bool s = l.type.isSigned();
            switch (op) {
            case BinOp::Add: return {b.CreateAdd(l.v, r.v), l.type};
            case BinOp::Sub: return {b.CreateSub(l.v, r.v), l.type};
            case BinOp::Mul: return {b.CreateMul(l.v, r.v), l.type};
            case BinOp::Div:
                checkDivisor(l, r, p);
                return {s ? b.CreateSDiv(l.v, r.v) : b.CreateUDiv(l.v, r.v), l.type};
            case BinOp::Mod:
                checkDivisor(l, r, p);
                return {s ? b.CreateSRem(l.v, r.v) : b.CreateURem(l.v, r.v), l.type};
            case BinOp::BitAnd: return {b.CreateAnd(l.v, r.v), l.type};
            case BinOp::BitOr: return {b.CreateOr(l.v, r.v), l.type};
            case BinOp::BitXor: return {b.CreateXor(l.v, r.v), l.type};
            case BinOp::Shl: return {b.CreateShl(l.v, r.v), l.type};
            case BinOp::Shr: return {s ? b.CreateAShr(l.v, r.v) : b.CreateLShr(l.v, r.v), l.type};
            default: break;
            }
        }
        if (l.type.isFloat()) {
            switch (op) {
            case BinOp::Add: return {b.CreateFAdd(l.v, r.v), l.type};
            case BinOp::Sub: return {b.CreateFSub(l.v, r.v), l.type};
            case BinOp::Mul: return {b.CreateFMul(l.v, r.v), l.type};
            case BinOp::Div: return {b.CreateFDiv(l.v, r.v), l.type};
            case BinOp::Mod: return {b.CreateFRem(l.v, r.v), l.type};
            default: break;
            }
        }
        badOperands(op, l, r, p);
    }

    // ---------- runtime errors ----------

    // finch.panic(msg, line): print "file:line: runtime error: msg" and stop the program.
    Function *panicFn() {
        if (Function *f = mod->getFunction("finch.panic")) return f;
        Function *f = Function::Create(FunctionType::get(b.getVoidTy(), {b.getPtrTy(), b.getInt64Ty()}, false),
                                       Function::InternalLinkage, "finch.panic", mod.get());
        f->addFnAttr(Attribute::NoReturn);
        f->addFnAttr(Attribute::Cold);
        f->addFnAttr(Attribute::NoInline);
        IRBuilder<> pb(BasicBlock::Create(ctx, "entry", f));
        Function *dprintf = libc("dprintf", b.getInt32Ty(), {b.getInt32Ty(), b.getPtrTy()}, true);
        pb.CreateCall(dprintf, {pb.getInt32(2), pb.CreateGlobalString("%s:%lld: runtime error: %s\n", "panic.fmt"),
                                pb.CreateGlobalString(g_file, "panic.file"), f->getArg(1), f->getArg(0)});
        pb.CreateCall(libc("exit", b.getVoidTy(), {b.getInt32Ty()}), {pb.getInt32(1)});
        pb.CreateUnreachable();
        return f;
    }

    // if (cond) panic(msg)
    void panicIf(Value *cond, const char *msg, Pos p) {
        BasicBlock *bad = newBlock("panic");
        BasicBlock *ok = newBlock("ok");
        b.CreateCondBr(cond, bad, ok);
        b.SetInsertPoint(bad);
        b.CreateCall(panicFn(), {b.CreateGlobalString(msg, "panic.msg"), b.getInt64(p.line)});
        b.CreateUnreachable();
        b.SetInsertPoint(ok);
    }

    // Dividing by zero is undefined in C; in Finch it is a clear error.
    void checkDivisor(const Value_ &l, const Value_ &r, Pos p) {
        auto *c = dyn_cast<ConstantInt>(r.v);
        if (c && c->isZero()) fail(p.line, p.col, "division by zero");
        if (!c) panicIf(b.CreateICmpEQ(r.v, ConstantInt::get(r.v->getType(), 0)), "division by zero", p);
        // the smallest signed number divided by -1 doesn't fit anywhere
        if (l.type.isSigned() && (!c || c->isMinusOne())) {
            Value *min = ConstantInt::get(l.v->getType(), APInt::getSignedMinValue(l.type.bits()));
            Value *minusOne = ConstantInt::get(r.v->getType(), -1, true);
            panicIf(b.CreateAnd(b.CreateICmpEQ(l.v, min), b.CreateICmpEQ(r.v, minusOne)),
                    "division overflows (the smallest number divided by -1)", p);
        }
    }

    Value_ compare(BinOp op, Value_ l, Value_ r, Pos p) {
        bool equality = op == BinOp::Eq || op == BinOp::NotEq;
        auto pointerLike = [](const FType &t) { return t.isPtr() || t.kind == FType::Str || t.kind == FType::Null; };

        // Pointers (and `s == null`) compare addresses.
        if (pointerLike(l.type) && pointerLike(r.type) &&
            (l.type.isPtr() || r.type.isPtr() || l.type.kind == FType::Null || r.type.kind == FType::Null)) {
            if (!equality) badOperands(op, l, r, p);
            bool bothPtr = (l.type.isPtr() || l.type.kind == FType::Null) && (r.type.isPtr() || r.type.kind == FType::Null);
            bool strNull = (l.type.kind == FType::Str && r.type.kind == FType::Null) || (r.type.kind == FType::Str && l.type.kind == FType::Null);
            if (!bothPtr && !strNull) badOperands(op, l, r, p);
            return {op == BinOp::Eq ? b.CreateICmpEQ(l.v, r.v) : b.CreateICmpNE(l.v, r.v), FType::Bool};
        }

        if (!unify(l, r)) badOperands(op, l, r, p);

        if (l.type.kind == FType::Str) {
            if (!equality) badOperands(op, l, r, p);
            Function *strcmp = libc("strcmp", b.getInt32Ty(), {b.getPtrTy(), b.getPtrTy()});
            Value *c = b.CreateCall(strcmp, {l.v, r.v});
            return {op == BinOp::Eq ? b.CreateICmpEQ(c, b.getInt32(0)) : b.CreateICmpNE(c, b.getInt32(0)), FType::Bool};
        }
        if (l.type.isFloat()) {
            CmpInst::Predicate pr;
            switch (op) {
            case BinOp::Eq: pr = CmpInst::FCMP_OEQ; break;
            case BinOp::NotEq: pr = CmpInst::FCMP_UNE; break;
            case BinOp::Less: pr = CmpInst::FCMP_OLT; break;
            case BinOp::LessEq: pr = CmpInst::FCMP_OLE; break;
            case BinOp::Greater: pr = CmpInst::FCMP_OGT; break;
            default: pr = CmpInst::FCMP_OGE; break;
            }
            return {b.CreateFCmp(pr, l.v, r.v), FType::Bool};
        }
        if (l.type.kind == FType::Bool && !equality) badOperands(op, l, r, p);
        bool s = l.type.isSigned();
        CmpInst::Predicate pr;
        switch (op) {
        case BinOp::Eq: pr = CmpInst::ICMP_EQ; break;
        case BinOp::NotEq: pr = CmpInst::ICMP_NE; break;
        case BinOp::Less: pr = s ? CmpInst::ICMP_SLT : CmpInst::ICMP_ULT; break;
        case BinOp::LessEq: pr = s ? CmpInst::ICMP_SLE : CmpInst::ICMP_ULE; break;
        case BinOp::Greater: pr = s ? CmpInst::ICMP_SGT : CmpInst::ICMP_UGT; break;
        default: pr = s ? CmpInst::ICMP_SGE : CmpInst::ICMP_UGE; break;
        }
        return {b.CreateICmp(pr, l.v, r.v), FType::Bool};
    }

    // && and || only evaluate the right side when needed.
    Value_ logic(const BinaryExpr &e) {
        bool isAnd = e.op == BinOp::And;
        Value *l = condition(*e.lhs);
        BasicBlock *from = b.GetInsertBlock();
        BasicBlock *rhsBB = newBlock(isAnd ? "and.rhs" : "or.rhs");
        BasicBlock *end = newBlock(isAnd ? "and.end" : "or.end");
        if (isAnd) b.CreateCondBr(l, rhsBB, end);
        else b.CreateCondBr(l, end, rhsBB);

        b.SetInsertPoint(rhsBB);
        Value *r = condition(*e.rhs);
        BasicBlock *rhsEnd = b.GetInsertBlock();
        b.CreateBr(end);

        b.SetInsertPoint(end);
        PHINode *phi = b.CreatePHI(b.getInt1Ty(), 2);
        phi->addIncoming(b.getInt1(!isAnd), from);
        phi->addIncoming(r, rhsEnd);
        return {phi, FType::Bool};
    }

    Value_ binary(const BinaryExpr &e) {
        switch (e.op) {
        case BinOp::And:
        case BinOp::Or: return logic(e);
        case BinOp::Eq:
        case BinOp::NotEq:
        case BinOp::Less:
        case BinOp::LessEq:
        case BinOp::Greater:
        case BinOp::GreaterEq: {
            Value_ l = expr(*e.lhs);
            return compare(e.op, l, expr(*e.rhs), e.pos);
        }
        default: {
            Value_ l = expr(*e.lhs);
            return arith(e.op, l, expr(*e.rhs), e.pos);
        }
        }
    }

    // ---------- calls & built-ins ----------

    static bool isBuiltin(const std::string &n) {
        FType t;
        return n == "print" || n == "addr" || typeFromName(n, t);
    }

    Value_ call(const CallExpr &c) {
        if (c.callee == "print") return print(c);
        if (c.callee == "addr") return addrOf(c);
        FType target;
        if (typeFromName(c.callee, target)) return convert(c, target);

        auto it = fns.find(c.callee);
        if (it != fns.end()) return callFinch(c, it->second);

        auto cf = cimports.fns.find(c.callee);
        if (cf != cimports.fns.end()) return callC(c, cf->second);

        if (lookup(c.callee)) fail(c.pos.line, c.pos.col, "'" + c.callee + "' is a variable, not a function");
        fail(c.pos.line, c.pos.col, "there is no function named '" + c.callee + "'" + headerHint(c.callee));
    }

    // Help people who forgot an import.
    static std::string headerHint(const std::string &name) {
        static const std::unordered_map<std::string, const char *> known = {
            {"printf", "stdio.h"}, {"puts", "stdio.h"},   {"putchar", "stdio.h"}, {"scanf", "stdio.h"},
            {"fopen", "stdio.h"},  {"getchar", "stdio.h"}, {"sqrt", "math.h"},     {"pow", "math.h"},
            {"sin", "math.h"},     {"cos", "math.h"},     {"floor", "math.h"},    {"fabs", "math.h"},
            {"malloc", "stdlib.h"}, {"free", "stdlib.h"}, {"rand", "stdlib.h"},   {"exit", "stdlib.h"},
            {"abs", "stdlib.h"},   {"atoi", "stdlib.h"},  {"strlen", "string.h"}, {"strcpy", "string.h"},
            {"memcpy", "string.h"}, {"time", "time.h"},   {"sleep", "unistd.h"},
        };
        auto it = known.find(name);
        return it == known.end() ? "" : std::string(" (it's from C: add  import \"") + it->second + "\"  at the top)";
    }

    void checkArgCount(const CallExpr &c, size_t want, bool variadic) {
        if (c.args.size() == want || (variadic && c.args.size() > want)) return;
        fail(c.pos.line, c.pos.col, "'" + c.callee + "' takes " + (variadic ? "at least " : "") + std::to_string(want) +
                                        " argument(s), but got " + std::to_string(c.args.size()));
    }

    Value_ callFinch(const CallExpr &c, const Fn &fn) {
        const FnDecl &d = *fn.decl;
        if (d.name == "main") fail(c.pos.line, c.pos.col, "main can't be called, it is where the program starts");
        checkArgCount(c, d.params.size(), false);
        std::vector<Value *> args;
        for (size_t i = 0; i < c.args.size(); i++)
            args.push_back(coerce(expr(*c.args[i]), d.params[i].type, c.args[i]->pos, "argument '" + d.params[i].name + "'"));
        return {b.CreateCall(fn.llvm, args), d.ret};
    }

    // C wants small integers widened by the caller: signed ones sign-extended, the rest zero-extended.
    static Attribute::AttrKind extAttr(const FType &t) {
        if (t.bits() >= 32 || !(t.isInt() || t.kind == FType::Bool || t.kind == FType::Char)) return Attribute::None;
        return t.isSigned() || t.kind == FType::Char ? Attribute::SExt : Attribute::ZExt;
    }

    Value_ callC(const CallExpr &c, const CFunc &f) {
        if (!f.unsupported.empty())
            fail(c.pos.line, c.pos.col, "the C function '" + f.name + "' can't be used from Finch yet: it " + f.unsupported);
        checkArgCount(c, f.params.size(), f.variadic);

        std::vector<llvm::Type *> ptypes;
        for (const FType &t : f.params) ptypes.push_back(ty(t));
        Function *fn = libc(f.name.c_str(), ty(f.ret), ptypes, f.variadic);

        std::vector<Value *> args;
        for (size_t i = 0; i < c.args.size(); i++) {
            Value_ v = expr(*c.args[i]);
            if (i < f.params.size()) {
                args.push_back(coerce(v, f.params[i], c.args[i]->pos, "argument " + std::to_string(i + 1) + " of " + f.name));
                continue;
            }
            // extra arguments of printf-like functions follow C's promotion rules
            const FType &t = v.type;
            if (t.kind == FType::Void) fail(c.args[i]->pos.line, c.args[i]->pos.col, "this gives back nothing");
            if (t.kind == FType::F32) v.v = b.CreateFPExt(v.v, b.getDoubleTy());
            else if (t.kind == FType::Bool || ((t.isInt() || t.kind == FType::Char) && t.bits() < 32))
                v.v = b.CreateIntCast(v.v, b.getInt32Ty(), t.isSigned() || t.kind == FType::Char);
            args.push_back(v.v);
        }

        CallInst *call = b.CreateCall(fn, args);
        for (size_t i = 0; i < f.params.size(); i++) {
            Attribute::AttrKind a = extAttr(f.params[i]);
            if (a != Attribute::None) {
                fn->addParamAttr(i, a);
                call->addParamAttr(i, a);
            }
        }
        if (Attribute::AttrKind a = extAttr(f.ret); a != Attribute::None) {
            fn->addRetAttr(a);
            call->addRetAttr(a);
        }
        return {call, f.ret};
    }

    Value *cGlobal(const std::string &name) {
        const CGlobal &g = cimports.globals.at(name);
        if (GlobalVariable *gv = mod->getNamedGlobal(name)) return gv;
        return new GlobalVariable(*mod, ty(g.type), false, GlobalValue::ExternalLinkage, nullptr, name);
    }

    // addr(x) gives a pointer to x
    Value_ addrOf(const CallExpr &c) {
        if (c.args.size() != 1) fail(c.pos.line, c.pos.col, "addr(...) takes exactly one variable");
        const Expr &e = *c.args[0];
        if (e.kind != ExprKind::Var && e.kind != ExprKind::Member)
            fail(e.pos.line, e.pos.col, "addr(...) needs a variable, like addr(x)");
        if (e.kind == ExprKind::Var) {
            Var *v = lookup(static_cast<const VarExpr &>(e).name);
            if (v && v->readonly) fail(e.pos.line, e.pos.col, "can't take the address of a loop variable");
        }
        Place pl = place(e);
        return {pl.addr, FType::ptrTo(pl.type)};
    }

    // print(a, b, c) prints the values separated by spaces, then a newline.
    Value_ print(const CallExpr &c) {
        std::string fmt;
        std::vector<Value *> args;
        for (size_t i = 0; i < c.args.size(); i++) {
            if (i) fmt += ' ';
            Value_ v = expr(*c.args[i]);
            const FType &t = v.type;
            if (t.isSigned()) { fmt += "%lld"; args.push_back(b.CreateSExt(v.v, b.getInt64Ty())); }
            else if (t.isUnsigned()) { fmt += "%llu"; args.push_back(b.CreateZExt(v.v, b.getInt64Ty())); }
            else if (t.isFloat()) { fmt += "%g"; args.push_back(b.CreateFPExt(v.v, b.getDoubleTy())); }
            else if (t.kind == FType::Char) { fmt += "%c"; args.push_back(b.CreateZExt(v.v, b.getInt32Ty())); }
            else if (t.kind == FType::Str) { fmt += "%s"; args.push_back(v.v); }
            else if (t.isPtr()) { fmt += "%p"; args.push_back(v.v); }
            else if (t.kind == FType::Null) fmt += "null";
            else if (t.kind == FType::Bool) {
                fmt += "%s";
                args.push_back(b.CreateSelect(v.v, b.CreateGlobalString("true", "true"), b.CreateGlobalString("false", "false")));
            } else fail(c.args[i]->pos.line, c.args[i]->pos.col, "this gives back nothing, so there is nothing to print");
        }
        fmt += '\n';
        args.insert(args.begin(), b.CreateGlobalString(fmt, "fmt"));
        b.CreateCall(libc("printf", b.getInt32Ty(), {b.getPtrTy()}, true), args);
        return {nullptr, FType::Void};
    }

    // Explicit conversions: int(x), u8(x), float(x), char(x), bool(x), ptr(x) ...
    Value_ convert(const CallExpr &c, const FType &to) {
        if (c.args.size() != 1) fail(c.pos.line, c.pos.col, c.callee + "(...) takes exactly one value");
        Value_ v = expr(*c.args[0]);
        const FType &from = v.type;
        Pos p = c.args[0]->pos;
        if (from == to) return v;

        if (to.isInt() || to.kind == FType::Char) {
            if (from.isInt() || from.kind == FType::Char) return {b.CreateIntCast(v.v, ty(to), from.isSigned()), to};
            if (from.kind == FType::Bool) return {b.CreateZExt(v.v, ty(to)), to};
            if (from.isFloat() && to.isInt())
                return {to.isSigned() ? b.CreateFPToSI(v.v, ty(to)) : b.CreateFPToUI(v.v, ty(to)), to};
        }
        if (to.isFloat()) {
            if (from.isInt()) return {intToFloat(v.v, from, to), to};
            if (from.isFloat()) return {b.CreateFPCast(v.v, ty(to)), to};
        }
        if (to.kind == FType::Bool && from.isInt()) return {b.CreateICmpNE(v.v, ConstantInt::get(ty(from), 0)), to};
        if (to.isPtr() && (from.isPtr() || from.kind == FType::Str || from.kind == FType::Null)) return {v.v, to};
        if (to.kind == FType::Str && from.isPtr()) return {v.v, to};
        fail(p.line, p.col, "can't turn " + from.name() + " into " + to.name());
    }
};

}  // namespace

std::unique_ptr<Module> generate(const Program &prog, const CImports &c, LLVMContext &ctx, TargetMachine &tm) {
    return Codegen(ctx, c, tm).run(prog);
}
