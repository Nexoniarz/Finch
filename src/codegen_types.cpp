// Code generation, part 2: types, conversions, ownership (copy/drop) and printing.

#include "codegen_impl.h"

using namespace llvm;

namespace finch {

// ---------- types ----------

llvm::Type *Codegen::ty(const FType &t) {
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
    case FType::Array: return StructType::get(ctx, {b.getPtrTy(), b.getInt64Ty(), b.getInt64Ty()});
    case FType::Map: {
        llvm::Type *P = b.getPtrTy(), *I = b.getInt64Ty();
        return StructType::get(ctx, {P, I, I, I, P, I});  // entries, len, used, cap, index, icap
    }
    case FType::Ptr:
    case FType::Null: return b.getPtrTy();
    case FType::Fixed: return ArrayType::get(ty(*t.elem), t.count);
    case FType::Struct: return t.info->llvm;
    case FType::Named: break;
    }
    std::fprintf(stderr, "internal compiler error: unresolved type %s\n", t.show().c_str());
    std::exit(2);
}

FType Codegen::resolve(const FType &t, Pos p) { return resolveT(t, p, true); }

StructInfo *Codegen::findStruct(const std::string &module, const std::string &name, bool qualified) {
    if (qualified) {
        if (module == "C") {
            auto it = cStructs.find(name);
            return it == cStructs.end() ? nullptr : it->second;
        }
        auto m = modules.find(module);
        if (m == modules.end()) return nullptr;
        auto it = m->second.structs.find(name);
        return it == m->second.structs.end() ? nullptr : it->second;
    }
    auto &here = modules[curModule].structs;
    if (auto it = here.find(name); it != here.end()) return it->second;
    if (auto it = cStructs.find(name); it != cStructs.end()) return it->second;
    return nullptr;
}

FType Codegen::resolveT(const FType &t, Pos p, bool deep) {
    switch (t.kind) {
    case FType::Ptr:
        if (!t.elem) return t;
        return FType::ptrTo(resolveT(*t.elem, p, false));
    case FType::Array: return FType::arrayOf(resolveT(*t.elem, p, false));
    case FType::Map: {
        FType k = resolveT(*t.key, p, true);
        if (!k.isInt() && k.kind != FType::Char && k.kind != FType::Bool && k.kind != FType::Str)
            failAt(p.file, p.line, p.col, "a map key must be a whole number, a char, a bool or a str, not " + k.show());
        FType v = resolveT(*t.elem, p, false);
        if (v.kind == FType::Void) failAt(p.file, p.line, p.col, "a map value can't have the type 'nothing'");
        return FType::mapOf(k, v);
    }
    case FType::Fixed: {
        FType f = t;
        f.elem = std::make_shared<FType>(resolveT(*t.elem, p, deep));
        return f;
    }
    case FType::Named: {
        bool qualified = !t.module.empty();
        if (qualified && t.module != "C" && !modules[curModule].imports.count(t.module))
            failAt(p.file, p.line, p.col, "'" + t.module + "' is not imported here (add: import " + t.module + ")");
        StructInfo *s = findStruct(t.module, t.name, qualified);
        if (!s) failAt(p.file, p.line, p.col, "there is no type named '" + t.show() + "'");
        FType r(FType::Struct);
        r.info = s;
        r.name = s->name;
        r.module = s->module == "C" ? "" : s->module;  // shown as math.Vec; C and main-file structs by name
        if (deep) resolveStruct(s);
        return r;
    }
    default: return t;
    }
}

void Codegen::resolveStruct(StructInfo *s) {
    if (s->state == 2) return;
    if (s->state == 1)
        failAt(s->pos.file, s->pos.line, s->pos.col,
               "the struct " + s->name + " contains itself; use ptr[" + s->name + "] or []" + s->name + " for that field");
    s->state = 1;
    std::string savedModule = curModule;
    curModule = s->isC ? "" : s->module;

    std::vector<llvm::Type *> elems;
    if (!s->isC) {
        for (const Field &f : s->decl->fields) {
            StructInfo::F nf;
            nf.name = f.name;
            nf.type = resolveT(f.type, f.pos, true);
            if (nf.type.kind == FType::Void) failAt(f.pos.file, f.pos.line, f.pos.col, "a field can't have the type 'nothing'");
            nf.init = f.init.get();
            nf.pos = f.namePos;
            nf.llvmIndex = elems.size();
            elems.push_back(ty(nf.type));
            s->owning |= owning(nf.type);
            s->fields.push_back(nf);
        }
        s->llvm->setBody(elems);
    } else {
        // Lay the C struct out byte for byte: explicit padding, packed, exactly C's offsets.
        long long off = 0;
        Pos none;
        for (const CField &cf : s->cstruct->fields) {
            if (cf.hidden || cf.offset < off) continue;
            FType ft = resolveT(cf.type, none, true);
            llvm::Type *lt = ty(ft);
            long long size = dl->getTypeAllocSize(lt);
            if (size != cf.size) continue;  // something Finch can't mirror; the padding covers it
            if (cf.offset > off) elems.push_back(ArrayType::get(b.getInt8Ty(), cf.offset - off));
            StructInfo::F nf;
            nf.name = cf.name;
            nf.type = ft;
            nf.llvmIndex = elems.size();
            elems.push_back(lt);
            s->fields.push_back(nf);
            off = cf.offset + size;
        }
        if (s->cstruct->size > off) elems.push_back(ArrayType::get(b.getInt8Ty(), s->cstruct->size - off));
        s->llvm->setBody(elems, true);
        if (s->cstruct->hasHidden && s->unsupported.empty()) s->unsupported = "has fields Finch can't see (unions or bit fields)";
    }
    curModule = savedModule;
    s->state = 2;
}

bool Codegen::owning(const FType &t) {
    switch (t.kind) {
    case FType::Str:
    case FType::Array:
    case FType::Map: return true;
    case FType::Struct:
        resolveStruct(t.info);
        return t.info->owning;
    default: return false;
    }
}

Constant *Codegen::zero(const FType &t) { return Constant::getNullValue(ty(t)); }

Value_ Codegen::defaultValue(const FType &t, Pos p) {
    if (t.kind == FType::Struct && !t.info->isC) {
        for (auto &f : t.info->fields)
            if (f.init) return construct(t.info, {}, {}, p);
    }
    return {zero(t), t, false, owning(t)};
}

// Can every value of `from` be stored in `to`? (i8 -> i32 yes, i32 -> u32 no)
bool Codegen::widens(const FType &from, const FType &to) {
    if (!from.isInt() || !to.isInt()) return false;
    if (from.isSigned() && to.isUnsigned()) return false;
    return to.bits() > from.bits();
}

// Does the constant number `c` (of type `from`) fit into `to`?
bool Codegen::fits(ConstantInt *c, const FType &from, const FType &to) {
    if (!to.isInt()) return false;
    bool negative = from.isSigned() && c->isNegative();
    if (negative && to.isUnsigned()) return false;
    if (negative) return c->getSExtValue() >= -(1LL << (to.bits() - 1));
    unsigned long long v = c->getZExtValue();
    if (to.bits() == 64) return to.isUnsigned() || v <= (unsigned long long)INT64_MAX;
    unsigned long long max = to.isUnsigned() ? (1ULL << to.bits()) - 1 : (1ULL << (to.bits() - 1)) - 1;
    return v <= max;
}

Value *Codegen::intCast(Value *v, const FType &from, const FType &to) { return b.CreateIntCast(v, ty(to), from.isSigned()); }

Value *Codegen::intToFloat(Value *v, const FType &from, const FType &to) {
    return from.isSigned() ? b.CreateSIToFP(v, ty(to)) : b.CreateUIToFP(v, ty(to));
}

// Implicit conversions: only the ones that can't surprise you.
Value_ Codegen::coerce(Value_ v, const FType &want, Pos p, const std::string &what) {
    const FType &from = v.type;
    if (from == want) return v;
    if (from.kind == FType::Null && want.isPtr()) return {ConstantPointerNull::get(b.getPtrTy()), want};
    // any pointer fits an untyped `ptr`; an untyped ptr fits any pointer
    if (want.isPtr() && from.isPtr() && (!want.elem || !from.elem)) return {v.v, want};

    if (from.isInt() && want.isInt()) {
        auto *c = v.literal ? dyn_cast<ConstantInt>(v.v) : nullptr;
        if ((c && fits(c, from, want)) || widens(from, want)) return {intCast(v.v, from, want), want, (bool)c};
        if (c)
            failAt(p.file, p.line, p.col, "the number " + std::to_string(from.isSigned() ? c->getSExtValue() : (long long)c->getZExtValue()) +
                                              " doesn't fit in " + want.show());
    }
    if (from.isInt() && want.isFloat()) return {intToFloat(v.v, from, want), want, v.literal};
    if (from.kind == FType::F32 && want.kind == FType::F64) return {b.CreateFPExt(v.v, ty(want)), want};
    if (from.kind == FType::F64 && want.kind == FType::F32 && v.literal) return {b.CreateFPTrunc(v.v, ty(want)), want, true};

    std::string msg = what + " must be " + want.show() + ", but this is " + from.show();
    if (from.isNumber() && want.isNumber())
        msg += " (use " + want.show() + "(...) to convert" + (from.isFloat() && want.isInt() ? ", it cuts off the fraction)" : ")");
    else if (want.kind == FType::Str && (from.isNumber() || from.kind == FType::Bool || from.kind == FType::Char))
        msg += " (turn it into text with str(...))";
    else if (want.isPtr() && from.kind == FType::Str)
        msg += " (use .ptr to get the text's address)";
    if (from.kind == FType::Null) msg = what + " can't be null (only pointers can)";
    failAt(p.file, p.line, p.col, msg);
}

// ---------- ownership ----------

Value *Codegen::tmpOf(llvm::Type *t) {
    Function *f = b.GetInsertBlock()->getParent();
    IRBuilder<> entry(&f->getEntryBlock(), f->getEntryBlock().begin());
    AllocaInst *a = entry.CreateAlloca(t);
    if (t->isStructTy()) a->setAlignment(Align(16));
    return a;
}

Value *Codegen::tmp(Value *v) {
    Value *a = tmpOf(v->getType());
    b.CreateStore(v, a);
    return a;
}

Value *Codegen::own(const Value_ &v) {
    if (!owning(v.type) || v.fresh) return v.v;
    return copyValue(v.v, v.type);
}

void Codegen::release(const Value_ &v) {
    // constants (string literals, empty arrays) own no memory: nothing to free
    if (v.fresh && v.v && !isa<Constant>(v.v) && owning(v.type)) dropValue(v.v, v.type);
}

Value *Codegen::copyValue(Value *v, const FType &t) {
    if (!owning(t)) return v;
    Value *out = tmpOf(ty(t));
    copyAt(out, tmp(v), t);
    return b.CreateLoad(ty(t), out);
}

void Codegen::dropValue(Value *v, const FType &t) {
    if (owning(t)) dropAt(tmp(v), t);
}

void Codegen::dropAt(Value *addr, const FType &t) {
    if (!owning(t)) return;
    if (t.kind == FType::Str) b.CreateCall(rt("finch_str_drop"), {addr});
    else b.CreateCall(dropFn(t), {addr});
}

void Codegen::copyAt(Value *dst, Value *src, const FType &t) {
    if (!owning(t)) {
        b.CreateStore(b.CreateLoad(ty(t), src), dst);
        return;
    }
    if (t.kind == FType::Str) b.CreateCall(rt("finch_str_copy"), {dst, src});
    else b.CreateCall(copyFn(t), {dst, src});
}

void Codegen::forN(Value *n, const std::function<void(Value *)> &body) {
    Value *i = tmpOf(b.getInt64Ty());
    b.CreateStore(b.getInt64(0), i);
    BasicBlock *cond = newBlock("n.cond"), *loop = newBlock("n.body"), *end = newBlock("n.end");
    b.CreateBr(cond);
    b.SetInsertPoint(cond);
    Value *iv = b.CreateLoad(b.getInt64Ty(), i);
    b.CreateCondBr(b.CreateICmpSLT(iv, n), loop, end);
    b.SetInsertPoint(loop);
    body(iv);
    b.CreateStore(b.CreateAdd(iv, b.getInt64(1)), i);
    b.CreateBr(cond);
    b.SetInsertPoint(end);
}

std::string Codegen::typeKey(const FType &t) {
    switch (t.kind) {
    case FType::Array: return "arr_" + typeKey(*t.elem);
    case FType::Map: return "map_" + typeKey(*t.key) + "_" + typeKey(*t.elem);
    case FType::Fixed: return "fix" + std::to_string(t.count) + "_" + typeKey(*t.elem);
    case FType::Ptr: return "ptr";
    case FType::Struct: return "S_" + (t.info->module.empty() ? std::string("main") : t.info->module) + "_" + t.info->name;
    default: return t.show();
    }
}

// Helper functions are generated once per type and shared; they have no source position.
struct HelperScope {
    Codegen &cg;
    IRBuilderBase::InsertPointGuard guard;
    DISubprogram *savedFn;
    HelperScope(Codegen &c, Function *f) : cg(c), guard(c.b), savedFn(c.diFn) {
        c.diFn = nullptr;
        c.b.SetInsertPoint(BasicBlock::Create(c.ctx, "entry", f));
        c.b.SetCurrentDebugLocation(DebugLoc());
    }
    ~HelperScope() { cg.diFn = savedFn; }
};

Function *Codegen::dropFn(const FType &t) {
    std::string key = "drop." + typeKey(t);
    if (auto it = helpers.find(key); it != helpers.end()) return it->second;
    Function *f = Function::Create(FunctionType::get(b.getVoidTy(), {b.getPtrTy()}, false), Function::InternalLinkage,
                                   "finch." + key, mod.get());
    helpers[key] = f;
    HelperScope hs(*this, f);
    Value *a = f->getArg(0);
    if (t.kind == FType::Array) {
        FType et = *t.elem;
        if (owning(et)) {
            Value *data = b.CreateLoad(b.getPtrTy(), b.CreateStructGEP(ty(t), a, 0));
            Value *len = b.CreateLoad(b.getInt64Ty(), b.CreateStructGEP(ty(t), a, 1));
            forN(len, [&](Value *i) { dropAt(b.CreateInBoundsGEP(ty(et), data, i), et); });
        }
        b.CreateCall(rt("finch_arr_free"), {a});
    } else if (t.kind == FType::Map) {
        FType kt = *t.key, vt = *t.elem;
        if (owning(kt) || owning(vt))
            forEachEntry(a, t, [&](Value *, Value *entry) {
                dropAt(b.CreateStructGEP(mapEntryTy(t), entry, 1), kt);
                dropAt(b.CreateStructGEP(mapEntryTy(t), entry, 2), vt);
            });
        b.CreateCall(rt("finch_map_free"), {a});
    } else if (t.kind == FType::Struct) {
        for (auto &fl : t.info->fields)
            if (owning(fl.type)) dropAt(b.CreateStructGEP(t.info->llvm, a, fl.llvmIndex), fl.type);
    }
    b.CreateRetVoid();
    return f;
}

Function *Codegen::copyFn(const FType &t) {
    std::string key = "copy." + typeKey(t);
    if (auto it = helpers.find(key); it != helpers.end()) return it->second;
    Function *f = Function::Create(FunctionType::get(b.getVoidTy(), {b.getPtrTy(), b.getPtrTy()}, false),
                                   Function::InternalLinkage, "finch." + key, mod.get());
    helpers[key] = f;
    HelperScope hs(*this, f);
    Value *dst = f->getArg(0), *src = f->getArg(1);
    if (t.kind == FType::Array) {
        FType et = *t.elem;
        b.CreateCall(rt("finch_arr_clone_raw"), {dst, src, elemSize(et)});
        if (owning(et)) {
            Value *d = b.CreateLoad(b.getPtrTy(), b.CreateStructGEP(ty(t), dst, 0));
            Value *s = b.CreateLoad(b.getPtrTy(), b.CreateStructGEP(ty(t), src, 0));
            Value *len = b.CreateLoad(b.getInt64Ty(), b.CreateStructGEP(ty(t), src, 1));
            forN(len, [&](Value *i) {
                copyAt(b.CreateInBoundsGEP(ty(et), d, i), b.CreateInBoundsGEP(ty(et), s, i), et);
            });
        }
    } else if (t.kind == FType::Map) {
        StructType *ET = mapEntryTy(t);
        FType kt = *t.key, vt = *t.elem;
        b.CreateCall(rt("finch_map_clone_raw"), {dst, src, b.getInt64(dl->getTypeAllocSize(ET))});
        if (owning(kt) || owning(vt))  // the slots were copied byte for byte: now give the copy its own keys and values
            forEachEntry(src, t, [&](Value *i, Value *from) {
                Value *to = mapEntry(dst, t, i);
                copyAt(b.CreateStructGEP(ET, to, 1), b.CreateStructGEP(ET, from, 1), kt);
                copyAt(b.CreateStructGEP(ET, to, 2), b.CreateStructGEP(ET, from, 2), vt);
            });
    } else if (t.kind == FType::Struct) {
        b.CreateStore(b.CreateLoad(ty(t), src), dst);
        for (auto &fl : t.info->fields)
            if (owning(fl.type))
                copyAt(b.CreateStructGEP(t.info->llvm, dst, fl.llvmIndex), b.CreateStructGEP(t.info->llvm, src, fl.llvmIndex), fl.type);
    }
    b.CreateRetVoid();
    return f;
}

// ---------- strings ----------

Value *Codegen::strConst(const std::string &s) {
    Constant *g = b.CreateGlobalString(s, "str");
    return ConstantStruct::get(cast<StructType>(ty(FType::Str)), {g, b.getInt64(s.size()), b.getInt64(0)});
}

// str -> char* for C (an empty str may have no memory at all; C gets "" then)
Value *Codegen::cstr(Value *s) {
    Value *p = b.CreateExtractValue(s, 0);
    return b.CreateSelect(b.CreateIsNull(p), b.CreateGlobalString("", "empty"), p);
}

Value_ Codegen::strFromC(Value *p) {
    Value *out = tmpOf(ty(FType::Str));
    b.CreateCall(rt("finch_str_from_c"), {out, p});
    return {b.CreateLoad(ty(FType::Str), out), FType::Str};
}

Value *Codegen::elemSize(const FType &t) { return b.getInt64(dl->getTypeAllocSize(ty(t))); }

// ---------- printing ----------

void Codegen::printf_(const std::string &fmt, std::vector<Value *> args) {
    args.insert(args.begin(), b.CreateGlobalString(fmt, "fmt"));
    b.CreateCall(libc("printf", b.getInt32Ty(), {b.getPtrTy()}, true), args);
}

void Codegen::emitPrint(Value *v, const FType &t, bool quoted) {
    if (t.isSigned()) printf_("%lld", {b.CreateSExt(v, b.getInt64Ty())});
    else if (t.isUnsigned()) printf_("%llu", {b.CreateZExt(v, b.getInt64Ty())});
    else if (t.isFloat()) printf_("%g", {b.CreateFPExt(v, b.getDoubleTy())});
    else if (t.kind == FType::Char) printf_(quoted ? "'%c'" : "%c", {b.CreateZExt(v, b.getInt32Ty())});
    else if (t.kind == FType::Bool) printf_("%s", {b.CreateSelect(v, b.CreateGlobalString("true", "true"), b.CreateGlobalString("false", "false"))});
    else if (t.kind == FType::Str)
        printf_(quoted ? "\"%.*s\"" : "%.*s", {b.CreateTrunc(b.CreateExtractValue(v, 1), b.getInt32Ty()), cstr(v)});
    else {  // pointers, arrays, maps, structs: built as text first (the same text str(x) gives)
        Value *buf = tmp(zero(FType::Str));
        emitFormat(buf, v, t, quoted);
        b.CreateCall(rt("finch_buf_print"), {buf});
    }
}

void Codegen::bufText(Value *buf, const std::string &text) {
    b.CreateCall(rt("finch_buf_add"), {buf, b.CreateGlobalString(text, "txt"), b.getInt64(text.size())});
}

void Codegen::emitFormat(Value *buf, Value *v, const FType &t, bool quoted) {
    if (t.isSigned()) b.CreateCall(rt("finch_buf_int"), {buf, b.CreateSExt(v, b.getInt64Ty())});
    else if (t.isUnsigned()) b.CreateCall(rt("finch_buf_uint"), {buf, b.CreateZExt(v, b.getInt64Ty())});
    else if (t.isFloat()) b.CreateCall(rt("finch_buf_float"), {buf, b.CreateFPExt(v, b.getDoubleTy())});
    else if (t.kind == FType::Char) b.CreateCall(rt("finch_buf_char"), {buf, v, b.getInt32(quoted)});
    else if (t.kind == FType::Bool) b.CreateCall(rt("finch_buf_bool"), {buf, b.CreateZExt(v, b.getInt32Ty())});
    else if (t.kind == FType::Str) b.CreateCall(rt("finch_buf_str"), {buf, tmp(v), b.getInt32(quoted)});
    else if (t.isPtr() || t.kind == FType::Null) b.CreateCall(rt("finch_buf_ptr"), {buf, v});  // the same everywhere: null or 0x...
    else b.CreateCall(formatFn(t), {buf, tmp(v)});
}

// One function per array / map / struct type: (str *buf, T *value)
Function *Codegen::formatFn(const FType &t) {
    std::string key = "format." + typeKey(t);
    if (auto it = helpers.find(key); it != helpers.end()) return it->second;
    Function *f = Function::Create(FunctionType::get(b.getVoidTy(), {b.getPtrTy(), b.getPtrTy()}, false),
                                   Function::InternalLinkage, "finch." + key, mod.get());
    helpers[key] = f;
    HelperScope hs(*this, f);
    Value *buf = f->getArg(0), *a = f->getArg(1);
    if (t.kind == FType::Array || t.kind == FType::Fixed) {
        FType et = *t.elem;
        Value *data, *len;
        if (t.kind == FType::Fixed) {
            data = a;
            len = b.getInt64(t.count);
        } else {
            data = b.CreateLoad(b.getPtrTy(), b.CreateStructGEP(ty(t), a, 0));
            len = b.CreateLoad(b.getInt64Ty(), b.CreateStructGEP(ty(t), a, 1));
        }
        bufText(buf, "[");
        forN(len, [&](Value *i) {
            BasicBlock *sep = newBlock("sep"), *item = newBlock("item");
            b.CreateCondBr(b.CreateICmpSGT(i, b.getInt64(0)), sep, item);
            b.SetInsertPoint(sep);
            bufText(buf, ", ");
            b.CreateBr(item);
            b.SetInsertPoint(item);
            emitFormat(buf, b.CreateLoad(ty(et), b.CreateInBoundsGEP(ty(et), data, i)), et, true);
        });
        bufText(buf, "]");
    } else if (t.kind == FType::Map) {  // {"a": 1, "b": 2}
        StructType *ET = mapEntryTy(t);
        Value *firstItem = tmp(b.getInt1(true));
        bufText(buf, "{");
        forEachEntry(a, t, [&](Value *, Value *entry) {
            BasicBlock *sep = newBlock("sep"), *item = newBlock("item");
            b.CreateCondBr(b.CreateLoad(b.getInt1Ty(), firstItem), item, sep);
            b.SetInsertPoint(sep);
            bufText(buf, ", ");
            b.CreateBr(item);
            b.SetInsertPoint(item);
            b.CreateStore(b.getInt1(false), firstItem);
            emitFormat(buf, b.CreateLoad(ty(*t.key), b.CreateStructGEP(ET, entry, 1)), *t.key, true);
            bufText(buf, ": ");
            emitFormat(buf, b.CreateLoad(ty(*t.elem), b.CreateStructGEP(ET, entry, 2)), *t.elem, true);
        });
        bufText(buf, "}");
    } else if (t.kind == FType::Struct) {
        bufText(buf, t.info->name + "(");
        bool first = true;
        for (auto &fl : t.info->fields) {
            if (fl.hidden) continue;
            bufText(buf, (first ? "" : ", ") + fl.name + ": ");
            first = false;
            emitFormat(buf, b.CreateLoad(ty(fl.type), b.CreateStructGEP(t.info->llvm, a, fl.llvmIndex)), fl.type, true);
        }
        bufText(buf, ")");
    }
    b.CreateRetVoid();
    return f;
}

// ---------- debug info ----------

DIType *Codegen::diType(const FType &t) {
    std::string key = typeKey(t);
    if (auto it = diTypes.find(key); it != diTypes.end()) return it->second;
    DIType *d = nullptr;
    auto basic = [&](const char *n, unsigned bits, unsigned enc) { return di->createBasicType(n, bits, enc); };
    switch (t.kind) {
    case FType::Void: return nullptr;
    case FType::Bool: d = basic("bool", 8, dwarf::DW_ATE_boolean); break;
    case FType::Char: d = basic("char", 8, dwarf::DW_ATE_signed_char); break;
    case FType::I8: d = basic("i8", 8, dwarf::DW_ATE_signed); break;
    case FType::I16: d = basic("i16", 16, dwarf::DW_ATE_signed); break;
    case FType::I32: d = basic("i32", 32, dwarf::DW_ATE_signed); break;
    case FType::I64: d = basic("int", 64, dwarf::DW_ATE_signed); break;
    case FType::U8: d = basic("u8", 8, dwarf::DW_ATE_unsigned); break;
    case FType::U16: d = basic("u16", 16, dwarf::DW_ATE_unsigned); break;
    case FType::U32: d = basic("u32", 32, dwarf::DW_ATE_unsigned); break;
    case FType::U64: d = basic("u64", 64, dwarf::DW_ATE_unsigned); break;
    case FType::F32: d = basic("f32", 32, dwarf::DW_ATE_float); break;
    case FType::F64: d = basic("float", 64, dwarf::DW_ATE_float); break;
    case FType::Ptr:
    case FType::Null: d = di->createPointerType(t.elem ? diType(*t.elem) : nullptr, 64, 0, std::nullopt, t.show()); break;
    case FType::Fixed:
        d = di->createArrayType(t.count * (uint64_t)dl->getTypeAllocSizeInBits(ty(*t.elem)), 0, diType(*t.elem),
                                di->getOrCreateArray({di->getOrCreateSubrange(0, t.count)}));
        break;
    case FType::Str:
    case FType::Array: {
        DIType *et = t.kind == FType::Str ? diType(FType::Char) : diType(*t.elem);
        DIType *pt = di->createPointerType(et, 64);
        DIType *i64 = diType(FType::I64);
        DIFile *f = diFiles[0];
        Metadata *els[] = {di->createMemberType(diUnit, "ptr", f, 0, 64, 64, 0, DINode::FlagZero, pt),
                           di->createMemberType(diUnit, "len", f, 0, 64, 64, 64, DINode::FlagZero, i64),
                           di->createMemberType(diUnit, "cap", f, 0, 64, 64, 128, DINode::FlagZero, i64)};
        d = di->createStructType(diUnit, t.show(), f, 0, 192, 64, DINode::FlagZero, nullptr, di->getOrCreateArray(els));
        break;
    }
    case FType::Map: {  // shown by its parts; the entries are behind `entries`
        DIType *i64 = diType(FType::I64), *pt = di->createPointerType(nullptr, 64);
        DIFile *f = diFiles[0];
        const char *names[] = {"entries", "len", "used", "cap", "index", "icap"};
        std::vector<Metadata *> els;
        for (unsigned k = 0; k < 6; k++)
            els.push_back(di->createMemberType(diUnit, names[k], f, 0, 64, 64, 64 * k, DINode::FlagZero, k == 0 || k == 4 ? pt : i64));
        d = di->createStructType(diUnit, t.show(), f, 0, 384, 64, DINode::FlagZero, nullptr, di->getOrCreateArray(els));
        break;
    }
    case FType::Struct: {
        StructInfo *s = t.info;
        DIFile *f = s->isC ? diFiles[0] : diFiles[s->pos.file];
        auto *fwd = di->createReplaceableCompositeType(dwarf::DW_TAG_structure_type, s->name, diUnit, f, s->pos.line);
        diTypes[key] = fwd;
        const StructLayout *lay = dl->getStructLayout(s->llvm);
        std::vector<Metadata *> els;
        for (auto &fl : s->fields) {
            uint64_t bits = dl->getTypeAllocSizeInBits(ty(fl.type));
            els.push_back(di->createMemberType(diUnit, fl.name, f, s->pos.line, bits, 8, lay->getElementOffsetInBits(fl.llvmIndex),
                                               DINode::FlagZero, diType(fl.type)));
        }
        DICompositeType *real = di->createStructType(diUnit, s->name, f, s->pos.line, lay->getSizeInBits(), 8,
                                                     DINode::FlagZero, nullptr, di->getOrCreateArray(els));
        di->replaceTemporary(TempDIType(fwd), real);
        d = real;
        break;
    }
    case FType::Named: return nullptr;
    }
    diTypes[key] = d;
    return d;
}

}  // namespace finch
