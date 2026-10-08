// Code generation, part 3: calls, built-in functions, conversions and methods.

#include "codegen_impl.h"

using namespace llvm;

namespace finch {

bool Codegen::isBuiltin(const std::string &n) {
    static const std::set<std::string> names = {"print", "addr", "input", "new", "free", "read_file", "write_file",
                                                "file_exists", "exit", "shell"};
    FType t;
    return names.count(n) || typeFromName(n, t);
}

// Help people who forgot an import.
std::string Codegen::headerHint(const std::string &name) {
    static const std::unordered_map<std::string, const char *> known = {
        {"printf", "stdio.h"},  {"puts", "stdio.h"},   {"putchar", "stdio.h"}, {"scanf", "stdio.h"},
        {"fopen", "stdio.h"},   {"getchar", "stdio.h"}, {"sqrt", "math.h"},     {"pow", "math.h"},
        {"sin", "math.h"},      {"cos", "math.h"},     {"floor", "math.h"},    {"fabs", "math.h"},
        {"malloc", "stdlib.h"}, {"rand", "stdlib.h"},  {"srand", "stdlib.h"},  {"abs", "stdlib.h"},
        {"atoi", "stdlib.h"},   {"strlen", "string.h"}, {"strcpy", "string.h"}, {"memcpy", "string.h"},
        {"time", "time.h"},     {"sleep", "unistd.h"}, {"usleep", "unistd.h"},
    };
    auto it = known.find(name);
    return it == known.end() ? "" : std::string(" (it's from C: add  import \"") + it->second + "\"  at the top)";
}

void Codegen::checkArgs(const std::vector<ExprPtr> &args, const std::vector<std::string> &names, size_t want, Pos p,
                        const std::string &name, bool variadic) {
    for (size_t i = 0; i < names.size(); i++)
        if (!names[i].empty())
            failAt(args[i]->pos.file, args[i]->pos.line, args[i]->pos.col, "named arguments (" + names[i] + ": ...) are only for creating structs");
    if (args.size() == want || (variadic && args.size() > want)) return;
    failAt(p.file, p.line, p.col, "'" + name + "' takes " + (variadic ? "at least " : "") + std::to_string(want) +
                                      " argument(s), but got " + std::to_string(args.size()));
}

Function *Codegen::libc(const char *name, llvm::Type *ret, std::vector<llvm::Type *> params, bool vararg) {
    FunctionCallee c = mod->getOrInsertFunction(name, FunctionType::get(ret, params, vararg));
    return cast<Function>(c.getCallee());
}

// The runtime (runtime/finch_rt.c), declared on first use.
Function *Codegen::rt(const std::string &name) {
    if (Function *f = mod->getFunction(name)) return f;
    llvm::Type *P = b.getPtrTy(), *I = b.getInt64Ty(), *I32 = b.getInt32Ty(), *D = b.getDoubleTy(), *V = b.getVoidTy();
    struct Sig { llvm::Type *ret; std::vector<llvm::Type *> params; };
    std::map<std::string, Sig> sigs = {
        {"finch_panic", {V, {P, I, P}}},
        {"finch_panic_index", {V, {P, I, I, I}}},
        {"finch_alloc", {P, {I}}},
        {"finch_zalloc", {P, {I}}},
        {"finch_str_copy", {V, {P, P}}},
        {"finch_str_own", {V, {P}}},
        {"finch_str_drop", {V, {P}}},
        {"finch_str_from_c", {V, {P, P}}},
        {"finch_str_concat", {V, {P, P, P}}},
        {"finch_str_eq", {I32, {P, P}}},
        {"finch_str_cmp", {I, {P, P}}},
        {"finch_str_from_int", {V, {P, I}}},
        {"finch_str_from_uint", {V, {P, I}}},
        {"finch_str_from_float", {V, {P, D}}},
        {"finch_str_from_char", {V, {P, b.getInt8Ty()}}},
        {"finch_str_from_bool", {V, {P, I32}}},
        {"finch_str_to_int", {I, {P, P, I}}},
        {"finch_str_to_float", {D, {P, P, I}}},
        {"finch_str_sub", {V, {P, P, I, I, P, I}}},
        {"finch_str_find", {I, {P, P}}},
        {"finch_str_starts", {I32, {P, P}}},
        {"finch_str_ends", {I32, {P, P}}},
        {"finch_str_trim", {V, {P, P}}},
        {"finch_str_upper", {V, {P, P}}},
        {"finch_str_lower", {V, {P, P}}},
        {"finch_str_replace", {V, {P, P, P, P}}},
        {"finch_str_split", {V, {P, P, P}}},
        {"finch_str_join", {V, {P, P, P}}},
        {"finch_str_repeat", {V, {P, P, I}}},
        {"finch_str_from_bytes", {V, {P, P}}},
        {"finch_arr_reserve", {V, {P, I, I}}},
        {"finch_arr_make", {V, {P, I, I}}},
        {"finch_arr_resize", {V, {P, I, I}}},
        {"finch_arr_clone_raw", {V, {P, P, I}}},
        {"finch_arr_free", {V, {P}}},
        {"finch_arr_insert_gap", {V, {P, I, I, P, I}}},
        {"finch_arr_remove_gap", {V, {P, I, I}}},
        {"finch_bytes_from_str", {V, {P, P}}},
        {"finch_args", {V, {P, I32, P}}},
        {"finch_input", {V, {P, P}}},
        {"finch_file_exists", {I32, {P}}},
        {"finch_read_file", {V, {P, P, P, I}}},
        {"finch_write_file", {I32, {P, P}}},
        {"finch_shell", {I, {P}}},
    };
    auto it = sigs.find(name);
    if (it == sigs.end()) {
        std::fprintf(stderr, "internal compiler error: unknown runtime function %s\n", name.c_str());
        std::exit(2);
    }
    Function *f = Function::Create(FunctionType::get(it->second.ret, it->second.params, false), Function::ExternalLinkage,
                                   name, mod.get());
    if (name == "finch_panic" || name == "finch_panic_index") {
        f->addFnAttr(Attribute::NoReturn);
        f->addFnAttr(Attribute::Cold);
    }
    if (name == "finch_str_from_char") f->addParamAttr(1, Attribute::SExt);
    return f;
}

// ---------- runtime errors ----------

Value *Codegen::fileName(Pos p) {
    std::string key = "file." + std::to_string(p.file);
    if (GlobalVariable *g = mod->getNamedGlobal("finch." + key)) return g;
    GlobalVariable *g = b.CreateGlobalString(g_files[p.file].path, "finch." + key);
    return g;
}

void Codegen::panicIf(Value *cond, const std::string &msg, Pos p) {
    BasicBlock *bad = newBlock("panic");
    BasicBlock *ok = newBlock("ok");
    b.CreateCondBr(cond, bad, ok);
    b.SetInsertPoint(bad);
    b.CreateCall(rt("finch_panic"), {fileName(p), b.getInt64(p.line), b.CreateGlobalString(msg, "panic.msg")});
    b.CreateUnreachable();
    b.SetInsertPoint(ok);
}

void Codegen::boundsCheck(Value *i, Value *len, Pos p) {
    BasicBlock *bad = newBlock("out.of.range");
    BasicBlock *ok = newBlock("in.range");
    b.CreateCondBr(b.CreateICmpUGE(i, len), bad, ok);  // unsigned: catches negative indexes too
    b.SetInsertPoint(bad);
    b.CreateCall(rt("finch_panic_index"), {fileName(p), b.getInt64(p.line), i, len});
    b.CreateUnreachable();
    b.SetInsertPoint(ok);
}

// Dividing by zero is undefined in C; in Finch it is a clear error.
void Codegen::checkDivisor(const Value_ &l, const Value_ &r, Pos p) {
    auto *c = dyn_cast<ConstantInt>(r.v);
    if (c && c->isZero()) failAt(p.file, p.line, p.col, "division by zero");
    if (!c) panicIf(b.CreateICmpEQ(r.v, ConstantInt::get(r.v->getType(), 0)), "division by zero", p);
    // the smallest signed number divided by -1 doesn't fit anywhere
    if (l.type.isSigned() && (!c || c->isMinusOne())) {
        Value *min = ConstantInt::get(l.v->getType(), APInt::getSignedMinValue(l.type.bits()));
        Value *minusOne = ConstantInt::get(r.v->getType(), -1, true);
        panicIf(b.CreateAnd(b.CreateICmpEQ(l.v, min), b.CreateICmpEQ(r.v, minusOne)),
                "division overflows (the smallest number divided by -1)", p);
    }
}

Value *Codegen::cGlobal(const std::string &name) {
    if (GlobalVariable *gv = mod->getNamedGlobal(name)) return gv;
    FType t = resolve(cimports.globals.at(name).type, Pos{});
    return new GlobalVariable(*mod, ty(t), false, GlobalValue::ExternalLinkage, nullptr, name);
}

// ---------- calls ----------

Value_ Codegen::call(const CallExpr &c) {
    const std::string &n = c.callee;
    Pos p = c.pos;
    auto one = [&](const char *what) -> const Expr & {
        checkArgs(c.args, c.argNames, 1, p, n);
        (void)what;
        return *c.args[0];
    };

    if (n == "print") {
        checkArgs(c.args, c.argNames, 0, p, n, true);
        return print(c.args, p);
    }
    if (n == "addr") {
        checkArgs(c.args, c.argNames, 1, p, n);
        return addrOf(c.args, p);
    }
    if (n == "input") {
        if (c.args.size() > 1) checkArgs(c.args, c.argNames, 1, p, n);
        Value_ prompt = c.args.empty() ? Value_{strConst(""), FType::Str, false, true}
                                       : coerce(expr(*c.args[0]), FType::Str, c.args[0]->pos, "the question");
        Value *out = tmpOf(ty(FType::Str));
        b.CreateCall(rt("finch_input"), {out, tmp(prompt.v)});
        release(prompt);
        return {b.CreateLoad(ty(FType::Str), out), FType::Str, false, true};
    }
    if (n == "new") {
        const Expr &a = one("value");
        Value_ v = expr(a);
        if (v.type.kind == FType::Void || v.type.kind == FType::Null)
            failAt(a.pos.file, a.pos.line, a.pos.col, "new(...) needs a value to put in memory, like new(Point(x: 1, y: 2))");
        Value *mem = b.CreateCall(rt("finch_alloc"), {elemSize(v.type)});
        b.CreateStore(own(v), mem);
        return {mem, FType::ptrTo(v.type)};
    }
    if (n == "free") {
        const Expr &a = one("pointer");
        Value_ v = expr(a);
        if (!v.type.isPtr())
            failAt(a.pos.file, a.pos.line, a.pos.col, "free(...) is only for pointers from new(...) or C; arrays, str and structs are freed by Finch at the end of their block");
        if (v.type.elem && owning(*v.type.elem)) {
            BasicBlock *drop = newBlock("free.drop"), *done = newBlock("free.done");
            b.CreateCondBr(b.CreateIsNull(v.v), done, drop);
            b.SetInsertPoint(drop);
            dropAt(v.v, *v.type.elem);
            b.CreateBr(done);
            b.SetInsertPoint(done);
        }
        b.CreateCall(libc("free", b.getVoidTy(), {b.getPtrTy()}), {v.v});
        return {nullptr, FType::Void};
    }
    if (n == "exit") {
        Value *code = coerce(expr(one("code")), FType::I32, c.args[0]->pos, "the exit code").v;
        b.CreateCall(libc("fflush", b.getInt32Ty(), {b.getPtrTy()}), {ConstantPointerNull::get(b.getPtrTy())});
        b.CreateCall(libc("exit", b.getVoidTy(), {b.getInt32Ty()}), {code});
        return {nullptr, FType::Void};
    }
    if (n == "read_file" || n == "file_exists") {
        Value_ path = coerce(expr(one("path")), FType::Str, c.args[0]->pos, "the file name");
        Value *pp = tmp(path.v);
        Value_ res;
        if (n == "file_exists") {
            res = {b.CreateICmpNE(b.CreateCall(rt("finch_file_exists"), {pp}), b.getInt32(0)), FType::Bool};
        } else {
            Value *out = tmpOf(ty(FType::Str));
            b.CreateCall(rt("finch_read_file"), {out, pp, fileName(p), b.getInt64(p.line)});
            res = {b.CreateLoad(ty(FType::Str), out), FType::Str, false, true};
        }
        release(path);
        return res;
    }
    if (n == "shell") {
        Value_ cmd = coerce(expr(one("command")), FType::Str, c.args[0]->pos, "the command");
        Value *ss = tmpOf(ty(FType::Str));
        b.CreateCall(rt("finch_str_copy"), {ss, tmp(cmd.v)});  // a heap copy is always NUL-terminated
        Value *r = b.CreateCall(rt("finch_shell"), {ss});
        dropAt(ss, FType::Str);
        release(cmd);
        return {r, FType::I64};
    }
    if (n == "write_file") {
        checkArgs(c.args, c.argNames, 2, p, n);
        Value_ path = coerce(expr(*c.args[0]), FType::Str, c.args[0]->pos, "the file name");
        Value_ text = coerce(expr(*c.args[1]), FType::Str, c.args[1]->pos, "the text");
        Value *ok = b.CreateCall(rt("finch_write_file"), {tmp(path.v), tmp(text.v)});
        release(path);
        release(text);
        return {b.CreateICmpNE(ok, b.getInt32(0)), FType::Bool};
    }
    FType target;
    if (typeFromName(n, target)) {
        checkArgs(c.args, c.argNames, 1, p, n);
        return convert(target, c.args, p, n);
    }

    ModuleScope &m = modules[curModule];
    if (auto it = m.fns.find(n); it != m.fns.end()) return callFinch(it->second, c.args, c.argNames, p, n);
    if (StructInfo *s = findStruct("", n, false)) return construct(s, c.args, c.argNames, p);
    if (auto cf = cimports.fns.find(n); cf != cimports.fns.end()) {
        checkArgs(c.args, c.argNames, cf->second.params.size(), p, n, cf->second.variadic);
        return callC(cf->second, c.args, p);
    }
    if (lookup(n)) failAt(p.file, p.line, p.col, "'" + n + "' is a variable, not a function");
    for (auto &[mname, ms] : modules)
        if (ms.fns.count(n) && mname != curModule && !mname.empty())
            failAt(p.file, p.line, p.col, "there is no function named '" + n + "' here; it is in the module " + mname +
                                              " (write " + mname + "." + n + "(...)" + (m.imports.count(mname) ? "" : " and add: import " + mname) + ")");
    failAt(p.file, p.line, p.col, "there is no function named '" + n + "'" + headerHint(n));
}

Value_ Codegen::callFinch(const Fn &fn, const std::vector<ExprPtr> &args, const std::vector<std::string> &names, Pos p,
                          const std::string &shown) {
    const FnDecl &d = *fn.decl;
    if (fn.module.empty() && d.name == "main") failAt(p.file, p.line, p.col, "main can't be called, it is where the program starts");
    checkArgs(args, names, fn.params.size(), p, shown);
    std::vector<Value *> vals;
    std::vector<Value_> fresh;
    for (size_t i = 0; i < args.size(); i++) {
        Value_ v = coerce(exprWant(*args[i], fn.params[i]), fn.params[i], args[i]->pos, "argument '" + d.params[i].name + "'");
        vals.push_back(v.v);  // owning values are lent: the function never drops them
        if (v.fresh) fresh.push_back(v);
    }
    setLoc(p);
    Value *r = b.CreateCall(fn.llvm, vals);
    for (auto &v : fresh) release(v);
    if (fn.ret.kind == FType::Void) return {nullptr, FType::Void};
    return {r, fn.ret, false, owning(fn.ret)};
}

Value_ Codegen::construct(StructInfo *s, const std::vector<ExprPtr> &args, const std::vector<std::string> &names, Pos p) {
    resolveStruct(s);
    FType t(FType::Struct);
    t.info = s;
    t.name = s->name;
    t.module = s->module == "C" ? "" : s->module;
    std::vector<const StructInfo::F *> fields;
    for (auto &f : s->fields) fields.push_back(&f);

    bool named = !names.empty() && !names[0].empty();
    std::vector<const Expr *> given(fields.size(), nullptr);
    for (size_t i = 0; i < args.size(); i++) {
        Pos ap = args[i]->pos;
        if (names[i].empty() == named)
            failAt(ap.file, ap.line, ap.col, "give either all fields by name (x: 1, y: 2) or none (1, 2), not a mix");
        if (!named) {
            if (args.size() != fields.size())
                failAt(p.file, p.line, p.col, s->name + " has " + std::to_string(fields.size()) + " fields, but got " +
                                                  std::to_string(args.size()) + " values (or name them: " + s->name + "(field: value))");
            given[i] = args[i].get();
            continue;
        }
        size_t k = 0;
        while (k < fields.size() && fields[k]->name != names[i]) k++;
        if (k == fields.size()) failAt(ap.file, ap.line, ap.col, s->name + " has no field '" + names[i] + "'");
        if (given[k]) failAt(ap.file, ap.line, ap.col, "the field '" + names[i] + "' is given twice");
        given[k] = args[i].get();
    }

    Value *v = zero(t);
    for (size_t k = 0; k < fields.size(); k++) {
        const StructInfo::F &f = *fields[k];
        Value_ fv;
        if (given[k]) {
            fv = coerce(exprWant(*given[k], f.type), f.type, given[k]->pos, "the field '" + f.name + "'");
        } else if (f.init) {
            // a default value is evaluated where the struct is defined, without the caller's variables
            std::vector<Scope> saved;
            std::swap(saved, scopes);
            scopes.reserve(256);
            pushScope();
            std::string savedModule = curModule;
            curModule = s->module;
            fv = coerce(exprWant(*f.init, f.type), f.type, f.init->pos, "the default of '" + f.name + "'");
            curModule = savedModule;
            scopes.pop_back();
            std::swap(saved, scopes);
        } else {
            continue;  // zero
        }
        v = b.CreateInsertValue(v, own(fv), f.llvmIndex);
    }
    return {v, t, false, owning(t)};
}

Value_ Codegen::callC(const CFunc &f, const std::vector<ExprPtr> &args, Pos p) {
    if (!f.unsupported.empty())
        failAt(p.file, p.line, p.col, "the C function '" + f.name + "' can't be used from Finch yet: it " + f.unsupported);
    std::vector<Value_> vals;
    for (size_t i = 0; i < args.size(); i++) {
        if (i < f.params.size()) {
            FType pt = resolve(f.params[i], p);
            vals.push_back(coerce(exprWant(*args[i], pt), pt, args[i]->pos, "argument " + std::to_string(i + 1) + " of " + f.name));
        } else {
            Value_ v = expr(*args[i]);
            if (v.type.kind == FType::Void) failAt(args[i]->pos.file, args[i]->pos.line, args[i]->pos.col, "this gives back nothing");
            if (v.type.kind == FType::Struct || v.type.kind == FType::Array)
                failAt(args[i]->pos.file, args[i]->pos.line, args[i]->pos.col, "structs and arrays can't be passed to the '...' part of a C function");
            vals.push_back(v);
        }
    }
    setLoc(p);
    Value_ r = emitCCall(f, declareC(f), vals, p);
    for (auto &v : vals) release(v);
    return r;
}

// ---------- built-ins ----------

// print(a, b, c) prints the values separated by spaces, then a newline.
Value_ Codegen::print(const std::vector<ExprPtr> &args, Pos) {
    for (size_t i = 0; i < args.size(); i++) {
        Value_ v = expr(*args[i]);
        if (v.type.kind == FType::Void)
            failAt(args[i]->pos.file, args[i]->pos.line, args[i]->pos.col, "this gives back nothing, so there is nothing to print");
        if (i) printf_(" ", {});
        emitPrint(v.v, v.type, false);
        release(v);
    }
    printf_("\n", {});
    return {nullptr, FType::Void};
}

// addr(x) gives a pointer to x; addr(function) gives a C-callable function pointer.
Value_ Codegen::addrOf(const std::vector<ExprPtr> &args, Pos p) {
    const Expr &e = *args[0];
    if (e.kind == ExprKind::Var) {
        const std::string &n = static_cast<const VarExpr &>(e).name;
        Var *v = lookup(n);
        if (!v) {
            auto &fns = modules[curModule].fns;
            if (auto it = fns.find(n); it != fns.end()) return {cThunk(it->second, p), FType::Ptr};
        }
        if (v && v->readonly) failAt(e.pos.file, e.pos.line, e.pos.col, "can't take the address of a loop variable");
    }
    if (e.kind != ExprKind::Var && e.kind != ExprKind::Member && e.kind != ExprKind::Index)
        failAt(e.pos.file, e.pos.line, e.pos.col, "addr(...) needs a variable, a field or an element, like addr(x)");
    Place pl = place(e, true);
    return {pl.addr, FType::ptrTo(pl.type)};
}

// Explicit conversions: int(x), u8(x), float(x), char(x), bool(x), str(x), ptr(x) ...
Value_ Codegen::convert(const FType &to, const std::vector<ExprPtr> &args, Pos, const std::string &name) {
    Value_ v = expr(*args[0]);
    const FType &from = v.type;
    Pos p = args[0]->pos;
    if (to.kind == FType::Str) {
        Value *out = tmpOf(ty(FType::Str));
        if (from.kind == FType::Str) return {copyValue(v.v, FType::Str), FType::Str, false, true};
        if (from.isPtr()) return strFromC(v.v);
        if (from.isSigned()) b.CreateCall(rt("finch_str_from_int"), {out, b.CreateSExt(v.v, b.getInt64Ty())});
        else if (from.isUnsigned()) b.CreateCall(rt("finch_str_from_uint"), {out, b.CreateZExt(v.v, b.getInt64Ty())});
        else if (from.isFloat()) b.CreateCall(rt("finch_str_from_float"), {out, b.CreateFPExt(v.v, b.getDoubleTy())});
        else if (from.kind == FType::Char) b.CreateCall(rt("finch_str_from_char"), {out, v.v});
        else if (from.kind == FType::Bool) b.CreateCall(rt("finch_str_from_bool"), {out, b.CreateZExt(v.v, b.getInt32Ty())});
        else if (from.kind == FType::Array && (from.elem->kind == FType::U8 || from.elem->kind == FType::Char)) {
            b.CreateCall(rt("finch_str_from_bytes"), {out, tmp(v.v)});
            release(v);
        } else {
            failAt(p.file, p.line, p.col, "can't turn " + from.show() + " into str");
        }
        return {b.CreateLoad(ty(FType::Str), out), FType::Str, false, true};
    }
    if (from == to) return v;
    if (from.kind == FType::Str && (to.isInt() || to.isFloat())) {
        Value *sp = tmp(v.v);
        Value_ r;
        if (to.isInt()) r = {intCast(b.CreateCall(rt("finch_str_to_int"), {sp, fileName(p), b.getInt64(p.line)}), FType::I64, to), to};
        else r = {b.CreateFPCast(b.CreateCall(rt("finch_str_to_float"), {sp, fileName(p), b.getInt64(p.line)}), ty(to)), to};
        release(v);
        return r;
    }
    if (to.isInt() || to.kind == FType::Char) {
        if (from.isInt() || from.kind == FType::Char) return {b.CreateIntCast(v.v, ty(to), from.isSigned()), to};
        if (from.kind == FType::Bool) return {b.CreateZExt(v.v, ty(to)), to};
        if (from.isFloat() && to.isInt()) return {to.isSigned() ? b.CreateFPToSI(v.v, ty(to)) : b.CreateFPToUI(v.v, ty(to)), to};
    }
    if (to.isFloat()) {
        if (from.isInt()) return {intToFloat(v.v, from, to), to};
        if (from.isFloat()) return {b.CreateFPCast(v.v, ty(to)), to};
    }
    if (to.kind == FType::Bool && from.isInt()) return {b.CreateICmpNE(v.v, ConstantInt::get(ty(from), 0)), to};
    if (to.isPtr() && (from.isPtr() || from.kind == FType::Null)) return {v.v, to};
    if (to.isPtr() && from.isInt()) return {b.CreateIntToPtr(intCast(v.v, from, FType::U64), b.getPtrTy()), to};  // an address or offset for C
    if (to.isInt() && from.isPtr()) return {b.CreatePtrToInt(v.v, ty(to)), to};
    if (to.isPtr() && from.kind == FType::Str) failAt(p.file, p.line, p.col, "use .ptr to get a str's address (it is only valid while the str lives)");
    failAt(p.file, p.line, p.col, "can't turn " + from.show() + " into " + name);
}

// ---------- methods ----------

Value_ Codegen::method(const MethodExpr &m) {
    Pos p = m.pos;
    // math.sqrt(x), math.Vec(1, 2)
    if (m.obj->kind == ExprKind::Var) {
        const std::string &mn = static_cast<const VarExpr &>(*m.obj).name;
        if (!lookup(mn) && modules[curModule].imports.count(mn)) {
            auto mi = modules.find(mn);
            if (mi == modules.end()) failAt(p.file, p.line, p.col, "the module '" + mn + "' was not found");
            if (auto it = mi->second.fns.find(m.name); it != mi->second.fns.end())
                return callFinch(it->second, m.args, m.argNames, p, mn + "." + m.name);
            if (auto it = mi->second.structs.find(m.name); it != mi->second.structs.end())
                return construct(it->second, m.args, m.argNames, p);
            failAt(p.file, p.line, p.col, "the module '" + mn + "' has no function or struct named '" + m.name + "'");
        }
        if (!lookup(mn) && !cimports.globals.count(mn) && !cimports.consts.count(mn))
            failAt(m.obj->pos.file, m.obj->pos.line, m.obj->pos.col,
                   "there is no variable or module named '" + mn + "' (to use a module, add  import " + mn + "  at the top)");
    }
    static const std::set<std::string> changing = {"push", "pop", "insert", "remove", "clear", "resize", "sort", "reverse"};
    LRef r = ref(*m.obj, changing.count(m.name) > 0);
    FType t = r.isPlace ? r.pl.type : r.val.type;
    if (t.kind == FType::Array) {
        Value_ recv = r.isPlace ? Value_{r.pl.addr, t} : r.val;
        if (!r.isPlace) {
            if (changing.count(m.name)) failAt(p.file, p.line, p.col, m.name + "() would change a temporary array; store it in a variable first");
            recv = {tmp(r.val.v), t, false, r.val.fresh};
        }
        Value_ out = arrayMethod(m, &recv);
        if (!r.isPlace && r.val.fresh) dropAt(recv.v, t);
        return out;
    }
    if (t.kind == FType::Str) {
        Value_ sv = r.isPlace ? Value_{b.CreateLoad(ty(t), r.pl.addr), t} : r.val;
        Value *addr = tmp(sv.v);  // the runtime takes strings by address
        auto argStr = [&](size_t i, const char *what) {
            return coerce(expr(*m.args[i]), FType::Str, m.args[i]->pos, what);
        };
        auto argInt = [&](size_t i, const char *what) {
            return coerce(expr(*m.args[i]), FType::I64, m.args[i]->pos, what).v;
        };
        auto need = [&](size_t n) { checkArgs(m.args, m.argNames, n, p, m.name); };
        Value *out = tmpOf(ty(FType::Str));
        Value_ res;
        const std::string &n = m.name;
        auto strRes = [&]() { return Value_{b.CreateLoad(ty(FType::Str), out), FType::Str, false, true}; };
        if (n == "sub") {
            need(2);
            Value *a = argInt(0, "the start"), *e = argInt(1, "the end");
            b.CreateCall(rt("finch_str_sub"), {out, addr, a, e, fileName(p), b.getInt64(p.line)});
            res = strRes();
        } else if (n == "find" || n == "contains" || n == "starts_with" || n == "ends_with") {
            need(1);
            Value_ t2 = argStr(0, "the text to look for");
            Value *tp = tmp(t2.v);
            if (n == "find") res = {b.CreateCall(rt("finch_str_find"), {addr, tp}), FType::I64};
            else if (n == "contains") res = {b.CreateICmpSGE(b.CreateCall(rt("finch_str_find"), {addr, tp}), b.getInt64(0)), FType::Bool};
            else res = {b.CreateICmpNE(b.CreateCall(rt(n == "starts_with" ? "finch_str_starts" : "finch_str_ends"), {addr, tp}), b.getInt32(0)), FType::Bool};
            release(t2);
        } else if (n == "trim" || n == "upper" || n == "lower") {
            need(0);
            b.CreateCall(rt("finch_str_" + n), {out, addr});
            res = strRes();
        } else if (n == "replace") {
            need(2);
            Value_ a = argStr(0, "the text to replace"), c = argStr(1, "the new text");
            b.CreateCall(rt("finch_str_replace"), {out, addr, tmp(a.v), tmp(c.v)});
            release(a);
            release(c);
            res = strRes();
        } else if (n == "split") {
            need(1);
            Value_ sep = argStr(0, "the separator");
            FType at = FType::arrayOf(FType::Str);
            Value *ao = tmpOf(ty(at));
            b.CreateCall(rt("finch_str_split"), {ao, addr, tmp(sep.v)});
            release(sep);
            res = {b.CreateLoad(ty(at), ao), at, false, true};
        } else if (n == "repeat") {
            need(1);
            b.CreateCall(rt("finch_str_repeat"), {out, addr, argInt(0, "the count")});
            res = strRes();
        } else if (n == "bytes") {
            need(0);
            FType at = FType::arrayOf(FType::U8);
            Value *ao = tmpOf(ty(at));
            b.CreateCall(rt("finch_bytes_from_str"), {ao, addr});
            res = {b.CreateLoad(ty(at), ao), at, false, true};
        } else {
            failAt(p.file, p.line, p.col, "a str has no method '" + n + "' (it has: sub, find, contains, starts_with, ends_with, split, trim, upper, lower, replace, repeat, bytes)");
        }
        if (!r.isPlace) release(sv);
        return res;
    }
    if (t.kind == FType::Struct)
        failAt(p.file, p.line, p.col, "structs don't have methods; write a function and call it: " + m.name + "(value, ...)");
    failAt(p.file, p.line, p.col, t.show() + " has no methods");
}

// An element compare for find/contains/sort: -1, 0, 1 (only numbers, chars, bools and str)
static bool comparable(const FType &t) {
    return t.isNumber() || t.kind == FType::Char || t.kind == FType::Bool || t.kind == FType::Str;
}

Value_ Codegen::arrayMethod(const MethodExpr &m, Value_ *recv) {
    Pos p = m.pos;
    const std::string &n = m.name;
    FType at = recv->type, et = *at.elem;
    llvm::Type *AT = ty(at), *ET = ty(et);
    Value *a = recv->v;  // the array's address
    auto need = [&](size_t k) { checkArgs(m.args, m.argNames, k, p, n); };
    auto lenAddr = [&]() { return b.CreateStructGEP(AT, a, 1); };
    auto len = [&]() { return b.CreateLoad(b.getInt64Ty(), lenAddr()); };
    auto data = [&]() { return b.CreateLoad(b.getPtrTy(), b.CreateStructGEP(AT, a, 0)); };
    auto elemValue = [&](size_t i) { return own(coerce(exprWant(*m.args[i], et), et, m.args[i]->pos, "the element")); };

    if (n == "push") {
        need(1);
        Value *v = elemValue(0);
        Value *l = len();
        b.CreateCall(rt("finch_arr_reserve"), {a, b.CreateAdd(l, b.getInt64(1)), elemSize(et)});
        b.CreateStore(v, b.CreateInBoundsGEP(ET, data(), l));
        b.CreateStore(b.CreateAdd(l, b.getInt64(1)), lenAddr());
        return {nullptr, FType::Void};
    }
    if (n == "pop") {
        need(0);
        Value *l = len();
        panicIf(b.CreateICmpEQ(l, b.getInt64(0)), "pop() on an empty array", p);
        Value *last = b.CreateSub(l, b.getInt64(1));
        Value *v = b.CreateLoad(ET, b.CreateInBoundsGEP(ET, data(), last));
        b.CreateStore(last, lenAddr());
        return {v, et, false, owning(et)};
    }
    if (n == "insert") {
        need(2);
        Value *i = coerce(expr(*m.args[0]), FType::I64, m.args[0]->pos, "the position").v;
        Value *v = elemValue(1);
        b.CreateCall(rt("finch_arr_insert_gap"), {a, i, elemSize(et), fileName(p), b.getInt64(p.line)});
        b.CreateStore(v, b.CreateInBoundsGEP(ET, data(), i));
        return {nullptr, FType::Void};
    }
    if (n == "remove") {
        need(1);
        Value *i = coerce(expr(*m.args[0]), FType::I64, m.args[0]->pos, "the position").v;
        boundsCheck(i, len(), p);
        Value *v = b.CreateLoad(ET, b.CreateInBoundsGEP(ET, data(), i));
        b.CreateCall(rt("finch_arr_remove_gap"), {a, i, elemSize(et)});
        return {v, et, false, owning(et)};
    }
    if (n == "clear" || n == "resize") {
        need(n == "clear" ? 0 : 1);
        Value *nl = n == "clear" ? (Value *)b.getInt64(0) : coerce(expr(*m.args[0]), FType::I64, m.args[0]->pos, "the new length").v;
        if (owning(et)) {  // drop the elements that go away
            Value *l = len(), *d = data();
            Value *from = b.CreateSelect(b.CreateICmpSLT(nl, l), b.CreateSelect(b.CreateICmpSLT(nl, b.getInt64(0)), b.getInt64(0), nl), l);
            forN(b.CreateSub(l, from), [&](Value *k) { dropAt(b.CreateInBoundsGEP(ET, d, b.CreateAdd(from, k)), et); });
        }
        b.CreateCall(rt("finch_arr_resize"), {a, nl, elemSize(et)});
        return {nullptr, FType::Void};
    }
    if (n == "find" || n == "contains") {
        need(1);
        if (!comparable(et)) failAt(p.file, p.line, p.col, n + "() works on arrays of numbers, chars, bools or str");
        Value_ x = coerce(exprWant(*m.args[0], et), et, m.args[0]->pos, "the value to look for");
        Value *xv = x.v;
        Value *res = tmpOf(b.getInt64Ty());
        b.CreateStore(b.getInt64(-1), res);
        Value *l = len(), *d = data();
        Value *i = tmpOf(b.getInt64Ty());
        b.CreateStore(b.getInt64(0), i);
        BasicBlock *cond = newBlock("find.cond"), *body = newBlock("find.body"), *hit = newBlock("find.hit"),
                   *next = newBlock("find.next"), *end = newBlock("find.end");
        b.CreateBr(cond);
        b.SetInsertPoint(cond);
        Value *iv = b.CreateLoad(b.getInt64Ty(), i);
        b.CreateCondBr(b.CreateICmpSLT(iv, l), body, end);
        b.SetInsertPoint(body);
        Value_ ev{b.CreateLoad(ET, b.CreateInBoundsGEP(ET, d, iv)), et};
        Value *eq = compare(BinOp::Eq, ev, {xv, et}, p).v;
        b.CreateCondBr(eq, hit, next);
        b.SetInsertPoint(hit);
        b.CreateStore(iv, res);
        b.CreateBr(end);
        b.SetInsertPoint(next);
        b.CreateStore(b.CreateAdd(iv, b.getInt64(1)), i);
        b.CreateBr(cond);
        b.SetInsertPoint(end);
        release(x);
        Value *r = b.CreateLoad(b.getInt64Ty(), res);
        if (n == "find") return {r, FType::I64};
        return {b.CreateICmpSGE(r, b.getInt64(0)), FType::Bool};
    }
    if (n == "join") {
        need(1);
        if (et.kind != FType::Str) failAt(p.file, p.line, p.col, "join() works on an array of str");
        Value_ sep = coerce(expr(*m.args[0]), FType::Str, m.args[0]->pos, "the separator");
        Value *out = tmpOf(ty(FType::Str));
        b.CreateCall(rt("finch_str_join"), {out, a, tmp(sep.v)});
        release(sep);
        return {b.CreateLoad(ty(FType::Str), out), FType::Str, false, true};
    }
    if (n == "slice") {
        need(2);
        Value *s = coerce(expr(*m.args[0]), FType::I64, m.args[0]->pos, "the start").v;
        Value *e = coerce(expr(*m.args[1]), FType::I64, m.args[1]->pos, "the end").v;
        Value *l = len();
        panicIf(b.CreateOr(b.CreateOr(b.CreateICmpSLT(s, b.getInt64(0)), b.CreateICmpSGT(e, l)), b.CreateICmpSGT(s, e)),
                "slice(start, end) is out of range", p);
        Value *cnt = b.CreateSub(e, s);
        Value *out = tmpOf(AT);
        b.CreateCall(rt("finch_arr_make"), {out, cnt, elemSize(et)});
        Value *src = data();
        Value *dst = b.CreateLoad(b.getPtrTy(), b.CreateStructGEP(AT, out, 0));
        forN(cnt, [&](Value *k) {
            copyAt(b.CreateInBoundsGEP(ET, dst, k), b.CreateInBoundsGEP(ET, src, b.CreateAdd(s, k)), et);
        });
        return {b.CreateLoad(AT, out), at, false, true};
    }
    if (n == "reverse") {
        need(0);
        Value *l = len(), *d = data();
        forN(b.CreateSDiv(l, b.getInt64(2)), [&](Value *k) {
            Value *x = b.CreateInBoundsGEP(ET, d, k);
            Value *y = b.CreateInBoundsGEP(ET, d, b.CreateSub(b.CreateSub(l, b.getInt64(1)), k));
            Value *xv = b.CreateLoad(ET, x), *yv = b.CreateLoad(ET, y);
            b.CreateStore(yv, x);
            b.CreateStore(xv, y);
        });
        return {nullptr, FType::Void};
    }
    if (n == "sort") {
        need(0);
        if (!comparable(et) || et.kind == FType::Bool) failAt(p.file, p.line, p.col, "sort() works on arrays of numbers, chars or str");
        std::string key = "cmp." + typeKey(et);
        Function *cmp = helpers[key];
        if (!cmp) {
            cmp = Function::Create(FunctionType::get(b.getInt32Ty(), {b.getPtrTy(), b.getPtrTy()}, false),
                                   Function::InternalLinkage, "finch." + key, mod.get());
            helpers[key] = cmp;
            IRBuilderBase::InsertPointGuard g(b);
            DISubprogram *savedFn = diFn;
            diFn = nullptr;
            b.SetInsertPoint(BasicBlock::Create(ctx, "entry", cmp));
            b.SetCurrentDebugLocation(DebugLoc());
            Value *x = cmp->getArg(0), *y = cmp->getArg(1);
            Value *r;
            if (et.kind == FType::Str) {
                r = b.CreateTrunc(b.CreateCall(rt("finch_str_cmp"), {x, y}), b.getInt32Ty());
            } else {
                Value_ xv{b.CreateLoad(ET, x), et}, yv{b.CreateLoad(ET, y), et};
                Value *lt = compare(BinOp::Less, xv, yv, p).v, *gt = compare(BinOp::Greater, xv, yv, p).v;
                r = b.CreateSub(b.CreateZExt(gt, b.getInt32Ty()), b.CreateZExt(lt, b.getInt32Ty()));
            }
            b.CreateRet(r);
            diFn = savedFn;
        }
        b.CreateCall(libc("qsort", b.getVoidTy(), {b.getPtrTy(), b.getInt64Ty(), b.getInt64Ty(), b.getPtrTy()}),
                     {data(), len(), elemSize(et), cmp});
        return {nullptr, FType::Void};
    }
    failAt(p.file, p.line, p.col, "an array has no method '" + n + "' (it has: push, pop, insert, remove, clear, resize, find, contains, join, slice, reverse, sort)");
}

}  // namespace finch
