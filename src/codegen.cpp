// Code generation, part 1: the program, statements, scopes and expressions.
// Types, ownership and printing live in codegen_types.cpp; calls and built-ins in
// codegen_builtins.cpp; the C calling convention in abi.cpp.

#include "codegen_impl.h"
#include "builtins_doc.h"

using namespace llvm;

namespace finch {

Codegen::Codegen(LLVMContext &c, const std::vector<Program> &ps, const CImports &ci, TargetMachine &tm, bool dbg)
    : ctx(c), b(c), mod(std::make_unique<Module>("finch", c)), progs(ps), cimports(ci), debug(dbg) {
    mod->setTargetTriple(tm.getTargetTriple());
    mod->setDataLayout(tm.createDataLayout());
    dl = &mod->getDataLayout();
    scopes.reserve(256);
    if (debug) {
        di = std::make_unique<DIBuilder>(*mod);
        for (const SourceFile &f : g_files) {
            std::string path = f.path;
            size_t slash = path.find_last_of('/');
            std::string dir = slash == std::string::npos ? "." : path.substr(0, slash);
            std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
            diFiles.push_back(di->createFile(name, dir));
        }
        diUnit = di->createCompileUnit(dwarf::DW_LANG_C, diFiles[0], "finch " FINCH_VERSION, false, "", 0);
        mod->addModuleFlag(Module::Warning, "Debug Info Version", DEBUG_METADATA_VERSION);
        if (tm.getTargetTriple().isWindowsMSVCEnvironment())
            mod->addModuleFlag(Module::Warning, "CodeView", 1);  // what Visual Studio and WinDbg read
        else
            mod->addModuleFlag(Module::Warning, "Dwarf Version", 5);
    }
}

std::unique_ptr<Module> Codegen::run() {
    declareStructs();
    declareFns();
    if (!modules[""].fns.count("main")) failAt(0, 1, 1, "there is no main function (add 'fn main() { ... }')");
    for (auto &[name, m] : modules)
        for (auto &[fname, fn] : m.fns) define(fn);
    defineMainWrapper();
    if (di) di->finalize();

    std::string err;
    raw_string_ostream os(err);
    if (verifyModule(*mod, &os)) {
        std::fprintf(stderr, "internal compiler error (please report):\n%s\n", err.c_str());
        std::exit(2);
    }
    return std::move(mod);
}

// ---------- declarations ----------

void Codegen::declareStructs() {
    for (const Program &p : progs) {
        ModuleScope &m = modules[p.module];
        for (const Import &im : p.imports)
            if (!im.isC) m.imports.insert(im.path);
        for (const StructDecl &d : p.structs) {
            if (m.structs.count(d.name)) failAt(d.pos.file, d.pos.line, d.pos.col, "the struct '" + d.name + "' is defined twice");
            auto s = std::make_unique<StructInfo>();
            s->name = d.name;
            s->module = p.module;
            s->decl = &d;
            s->pos = d.pos;
            s->llvm = StructType::create(ctx, "finch." + (p.module.empty() ? "" : p.module + ".") + d.name);
            m.structs[d.name] = s.get();
            structStore.push_back(std::move(s));
        }
    }
    for (const auto &[name, cs] : cimports.structs) {
        auto s = std::make_unique<StructInfo>();
        s->name = name;
        s->module = "C";
        s->isC = true;
        s->cdecl = &cs;
        s->unsupported = cs.unsupported;
        s->llvm = StructType::create(ctx, "c." + name);
        cStructs[name] = s.get();
        structStore.push_back(std::move(s));
    }
    for (auto &s : structStore) resolveStruct(s.get());
    if (g_index)
        for (auto &s : structStore) {
            StructIndex si{s->module, s->name, {}, -1, 0, 0};
            if (s->decl) {
                si.file = s->decl->namePos.file;
                si.line = s->decl->namePos.line;
                si.col = s->decl->namePos.col;
                note(s->decl->namePos, s->name.size(), structHover(s.get()), s->decl->namePos);
            }
            for (auto &f : s->fields) {
                si.fields.push_back({f.name, f.type.show()});
                if (s->decl) note(f.pos, f.name.size(), "```finch\n" + f.type.show() + " " + f.name + "\n```\nfield of `" + s->name + "`", f.pos);
            }
            g_index->structs.push_back(si);
        }
}

void Codegen::declareFns() {
    for (const Program &p : progs)
        for (const FnDecl &fn : p.fns) declare(p, fn);
}

void Codegen::declare(const Program &p, const FnDecl &fn) {
    ModuleScope &m = modules[p.module];
    Pos pos = fn.pos;
    if (m.fns.count(fn.name)) failAt(pos.file, pos.line, pos.col, "function '" + fn.name + "' is defined twice");
    if (isBuiltin(fn.name)) failAt(pos.file, pos.line, pos.col, "'" + fn.name + "' is a built-in function, pick another name");
    if (m.structs.count(fn.name)) failAt(pos.file, pos.line, pos.col, "'" + fn.name + "' is already the name of a struct");

    curModule = p.module;
    Fn f;
    f.decl = &fn;
    f.module = p.module;
    f.ret = resolve(fn.ret, pos);
    for (const Param &prm : fn.params) f.params.push_back(resolve(prm.type, prm.pos));

    bool isMain = p.module.empty() && fn.name == "main";
    if (isMain) {
        bool argsOk = f.params.empty() || (f.params.size() == 1 && f.params[0] == FType::arrayOf(FType::Str));
        if (!argsOk) failAt(pos.file, pos.line, pos.col, "main takes no parameters, or the program's arguments: fn main([]str args)");
        FType::Kind r = f.ret.kind;
        if (r != FType::Void && r != FType::I64 && r != FType::I32)
            failAt(pos.file, pos.line, pos.col, "main returns nothing (fn main()) or an exit code (fn main() -> int)");
    }

    std::vector<llvm::Type *> params;
    for (size_t i = 0; i < f.params.size(); i++) {
        params.push_back(ty(f.params[i]));
        f.borrowParam.push_back(owning(f.params[i]) && !mutates(fn.params[i].name, *fn.body));
    }
    // User functions get a prefix so they never clash with C library names.
    std::string name = "finch." + (p.module.empty() ? "" : p.module + ".") + fn.name;
    f.llvm = Function::Create(FunctionType::get(ty(f.ret), params, false), Function::InternalLinkage, name, mod.get());
    m.fns[fn.name] = f;
}

// The real C `main` calls the Finch one and turns its result into an exit code.
void Codegen::defineMainWrapper() {
    Fn &fm = modules[""].fns["main"];
    Function *w = Function::Create(FunctionType::get(b.getInt32Ty(), {b.getInt32Ty(), b.getPtrTy()}, false),
                                   Function::ExternalLinkage, "main", mod.get());
    b.SetInsertPoint(BasicBlock::Create(ctx, "entry", w));
    b.SetCurrentDebugLocation(DebugLoc());
    std::vector<Value *> args;
    Value *argsArr = nullptr;
    if (!fm.params.empty()) {
        Value *out = tmp(Constant::getNullValue(ty(fm.params[0])));
        b.CreateCall(rt("finch_args"), {out, w->getArg(0), w->getArg(1)});
        argsArr = b.CreateLoad(ty(fm.params[0]), out);
        args.push_back(argsArr);
    }
    Value *r = b.CreateCall(fm.llvm, args);
    if (argsArr) dropValue(argsArr, fm.params[0]);
    if (fm.ret.kind == FType::Void) b.CreateRet(b.getInt32(0));
    else b.CreateRet(b.CreateIntCast(r, b.getInt32Ty(), true));
}

void Codegen::define(Fn &fn) {
    const FnDecl &d = *fn.decl;
    curFn = &fn;
    curModule = fn.module;
    if (g_index) {
        FnInfo fi{fn.module, d.name, signature(fn), "", {}, d.namePos.file, d.namePos.line, d.namePos.col, d.body->end.line};
        for (size_t i = 0; i < fn.params.size(); i++) fi.params.push_back(fn.params[i].show() + " " + d.params[i].name);
        g_index->fns.push_back(fi);
        note(d.namePos, d.name.size(), "```finch\n" + signature(fn) + "\n```", d.namePos);
    }
    Function *f = fn.llvm;
    b.SetInsertPoint(BasicBlock::Create(ctx, "entry", f));
    if (di) {
        DIFile *file = diFiles[d.pos.file];
        std::vector<Metadata *> types{diType(fn.ret)};
        for (const FType &t : fn.params) types.push_back(diType(t));
        diFn = di->createFunction(file, d.name, f->getName(), file, d.pos.line,
                                  di->createSubroutineType(di->getOrCreateTypeArray(types)), d.pos.line,
                                  DINode::FlagPrototyped, DISubprogram::SPFlagDefinition);
        f->setSubprogram(diFn);
    }
    setLoc(d.pos);

    scopes.clear();
    varOrder = 0;
    pushScope();
    unsigned i = 0;
    for (Argument &arg : f->args()) {
        const Param &p = d.params[i];
        const FType &t = fn.params[i];
        arg.setName(p.name);
        if (owning(t) && !fn.borrowParam[i]) addVar(p.namePos, p.name, t, copyValue(&arg, t), true, false, i + 1);  // changed inside: own a copy
        else addVar(p.namePos, p.name, t, &arg, false, false, i + 1);
        i++;
    }
    blockBody(*d.body);
    if (!terminated()) {
        if (fn.ret.kind != FType::Void)
            failAt(d.pos.file, d.pos.line, d.pos.col, "function '" + d.name + "' can reach its end without returning " + fn.ret.show());
        emitCleanups(0);
        b.CreateRetVoid();
    }
    scopes.clear();
    diFn = nullptr;
}

// ---------- scopes ----------

void Codegen::pushScope() { scopes.emplace_back(); }

void Codegen::popScope() {
    if (!terminated()) emitScopeCleanups(scopes.size() - 1);
    scopes.pop_back();
}

void Codegen::emitCleanups(size_t downTo) {
    for (size_t i = scopes.size(); i-- > downTo;) emitScopeCleanups(i);
}

// Drop the scope's variables and run its defers, newest first.
void Codegen::emitScopeCleanups(size_t idx) {
    for (size_t k = scopes[idx].cleanups.size(); k-- > 0;) {
        Cleanup c = scopes[idx].cleanups[k];  // a copy: emitting a defer may grow `scopes`
        if (!c.isDefer) {
            Var v = scopes[idx].vars.at(c.var);
            if (!v.moved) dropAt(v.slot, v.type);
            continue;
        }
        int savedLimit = deferLimit;
        deferLimit = c.order;
        inDefer++;
        pushScope();
        stmt(*c.body);
        popScope();
        inDefer--;
        deferLimit = savedLimit;
    }
}

bool Codegen::terminated() { return b.GetInsertBlock()->getTerminator() != nullptr; }

BasicBlock *Codegen::newBlock(const char *name) {
    return BasicBlock::Create(ctx, name, b.GetInsertBlock()->getParent());
}

// Variables live in stack slots created at the top of the function; LLVM turns them into registers.
AllocaInst *Codegen::slot(const FType &t, const std::string &name) {
    Function *f = b.GetInsertBlock()->getParent();
    IRBuilder<> entry(&f->getEntryBlock(), f->getEntryBlock().begin());
    AllocaInst *a = entry.CreateAlloca(ty(t), nullptr, name);
    if (t.kind == FType::Struct && t.info->isC) a->setAlignment(Align(t.info->cdecl->align));
    return a;
}

Var *Codegen::lookup(const std::string &name) {
    for (size_t i = scopes.size(); i-- > 0;) {
        auto found = scopes[i].vars.find(name);
        if (found == scopes[i].vars.end()) continue;
        if (deferLimit >= 0 && found->second.order > deferLimit) continue;  // declared after the defer
        return &found->second;
    }
    return nullptr;
}

Var &Codegen::addVar(Pos p, const std::string &name, const FType &t, Value *init, bool owned, bool readonly,
                     unsigned argNo) {
    if (lookup(name)) failAt(p.file, p.line, p.col, "a variable named '" + name + "' already exists here");
    ModuleScope &m = modules[curModule];
    if (m.fns.count(name)) failAt(p.file, p.line, p.col, "'" + name + "' is already the name of a function");
    if (m.imports.count(name)) failAt(p.file, p.line, p.col, "'" + name + "' is already the name of an imported module");
    if (isBuiltin(name)) failAt(p.file, p.line, p.col, "'" + name + "' is a built-in name, pick another one");
    AllocaInst *s = slot(t, name);
    b.CreateStore(init, s);
    Var v;
    v.slot = s;
    v.type = t;
    v.readonly = readonly;
    v.owned = owned && owning(t);
    v.order = ++varOrder;
    v.pos = p;
    if (v.owned) scopes.back().cleanups.push_back({false, name});
    if (g_index && name[0] != '$' && curFn) {
        const FnDecl &d = *curFn->decl;
        g_index->vars.push_back({name, t.show(), p.file, p.line, p.col, d.pos.file, d.pos.line, d.body->end.line});
        note(p, name.size(), "```finch\n" + t.show() + " " + name + "\n```", p);
    }
    if (di && name[0] != '$') {
        DILocalVariable *dv = argNo ? di->createParameterVariable(diFn, name, argNo, diFiles[p.file], p.line, diType(t))
                                    : di->createAutoVariable(diFn, name, diFiles[p.file], p.line, diType(t));
        di->insertDeclare(s, dv, di->createExpression(), DILocation::get(ctx, p.line, p.col, diFn), b.GetInsertBlock());
    }
    return scopes.back().vars[name] = v;
}

// ---------- statements ----------

void Codegen::block(const BlockStmt &blk) {
    pushScope();
    blockBody(blk);
    popScope();
}

void Codegen::blockBody(const BlockStmt &blk) {
    for (const StmtPtr &s : blk.body) {
        if (terminated())
            failAt(s->pos.file, s->pos.line, s->pos.col, "this code can never run (it comes after return, break or continue)");
        stmt(*s);
    }
}

void Codegen::stmt(const Stmt &s) {
    setLoc(s.pos);
    switch (s.kind) {
    case StmtKind::Block: block(static_cast<const BlockStmt &>(s)); break;
    case StmtKind::VarDecl: varDecl(static_cast<const VarDeclStmt &>(s)); break;
    case StmtKind::Assign: assign(static_cast<const AssignStmt &>(s)); break;
    case StmtKind::Expr: release(expr(*static_cast<const ExprStmt &>(s).expr)); break;
    case StmtKind::If: ifStmt(static_cast<const IfStmt &>(s)); break;
    case StmtKind::While: whileStmt(static_cast<const WhileStmt &>(s)); break;
    case StmtKind::For: forStmt(static_cast<const ForStmt &>(s)); break;
    case StmtKind::ForEach: forEachStmt(static_cast<const ForEachStmt &>(s)); break;
    case StmtKind::Return: returnStmt(static_cast<const ReturnStmt &>(s)); break;
    case StmtKind::Break: jump(true, s.pos); break;
    case StmtKind::Continue: jump(false, s.pos); break;
    case StmtKind::Defer: {
        auto &d = static_cast<const DeferStmt &>(s);
        scopes.back().cleanups.push_back({true, "", d.body.get(), varOrder});
        break;
    }
    }
}

void Codegen::jump(bool isBreak, Pos p) {
    const char *word = isBreak ? "break" : "continue";
    if (inDefer) failAt(p.file, p.line, p.col, std::string(word) + " can't be used inside defer");
    if (loops.empty()) failAt(p.file, p.line, p.col, std::string(word) + " can only be used inside a loop");
    Loop l = loops.back();
    emitCleanups(l.scopeDepth);
    b.CreateBr(isBreak ? l.breakTo : l.continueTo);
}

void Codegen::varDecl(const VarDeclStmt &s) {
    Pos p = s.pos;
    if (!s.hasType) {
        Value_ v = expr(*s.init);
        Pos ip = s.init->pos;
        if (v.type.kind == FType::Void) failAt(ip.file, ip.line, ip.col, "this gives back nothing, so it can't be stored");
        if (v.type.kind == FType::Null) failAt(ip.file, ip.line, ip.col, "null needs a type, like: ptr[int] " + s.name + " = null");
        addVar(s.namePos, s.name, v.type, own(v), true);
        return;
    }
    FType t = resolve(s.type, p);
    if (t.kind == FType::Void) failAt(p.file, p.line, p.col, "a variable can't have the type 'nothing'");
    Value_ v = s.init ? coerce(exprWant(*s.init, t), t, s.init->pos, "'" + s.name + "'") : defaultValue(t, p);
    addVar(s.namePos, s.name, t, own(v), true);
}

void Codegen::assign(const AssignStmt &s) {
    std::string root = rootVar(*s.target);
    if (!root.empty()) {
        Var *v = lookup(root);
        if (v && v->readonly) failAt(s.pos.file, s.pos.line, s.pos.col, v->readonlyWhy);
        if (!v && s.target->kind == ExprKind::Var && !cimports.globals.count(root))
            failAt(s.pos.file, s.pos.line, s.pos.col, "there is no variable named '" + root + "' (create it with " + root + " := ...)");
    }
    std::string what = s.target->kind == ExprKind::Var ? "'" + root + "'" : "the value";

    if (s.op == '=') {
        // The right side first: it may move memory the target lives in (a.push(...) inside it).
        Value_ v;
        bool known = false;
        FType want;
        if (s.target->kind == ExprKind::Var) {
            if (Var *var = lookup(root)) { want = var->type; known = true; }
        }
        v = known ? exprWant(*s.value, want) : expr(*s.value);
        Place dst = place(*s.target, true);
        v = coerce(v, dst.type, s.value->pos, what);
        Value *nv = own(v);
        if (owning(dst.type)) {
            Value *old = b.CreateLoad(ty(dst.type), dst.addr);
            b.CreateStore(nv, dst.addr);
            dropValue(old, dst.type);
        } else {
            b.CreateStore(nv, dst.addr);
        }
        return;
    }
    BinOp op = s.op == '+' ? BinOp::Add : s.op == '-' ? BinOp::Sub : s.op == '*' ? BinOp::Mul
             : s.op == '/' ? BinOp::Div : BinOp::Mod;
    Value_ rhs = expr(*s.value);
    Place dst = place(*s.target, true);
    Value_ cur{b.CreateLoad(ty(dst.type), dst.addr), dst.type};
    Value_ v = coerce(arith(op, cur, rhs, s.pos), dst.type, s.pos, what);
    Value *nv = own(v);
    if (owning(dst.type)) {
        b.CreateStore(nv, dst.addr);
        dropValue(cur.v, dst.type);
    } else {
        b.CreateStore(nv, dst.addr);
    }
}

Value *Codegen::condition(const Expr &e) { return coerce(expr(e), FType::Bool, e.pos, "a condition").v; }

void Codegen::ifStmt(const IfStmt &s) {
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
void Codegen::continueAt(BasicBlock *bb, BasicBlock *deadEnd) {
    if (pred_empty(bb)) {
        bb->eraseFromParent();
        b.SetInsertPoint(deadEnd);
    } else {
        b.SetInsertPoint(bb);
    }
}

void Codegen::whileStmt(const WhileStmt &s) {
    BasicBlock *condBB = newBlock("while.cond");
    BasicBlock *body = newBlock("while.body");
    BasicBlock *end = newBlock("while.end");
    b.CreateBr(condBB);

    b.SetInsertPoint(condBB);
    Value *cond = condition(*s.cond);
    if (auto *c = dyn_cast<ConstantInt>(cond); c && c->isOne()) b.CreateBr(body);  // while true
    else b.CreateCondBr(cond, body, end);

    b.SetInsertPoint(body);
    loops.push_back({condBB, end, scopes.size()});
    block(*s.body);
    loops.pop_back();
    if (!terminated()) b.CreateBr(condBB);
    continueAt(end, b.GetInsertBlock());
}

void Codegen::forStmt(const ForStmt &s) {
    Value *from = coerce(expr(*s.from), FType::I64, s.from->pos, "the start of a range").v;
    Value *to = coerce(expr(*s.to), FType::I64, s.to->pos, "the end of a range").v;

    pushScope();
    Var &iv = addVar(s.varPos, s.var, FType::I64, from, false, true);
    iv.readonlyWhy = "the loop variable '" + s.var + "' can't be changed";
    Value *i = iv.slot;

    BasicBlock *condBB = newBlock("for.cond");
    BasicBlock *body = newBlock("for.body");
    BasicBlock *step = newBlock("for.step");
    BasicBlock *end = newBlock("for.end");
    b.CreateBr(condBB);

    b.SetInsertPoint(condBB);
    b.CreateCondBr(b.CreateICmpSLT(b.CreateLoad(b.getInt64Ty(), i), to), body, end);

    b.SetInsertPoint(body);
    loops.push_back({step, end, scopes.size()});
    block(*s.body);
    loops.pop_back();
    if (!terminated()) b.CreateBr(step);

    b.SetInsertPoint(step);
    b.CreateStore(b.CreateAdd(b.CreateLoad(b.getInt64Ty(), i), b.getInt64(1)), i);
    b.CreateBr(condBB);

    b.SetInsertPoint(end);
    popScope();
}

// for x in list: x is each element (or each char of a str)
void Codegen::forEachStmt(const ForEachStmt &s) {
    pushScope();
    LRef r = ref(*s.list);
    Value *addr;
    FType lt;
    bool live = r.isPlace;  // iterate the variable itself: re-read its length every round
    if (r.isPlace) {
        addr = r.pl.addr;
        lt = r.pl.type;
    } else {
        lt = r.val.type;
        addr = addVar(s.pos, "$list" + std::to_string(varOrder), lt, own(r.val), true).slot;
    }
    if (lt.kind != FType::Array && lt.kind != FType::Str && lt.kind != FType::Fixed)
        failAt(s.list->pos.file, s.list->pos.line, s.list->pos.col,
               "for ... in needs an array or a str, but this is " + lt.show() + " (for a range of numbers write 0..n)");
    FType et = lt.kind == FType::Str ? FType(FType::Char) : *lt.elem;
    // If the loop changes the list, each element is copied, so it can't disappear under us.
    bool copyElems = live && owning(et) && mutates(rootVar(*s.list), *s.body);

    Value *i = slot(FType::I64, "$i");
    b.CreateStore(b.getInt64(0), i);
    BasicBlock *condBB = newBlock("each.cond");
    BasicBlock *body = newBlock("each.body");
    BasicBlock *step = newBlock("each.step");
    BasicBlock *end = newBlock("each.end");
    b.CreateBr(condBB);

    b.SetInsertPoint(condBB);
    Value *len = lt.kind == FType::Fixed ? (Value *)b.getInt64(lt.count) : b.CreateLoad(b.getInt64Ty(), b.CreateStructGEP(ty(lt), addr, 1));
    Value *iv = b.CreateLoad(b.getInt64Ty(), i);
    b.CreateCondBr(b.CreateICmpSLT(iv, len), body, end);

    b.SetInsertPoint(body);
    Value *elemAddr;
    if (lt.kind == FType::Fixed) {
        elemAddr = b.CreateInBoundsGEP(ty(lt), addr, {b.getInt64(0), iv});
    } else {
        Value *data = b.CreateLoad(b.getPtrTy(), b.CreateStructGEP(ty(lt), addr, 0));
        elemAddr = b.CreateInBoundsGEP(ty(et), data, iv);
    }
    Value *elem = b.CreateLoad(ty(et), elemAddr);
    loops.push_back({step, end, scopes.size()});
    pushScope();
    Var &xv = addVar(s.varPos, s.var, et, copyElems ? copyValue(elem, et) : elem, copyElems, true);
    xv.readonlyWhy = "the loop variable '" + s.var + "' can't be changed (to change the list, loop with for i in 0..list.len and use list[i])";
    block(*s.body);
    popScope();
    loops.pop_back();
    if (!terminated()) b.CreateBr(step);

    b.SetInsertPoint(step);
    b.CreateStore(b.CreateAdd(b.CreateLoad(b.getInt64Ty(), i), b.getInt64(1)), i);
    b.CreateBr(condBB);

    b.SetInsertPoint(end);
    popScope();
}

void Codegen::returnStmt(const ReturnStmt &s) {
    const FnDecl &d = *curFn->decl;
    if (inDefer) failAt(s.pos.file, s.pos.line, s.pos.col, "return can't be used inside defer");
    if (curFn->ret.kind == FType::Void) {
        if (s.value)
            failAt(s.value->pos.file, s.value->pos.line, s.value->pos.col, "'" + d.name + "' doesn't return a value (it has no '-> type')");
        emitCleanups(0);
        b.CreateRetVoid();
        return;
    }
    if (!s.value) failAt(s.pos.file, s.pos.line, s.pos.col, "'" + d.name + "' must return " + curFn->ret.show());

    // `return local` moves the variable out instead of copying it.
    if (s.value->kind == ExprKind::Var) {
        const std::string &name = static_cast<const VarExpr &>(*s.value).name;
        for (size_t idx = scopes.size(); idx-- > 0;) {
            auto it = scopes[idx].vars.find(name);
            if (it == scopes[idx].vars.end()) continue;
            if (it->second.owned && it->second.type == curFn->ret) {
                Value *v = b.CreateLoad(ty(it->second.type), it->second.slot);
                it->second.moved = true;
                emitCleanups(0);
                scopes[idx].vars[name].moved = false;
                b.CreateRet(v);
                return;
            }
            break;
        }
    }
    Value_ v = coerce(exprWant(*s.value, curFn->ret), curFn->ret, s.value->pos, "the returned value");
    Value *out = own(v);
    emitCleanups(0);
    b.CreateRet(out);
}

// ---------- mutation analysis (does a function or loop change a variable?) ----------

std::string Codegen::rootVar(const Expr &e) {
    switch (e.kind) {
    case ExprKind::Var: return static_cast<const VarExpr &>(e).name;
    case ExprKind::Member: return rootVar(*static_cast<const MemberExpr &>(e).obj);
    case ExprKind::Index: return rootVar(*static_cast<const IndexExpr &>(e).obj);
    default: return "";
    }
}

bool Codegen::mutatesExpr(const std::string &name, const Expr &e) {
    static const std::set<std::string> changing = {"push", "pop", "insert", "remove", "clear", "resize", "sort", "reverse"};
    switch (e.kind) {
    case ExprKind::Unary: return mutatesExpr(name, *static_cast<const UnaryExpr &>(e).operand);
    case ExprKind::Binary: {
        auto &x = static_cast<const BinaryExpr &>(e);
        return mutatesExpr(name, *x.lhs) || mutatesExpr(name, *x.rhs);
    }
    case ExprKind::Call: {
        auto &c = static_cast<const CallExpr &>(e);
        if (c.callee == "addr" && c.args.size() == 1 && rootVar(*c.args[0]) == name) return true;
        for (auto &a : c.args)
            if (mutatesExpr(name, *a)) return true;
        return false;
    }
    case ExprKind::Member: return mutatesExpr(name, *static_cast<const MemberExpr &>(e).obj);
    case ExprKind::Index: {
        auto &x = static_cast<const IndexExpr &>(e);
        return mutatesExpr(name, *x.obj) || mutatesExpr(name, *x.index);
    }
    case ExprKind::ArrayLit:
        for (auto &a : static_cast<const ArrayLitExpr &>(e).elems)
            if (mutatesExpr(name, *a)) return true;
        return false;
    case ExprKind::Method: {
        auto &m = static_cast<const MethodExpr &>(e);
        if (changing.count(m.name) && rootVar(*m.obj) == name) return true;
        if (mutatesExpr(name, *m.obj)) return true;
        for (auto &a : m.args)
            if (mutatesExpr(name, *a)) return true;
        return false;
    }
    default: return false;
    }
}

bool Codegen::mutates(const std::string &name, const Stmt &s) {
    auto e = [&](const ExprPtr &x) { return x && mutatesExpr(name, *x); };
    switch (s.kind) {
    case StmtKind::Block:
        for (auto &st : static_cast<const BlockStmt &>(s).body)
            if (mutates(name, *st)) return true;
        return false;
    case StmtKind::VarDecl: return e(static_cast<const VarDeclStmt &>(s).init);
    case StmtKind::Assign: {
        auto &a = static_cast<const AssignStmt &>(s);
        return rootVar(*a.target) == name || e(a.target) || e(a.value);
    }
    case StmtKind::Expr: return e(static_cast<const ExprStmt &>(s).expr);
    case StmtKind::If: {
        auto &x = static_cast<const IfStmt &>(s);
        return e(x.cond) || mutates(name, *x.then) || (x.otherwise && mutates(name, *x.otherwise));
    }
    case StmtKind::While: {
        auto &x = static_cast<const WhileStmt &>(s);
        return e(x.cond) || mutates(name, *x.body);
    }
    case StmtKind::For: {
        auto &x = static_cast<const ForStmt &>(s);
        return e(x.from) || e(x.to) || mutates(name, *x.body);
    }
    case StmtKind::ForEach: {
        auto &x = static_cast<const ForEachStmt &>(s);
        return e(x.list) || mutates(name, *x.body);
    }
    case StmtKind::Return: return e(static_cast<const ReturnStmt &>(s).value);
    case StmtKind::Defer: return mutates(name, *static_cast<const DeferStmt &>(s).body);
    default: return false;
    }
}

// ---------- expressions ----------

Value_ Codegen::exprWant(const Expr &e, const FType &want) {
    if (e.kind == ExprKind::ArrayLit) return arrayLit(static_cast<const ArrayLitExpr &>(e), &want);
    return expr(e);
}

Value_ Codegen::expr(const Expr &e) {
    switch (e.kind) {
    case ExprKind::Int: return {b.getInt64(static_cast<const IntExpr &>(e).value), FType::I64, true};
    case ExprKind::Float: return {ConstantFP::get(b.getDoubleTy(), static_cast<const FloatExpr &>(e).value), FType::F64, true};
    case ExprKind::Bool: return {b.getInt1(static_cast<const BoolExpr &>(e).value), FType::Bool};
    case ExprKind::Char: return {b.getInt8(static_cast<const CharExpr &>(e).value), FType::Char};
    case ExprKind::Str: return {strConst(static_cast<const StrExpr &>(e).value), FType::Str, false, true};
    case ExprKind::Null: return {ConstantPointerNull::get(b.getPtrTy()), FType::Null};
    case ExprKind::Var: {
        auto &v = static_cast<const VarExpr &>(e);
        if (!lookup(v.name)) {
            auto c = cimports.consts.find(v.name);
            if (c != cimports.consts.end()) {
                note(e.pos, v.name.size(), "C constant `" + v.name + " = " +
                     (c->second.type.isFloat() ? std::to_string(c->second.f) : std::to_string(c->second.i)) + "`");
                if (c->second.type.isFloat()) return {ConstantFP::get(b.getDoubleTy(), c->second.f), FType::F64, true};
                return {b.getInt64(c->second.i), FType::I64, true};
            }
            if (modules[curModule].imports.count(v.name))
                failAt(e.pos.file, e.pos.line, e.pos.col, "'" + v.name + "' is a module; use something from it, like " + v.name + ".name(...)");
            // C's standard streams are macros on some systems; the runtime knows them everywhere
            if (!cimports.globals.count(v.name) && (v.name == "stdin" || v.name == "stdout" || v.name == "stderr") &&
                cimports.fns.count("fprintf")) {
                int which = v.name == "stdin" ? 0 : v.name == "stdout" ? 1 : 2;
                return {b.CreateCall(rt("finch_std_stream"), {b.getInt32(which)}), FType::Ptr};
            }
        }
        Place pl = place(e);
        return {b.CreateLoad(ty(pl.type), pl.addr, v.name), pl.type};
    }
    case ExprKind::Member:
    case ExprKind::Index: {
        LRef r = ref(e);
        if (!r.isPlace) return r.val;
        return {b.CreateLoad(ty(r.pl.type), r.pl.addr), r.pl.type};
    }
    case ExprKind::Unary: return unary(static_cast<const UnaryExpr &>(e));
    case ExprKind::Binary: return binary(static_cast<const BinaryExpr &>(e));
    case ExprKind::Call: return call(static_cast<const CallExpr &>(e));
    case ExprKind::Method: return method(static_cast<const MethodExpr &>(e));
    case ExprKind::ArrayLit: return arrayLit(static_cast<const ArrayLitExpr &>(e), nullptr);
    }
    return {nullptr, FType::Void};
}

Value_ Codegen::unary(const UnaryExpr &u) {
    Pos p = u.pos;
    Value_ v = expr(*u.operand);
    if (u.op == '!') return {b.CreateNot(coerce(v, FType::Bool, u.operand->pos, "the value after '!'").v), FType::Bool};
    if (u.op == '~') {
        if (!v.type.isInt()) failAt(p.file, p.line, p.col, "'~' flips the bits of a whole number, not of " + v.type.show());
        return {b.CreateNot(v.v), v.type};
    }
    if (v.type.isSigned()) return {b.CreateNeg(v.v), v.type, v.literal};
    if (v.type.isFloat()) return {b.CreateFNeg(v.v), v.type, v.literal};
    if (v.type.isUnsigned()) failAt(p.file, p.line, p.col, v.type.show() + " can't be negative (it's unsigned)");
    failAt(p.file, p.line, p.col, "can't put '-' in front of " + v.type.show());
}

// An address for `e` if it has one (a variable, field, element, p.value); otherwise its value.
LRef Codegen::ref(const Expr &e, bool forWrite) {
    switch (e.kind) {
    case ExprKind::Var: {
        auto &v = static_cast<const VarExpr &>(e);
        if (Var *var = lookup(v.name)) {
            note(e.pos, v.name.size(), "```finch\n" + var->type.show() + " " + v.name + "\n```", var->pos);
            return {true, {var->slot, var->type}, {}};
        }
        if (cimports.globals.count(v.name)) return {true, {cGlobal(v.name), resolve(cimports.globals.at(v.name).type, e.pos)}, {}};
        return {false, {}, expr(e)};
    }
    case ExprKind::Member: return member(static_cast<const MemberExpr &>(e), forWrite);
    case ExprKind::Index: return index(static_cast<const IndexExpr &>(e), forWrite);
    default: return {false, {}, expr(e)};
    }
}

Place Codegen::place(const Expr &e, bool forWrite) {
    if (e.kind == ExprKind::Var) {
        auto &v = static_cast<const VarExpr &>(e);
        if (!lookup(v.name) && !cimports.globals.count(v.name))
            failAt(e.pos.file, e.pos.line, e.pos.col, "there is no variable named '" + v.name + "'");
    }
    LRef r = ref(e, forWrite);
    if (!r.isPlace) failAt(e.pos.file, e.pos.line, e.pos.col, "this is a temporary value, it can't be changed or pointed to");
    return r.pl;
}

LRef Codegen::member(const MemberExpr &m, bool forWrite) {
    Pos p = m.pos;
    if (m.obj->kind == ExprKind::Var) {
        const std::string &mn = static_cast<const VarExpr &>(*m.obj).name;
        if (!lookup(mn) && modules[curModule].imports.count(mn))
            failAt(p.file, p.line, p.col, "modules only have functions and structs; call it: " + mn + "." + m.field + "(...)");
    }
    LRef obj = ref(*m.obj, forWrite);
    FType t = obj.isPlace ? obj.pl.type : obj.val.type;

    // pointers: p.value is the target, p.field reaches through to a struct's field
    if (t.isPtr()) {
        Value *pv = obj.isPlace ? b.CreateLoad(b.getPtrTy(), obj.pl.addr) : obj.val.v;
        if (!t.elem) failAt(p.file, p.line, p.col, "this ptr has no type, so Finch doesn't know what's inside (use ptr[int] etc.)");
        if (!isa<AllocaInst>(pv) && !isa<GlobalValue>(pv))
            panicIf(b.CreateIsNull(pv), "used ." + m.field + " on a null pointer", p);
        // p.value is the target -- unless the target is a struct with its own field called `value`
        bool structHasValue = false;
        if (t.elem->kind == FType::Struct)
            for (auto &f : t.elem->info->fields) structHasValue |= f.name == "value" && !f.hidden;
        if (m.field == "value" && !structHasValue) return {true, {pv, *t.elem}, {}};
        if (t.elem->kind != FType::Struct)
            failAt(p.file, p.line, p.col, "a " + t.show() + " only has '.value' (the thing it points to)");
        obj = {true, {pv, *t.elem}, {}};
        t = *t.elem;
    }

    if (t.kind == FType::Array || t.kind == FType::Str || t.kind == FType::Fixed) {
        if (m.field == "len" || m.field == "ptr")
            note(Pos{p.line, p.col + 1, p.file}, m.field.size(),
                 m.field == "len" ? "`.len -> int`: the number of " + std::string(t.kind == FType::Str ? "bytes" : "elements")
                                  : "`.ptr`: the address of the first element, for C");
        if (m.field != "len" && m.field != "ptr")
            failAt(p.file, p.line, p.col, (t.kind == FType::Str ? "a str" : "an array") + std::string(" has .len and .ptr, not '.") + m.field + "'");
        if (forWrite) failAt(p.file, p.line, p.col, "." + m.field + " can't be changed directly" + (t.kind == FType::Array ? " (use resize, push or pop)" : ""));
        if (t.kind == FType::Fixed) {
            if (m.field == "len") return {false, {}, {b.getInt64(t.count), FType::I64}};
            if (!obj.isPlace) failAt(p.file, p.line, p.col, ".ptr of a temporary value would point to nothing");
            return {false, {}, {obj.pl.addr, FType::ptrTo(*t.elem)}};
        }
        Value *v = obj.isPlace ? b.CreateLoad(ty(t), obj.pl.addr) : obj.val.v;
        if (m.field == "len") {
            Value *len = b.CreateExtractValue(v, 1);
            if (!obj.isPlace) release(obj.val);
            return {false, {}, {len, FType::I64}};
        }
        // a literal lives as long as the program; other temporaries are freed right after this expression
        if (!obj.isPlace && obj.val.fresh && !isa<Constant>(obj.val.v))
            failAt(p.file, p.line, p.col, ".ptr of a temporary value would point to freed memory");
        FType pt = t.kind == FType::Str ? FType::ptrTo(FType::Char) : FType::ptrTo(*t.elem);
        return {false, {}, {t.kind == FType::Str ? cstr(v) : b.CreateExtractValue(v, 0), pt}};
    }

    if (t.kind != FType::Struct) failAt(p.file, p.line, p.col, t.show() + " has no fields (no '." + m.field + "')");
    StructInfo *s = t.info;
    int idx = -1;
    for (size_t k = 0; k < s->fields.size(); k++)
        if (s->fields[k].name == m.field && !s->fields[k].hidden) idx = (int)k;
    if (idx < 0) {
        std::string names;
        for (auto &f : s->fields)
            if (!f.hidden) names += (names.empty() ? "" : ", ") + f.name;
        failAt(p.file, p.line, p.col, "the struct " + t.show() + " has no field '" + m.field + "' (it has: " + names + ")");
    }
    const StructInfo::F &f = s->fields[idx];
    note(Pos{p.line, p.col + 1, p.file}, m.field.size(),
         "```finch\n" + f.type.show() + " " + m.field + "\n```\nfield of `" + s->name + "`", f.pos);
    unsigned llvmIdx = f.llvmIndex;
    if (obj.isPlace) return {true, {b.CreateStructGEP(s->llvm, obj.pl.addr, llvmIdx), f.type}, {}};
    // a field of a temporary struct: take it (copying if it owns memory), then drop the rest
    Value *fv = b.CreateExtractValue(obj.val.v, llvmIdx);
    Value_ out{fv, f.type};
    if (owning(f.type) && obj.val.fresh) out = {copyValue(fv, f.type), f.type, false, true};
    release(obj.val);
    return {false, {}, out};
}

LRef Codegen::index(const IndexExpr &e, bool forWrite) {
    Pos p = e.pos;
    LRef obj = ref(*e.obj, forWrite);
    FType t = obj.isPlace ? obj.pl.type : obj.val.type;
    Value *i = coerce(expr(*e.index), FType::I64, e.index->pos, "an index").v;

    if (t.isPtr()) {  // C-style: p[i], no bounds known
        if (!t.elem) failAt(p.file, p.line, p.col, "this ptr has no type, so Finch doesn't know what's inside (use ptr[int] etc.)");
        Value *pv = obj.isPlace ? b.CreateLoad(b.getPtrTy(), obj.pl.addr) : obj.val.v;
        panicIf(b.CreateIsNull(pv), "used [] on a null pointer", p);
        return {true, {b.CreateInBoundsGEP(ty(*t.elem), pv, i), *t.elem}, {}};
    }
    if (t.kind == FType::Fixed) {
        boundsCheck(i, b.getInt64(t.count), p);
        if (obj.isPlace) return {true, {b.CreateInBoundsGEP(ty(t), obj.pl.addr, {b.getInt64(0), i}), *t.elem}, {}};
        Value *a = tmp(obj.val.v);
        return {false, {}, {b.CreateLoad(ty(*t.elem), b.CreateInBoundsGEP(ty(t), a, {b.getInt64(0), i})), *t.elem}};
    }
    if (t.kind != FType::Array && t.kind != FType::Str)
        failAt(p.file, p.line, p.col, "[ ] works on arrays, str and pointers, not on " + t.show());
    FType et = t.kind == FType::Str ? FType(FType::Char) : *t.elem;
    if (t.kind == FType::Str && forWrite) {
        if (!obj.isPlace) failAt(p.file, p.line, p.col, "this is a temporary str, it can't be changed");
        b.CreateCall(rt("finch_str_own"), {obj.pl.addr});  // literals live in read-only memory: copy first
    }
    Value *v = obj.isPlace ? b.CreateLoad(ty(t), obj.pl.addr) : obj.val.v;
    Value *len = b.CreateExtractValue(v, 1);
    boundsCheck(i, len, p);
    Value *addr = b.CreateInBoundsGEP(ty(et), b.CreateExtractValue(v, 0), i);
    if (obj.isPlace) return {true, {addr, et}, {}};
    Value *ev = b.CreateLoad(ty(et), addr);
    Value_ out{ev, et};
    if (owning(et) && obj.val.fresh) out = {copyValue(ev, et), et, false, true};
    release(obj.val);
    return {false, {}, out};
}

Value_ Codegen::arrayLit(const ArrayLitExpr &a, const FType *want) {
    Pos p = a.pos;
    FType et;
    if (want && want->kind == FType::Array) {
        et = *want->elem;
    } else if (a.elems.empty()) {
        failAt(p.file, p.line, p.col, "an empty [] needs a type, like: []int nums = []");
    } else {
        // numbers written in the code: if any has a dot, they are all floats
        bool allNum = true, anyFloat = false;
        for (auto &x : a.elems) {
            const Expr *y = x.get();
            if (y->kind == ExprKind::Unary && static_cast<const UnaryExpr *>(y)->op == '-') y = static_cast<const UnaryExpr *>(y)->operand.get();
            if (y->kind == ExprKind::Float) anyFloat = true;
            else if (y->kind != ExprKind::Int) allNum = false;
        }
        et = FType::Void;
        if (allNum && anyFloat) et = FType::F64;
    }
    std::vector<Value *> vals;
    for (size_t k = 0; k < a.elems.size(); k++) {
        Value_ v = et.kind == FType::Void ? exprWant(*a.elems[k], et) : exprWant(*a.elems[k], et);
        if (et.kind == FType::Void) {
            if (v.type.kind == FType::Void || v.type.kind == FType::Null)
                failAt(a.elems[k]->pos.file, a.elems[k]->pos.line, a.elems[k]->pos.col, "an array element needs a value with a type");
            et = v.type;
        }
        v = coerce(v, et, a.elems[k]->pos, "every element of this array");
        vals.push_back(own(v));
    }
    FType at = FType::arrayOf(et);
    Value *n = b.getInt64(vals.size());
    Value *data = vals.empty() ? (Value *)ConstantPointerNull::get(b.getPtrTy())
                               : b.CreateCall(rt("finch_alloc"), {b.CreateMul(n, elemSize(et))});
    for (size_t k = 0; k < vals.size(); k++) b.CreateStore(vals[k], b.CreateInBoundsGEP(ty(et), data, b.getInt64(k)));
    Value *arr = UndefValue::get(ty(at));
    arr = b.CreateInsertValue(arr, data, 0);
    arr = b.CreateInsertValue(arr, n, 1);
    arr = b.CreateInsertValue(arr, n, 2);
    return {arr, at, false, true};
}

// ---------- operators ----------

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

void Codegen::badOperands(BinOp op, const Value_ &l, const Value_ &r, Pos p) {
    std::string msg = std::string("can't use '") + opName(op) + "' on " + l.type.show() +
                      (l.type == r.type ? "" : " and " + r.type.show());
    if (l.type.isInt() && r.type.isInt() && l.type != r.type) msg += " (convert one side, like " + l.type.show() + "(...))";
    if (l.type.kind == FType::Bool && (op == BinOp::BitAnd || op == BinOp::BitOr))
        msg += std::string(" (for true/false use '") + (op == BinOp::BitAnd ? "&&" : "||") + "')";
    else if ((l.type.isFloat() || r.type.isFloat()) && op >= BinOp::BitAnd && op <= BinOp::Shr)
        msg += " (bit operators work on whole numbers only)";
    else if (op == BinOp::Add && (l.type.kind == FType::Str || r.type.kind == FType::Str))
        msg += " (turn the other side into text first: str(...))";
    else if ((l.type.kind == FType::Struct || l.type.kind == FType::Array) && (op == BinOp::Eq || op == BinOp::NotEq))
        msg += " (compare the parts one by one)";
    failAt(p.file, p.line, p.col, msg);
}

// Bring both sides to one type: a number literal adapts to the other side,
// otherwise the smaller type grows into the bigger one.
bool Codegen::unify(Value_ &l, Value_ &r) {
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

Value_ Codegen::arith(BinOp op, Value_ l, Value_ r, Pos p) {
    Value_ v = arithOp(op, l, r, p);
    v.literal = l.literal && r.literal && isa<Constant>(v.v);
    return v;
}

Value_ Codegen::arithOp(BinOp op, Value_ l, Value_ r, Pos p) {
    if (op == BinOp::Add && l.type.kind == FType::Str && r.type.kind == FType::Str) {
        Value *out = tmp(Constant::getNullValue(ty(FType::Str)));
        b.CreateCall(rt("finch_str_concat"), {out, tmp(l.v), tmp(r.v)});
        release(l);
        release(r);
        return {b.CreateLoad(ty(FType::Str), out), FType::Str, false, true};
    }
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

Value_ Codegen::compare(BinOp op, Value_ l, Value_ r, Pos p) {
    bool equality = op == BinOp::Eq || op == BinOp::NotEq;
    auto ptrLike = [](const FType &t) { return t.isPtr() || t.kind == FType::Null; };

    if (ptrLike(l.type) || ptrLike(r.type)) {  // pointers compare addresses
        if (!equality || !ptrLike(l.type) || !ptrLike(r.type)) {
            if (l.type.kind == FType::Str || r.type.kind == FType::Str)
                failAt(p.file, p.line, p.col, "a str is never null; to check for empty text use s.len == 0");
            badOperands(op, l, r, p);
        }
        return {op == BinOp::Eq ? b.CreateICmpEQ(l.v, r.v) : b.CreateICmpNE(l.v, r.v), FType::Bool};
    }

    if (l.type.kind == FType::Str && r.type.kind == FType::Str) {
        Value *res;
        if (equality) {
            Value *eq = b.CreateCall(rt("finch_str_eq"), {tmp(l.v), tmp(r.v)});
            res = op == BinOp::Eq ? b.CreateICmpNE(eq, b.getInt32(0)) : b.CreateICmpEQ(eq, b.getInt32(0));
        } else {
            Value *c = b.CreateCall(rt("finch_str_cmp"), {tmp(l.v), tmp(r.v)});
            CmpInst::Predicate pr = op == BinOp::Less ? CmpInst::ICMP_SLT : op == BinOp::LessEq ? CmpInst::ICMP_SLE
                                  : op == BinOp::Greater ? CmpInst::ICMP_SGT : CmpInst::ICMP_SGE;
            res = b.CreateICmp(pr, c, b.getInt64(0));
        }
        release(l);
        release(r);
        return {res, FType::Bool};
    }

    if (!unify(l, r)) badOperands(op, l, r, p);
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
    if (!l.type.isInt() && l.type.kind != FType::Char && l.type.kind != FType::Bool) badOperands(op, l, r, p);
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
Value_ Codegen::logic(const BinaryExpr &e) {
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

Value_ Codegen::binary(const BinaryExpr &e) {
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

// ---------- language server index ----------

void Codegen::note(Pos at, size_t len, const std::string &hover, Pos def) {
    if (!g_index || at.line <= 0) return;
    SymRef r{at.file, at.line, at.col, (int)len, hover};
    if (def.line > 0) {
        r.defFile = def.file;
        r.defLine = def.line;
        r.defCol = def.col;
    }
    g_index->refs.push_back(r);
}

std::string Codegen::signature(const Fn &fn) {
    std::string s = "fn " + (fn.module.empty() ? "" : fn.module + ".") + fn.decl->name + "(";
    for (size_t i = 0; i < fn.params.size(); i++) s += (i ? ", " : "") + fn.params[i].show() + " " + fn.decl->params[i].name;
    s += ")";
    if (fn.ret.kind != FType::Void) s += " -> " + fn.ret.show();
    return s;
}

std::string Codegen::structHover(StructInfo *s) {
    std::string h = "```finch\nstruct " + s->name + " {\n";
    for (auto &f : s->fields) h += "    " + f.type.show() + " " + f.name + "\n";
    return h + "}\n```" + (s->isC ? "\nfrom C" : "");
}

// ---------- debug info ----------

void Codegen::setLoc(Pos p) {
    if (!di || !diFn) return;
    b.SetCurrentDebugLocation(DILocation::get(ctx, p.line, p.col, diFn));
}

}  // namespace finch

std::unique_ptr<llvm::Module> generate(const std::vector<Program> &progs, const CImports &c, llvm::LLVMContext &ctx,
                                       llvm::TargetMachine &tm, bool debug) {
    return finch::Codegen(ctx, progs, c, tm, debug).run();
}
