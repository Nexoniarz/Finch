// The C calling convention for structs passed and returned by value (System V x86-64),
// the way clang does it: small structs travel in registers, split into eightbytes that are
// either INTEGER (general registers) or SSE (xmm registers); bigger ones go through memory.

#include "codegen_impl.h"
#include "target.h"

using namespace llvm;

namespace finch {

namespace {

enum Class { None, Integer, Sse };

struct Leaf {
    uint64_t offset;
    uint64_t size;
    bool isFloat;
    bool isDouble;
};

}  // namespace

// Flatten a value into its scalar pieces with byte offsets.
static void leaves(Codegen &cg, const FType &t, uint64_t base, std::vector<Leaf> &out) {
    switch (t.kind) {
    case FType::Struct: {
        const StructLayout *lay = cg.dl->getStructLayout(t.info->llvm);
        for (auto &f : t.info->fields) leaves(cg, f.type, base + lay->getElementOffset(f.llvmIndex), out);
        break;
    }
    case FType::Fixed: {
        uint64_t es = cg.dl->getTypeAllocSize(cg.ty(*t.elem));
        for (long long i = 0; i < t.count; i++) leaves(cg, *t.elem, base + i * es, out);
        break;
    }
    default:
        out.push_back({base, cg.dl->getTypeAllocSize(cg.ty(t)), t.kind == FType::F32 || t.kind == FType::F64,
                       t.kind == FType::F64});
    }
}

AbiArg Codegen::classify(const FType &t) {
    AbiArg a;
    if (t.kind != FType::Struct) return a;  // Direct
    uint64_t size = dl->getTypeAllocSize(ty(t));
    if (g_target.windows) {
        // Microsoft x64: 1, 2, 4 or 8 bytes travel as one integer (even if they hold floats);
        // anything else is copied by the caller and passed by address
        if (size == 1 || size == 2 || size == 4 || size == 8) {
            a.kind = AbiArg::Expand;
            a.parts.push_back(b.getIntNTy(size * 8));
            a.coerced = a.parts[0];
        } else {
            a.kind = AbiArg::Indirect;
        }
        return a;
    }
    if (size > 16 || size == 0) {
        a.kind = AbiArg::Memory;
        return a;
    }
    std::vector<Leaf> ls;
    leaves(*this, t, 0, ls);
    unsigned n = (size + 7) / 8;
    Class cls[2] = {None, None};
    bool hasDouble[2] = {false, false};
    for (const Leaf &l : ls) {
        if (l.offset % std::min<uint64_t>(l.size, 8) != 0) {  // misaligned field (packed struct): memory
            a.kind = AbiArg::Memory;
            return a;
        }
        unsigned k = l.offset / 8;
        cls[k] = (cls[k] == Integer || !l.isFloat) ? Integer : Sse;
        hasDouble[k] |= l.isDouble;
    }
    a.kind = AbiArg::Expand;
    for (unsigned k = 0; k < n; k++) {
        uint64_t bytes = std::min<uint64_t>(8, size - 8 * k);
        if (cls[k] == Sse) {
            if (hasDouble[k]) a.parts.push_back(b.getDoubleTy());
            else if (bytes > 4) a.parts.push_back(FixedVectorType::get(b.getFloatTy(), 2));
            else a.parts.push_back(b.getFloatTy());
        } else {
            a.parts.push_back(b.getIntNTy(bytes * 8));
        }
    }
    a.coerced = a.parts.size() == 1 ? a.parts[0] : StructType::get(ctx, a.parts);
    return a;
}

namespace {

struct Plan {
    AbiArg ret;
    std::vector<AbiArg> params;
    llvm::FunctionType *fty = nullptr;
};

}  // namespace

// Decide how every parameter travels, counting the 6 integer and 8 SSE argument registers.
static Plan makePlan(Codegen &cg, const FType &ret, const std::vector<FType> &params, bool variadic,
                     std::vector<llvm::Type *> *extraTypes = nullptr) {
    Plan pl;
    int intRegs = 6, sseRegs = 8;
    std::vector<llvm::Type *> types;
    llvm::Type *retTy = cg.ty(ret);
    if (ret.kind == FType::Str) retTy = cg.b.getPtrTy();
    pl.ret = cg.classify(ret);
    if (pl.ret.kind == AbiArg::Indirect) pl.ret.kind = AbiArg::Memory;  // returned through a hidden pointer
    if (pl.ret.kind == AbiArg::Memory) {
        types.push_back(cg.b.getPtrTy());  // sret
        retTy = cg.b.getVoidTy();
        intRegs--;
    } else if (pl.ret.kind == AbiArg::Expand) {
        retTy = pl.ret.coerced;
    }
    for (const FType &t : params) {
        AbiArg a = cg.classify(t);
        if (a.kind == AbiArg::Expand) {
            int ni = 0, ns = 0;
            for (llvm::Type *p : a.parts) (p->isIntegerTy() ? ni : ns)++;
            if (g_target.windows || (ni <= intRegs && ns <= sseRegs)) {
                intRegs -= ni;
                sseRegs -= ns;
                for (llvm::Type *p : a.parts) types.push_back(p);
            } else {
                a.kind = AbiArg::Memory;  // out of registers: the whole struct goes on the stack
            }
        }
        if (a.kind == AbiArg::Memory || a.kind == AbiArg::Indirect) types.push_back(cg.b.getPtrTy());
        if (a.kind == AbiArg::Direct) {
            if (t.isFloat()) sseRegs--;
            else intRegs--;
            types.push_back(t.kind == FType::Str ? cg.b.getPtrTy() : cg.ty(t));
        }
        pl.params.push_back(a);
    }
    (void)extraTypes;
    pl.fty = FunctionType::get(retTy, types, variadic);
    return pl;
}

static Attribute::AttrKind extAttr(const FType &t) {
    if (t.bits() >= 32 || !(t.isInt() || t.kind == FType::Bool || t.kind == FType::Char)) return Attribute::None;
    return t.isSigned() || t.kind == FType::Char ? Attribute::SExt : Attribute::ZExt;
}

// Attributes that tell LLVM how C passes each piece: sret, byval, signext/zeroext.
static AttributeList attrsFor(Codegen &cg, const Plan &pl, const FType &ret, const std::vector<FType> &params) {
    LLVMContext &ctx = cg.ctx;
    AttributeList al;
    unsigned idx = 0;
    if (pl.ret.kind == AbiArg::Memory) {
        al = al.addParamAttribute(ctx, idx, Attribute::getWithStructRetType(ctx, cg.ty(ret)));
        idx++;
    } else if (Attribute::AttrKind k = extAttr(ret); k != Attribute::None) {
        al = al.addRetAttribute(ctx, k);
    }
    for (size_t i = 0; i < params.size(); i++) {
        const AbiArg &a = pl.params[i];
        if (a.kind == AbiArg::Memory) {
            uint64_t align = std::max<uint64_t>(8, params[i].info->isC ? params[i].info->cdecl->align : 8);
            al = al.addParamAttribute(ctx, idx, Attribute::getWithByValType(ctx, cg.ty(params[i])));
            al = al.addParamAttribute(ctx, idx, Attribute::getWithAlignment(ctx, Align(align)));
            idx++;
        } else if (a.kind == AbiArg::Indirect) {
            idx++;
        } else if (a.kind == AbiArg::Expand) {
            idx += a.parts.size();
        } else {
            if (Attribute::AttrKind k = extAttr(params[i]); k != Attribute::None) al = al.addParamAttribute(ctx, idx, k);
            idx++;
        }
    }
    return al;
}

static std::vector<FType> resolvedParams(Codegen &cg, const CFunc &f) {
    std::vector<FType> ps;
    for (const FType &t : f.params) ps.push_back(cg.resolve(t, Pos{}));
    return ps;
}

Function *Codegen::declareC(const CFunc &f) {
    if (Function *fn = mod->getFunction(f.name)) return fn;
    FType ret = resolve(f.ret, Pos{});
    std::vector<FType> ps = resolvedParams(*this, f);
    for (const FType &t : ps)
        if (t.kind == FType::Struct && !t.info->unsupported.empty()) return nullptr;
    Plan pl = makePlan(*this, ret, ps, f.variadic);
    Function *fn = Function::Create(pl.fty, Function::ExternalLinkage, f.name, mod.get());
    fn->setAttributes(attrsFor(*this, pl, ret, ps));
    return fn;
}

// A stack slot big enough for both the struct and its register form.
static Value *abiSlot(Codegen &cg, const FType &t, const AbiArg &a) {
    llvm::Type *st = cg.ty(t);
    llvm::Type *big = a.coerced && cg.dl->getTypeAllocSize(a.coerced) > cg.dl->getTypeAllocSize(st) ? a.coerced : st;
    return cg.tmpOf(big);
}

Value_ Codegen::emitCCall(const CFunc &f, Function *fn, std::vector<Value_> args, Pos p) {
    FType ret = resolve(f.ret, p);
    std::vector<FType> ps = resolvedParams(*this, f);
    for (const FType &t : ps)
        if (t.kind == FType::Struct && !t.info->unsupported.empty())
            failAt(p.file, p.line, p.col, "the C function '" + f.name + "' takes the struct " + t.info->name +
                                              " by value, which Finch can't pass yet: it " + t.info->unsupported);
    if (ret.kind == FType::Struct && !ret.info->unsupported.empty())
        failAt(p.file, p.line, p.col, "the C function '" + f.name + "' returns the struct " + ret.info->name +
                                          " by value, which Finch can't receive yet: it " + ret.info->unsupported);
    Plan pl = makePlan(*this, ret, ps, f.variadic);

    std::vector<Value *> ll;
    Value *sret = nullptr;
    if (pl.ret.kind == AbiArg::Memory) {
        sret = tmpOf(ty(ret));
        cast<AllocaInst>(sret)->setAlignment(Align(16));
        ll.push_back(sret);
    }
    for (size_t i = 0; i < args.size(); i++) {
        const Value_ &v = args[i];
        if (i >= ps.size()) {  // the '...' part: C's default promotions
            const FType &t = v.type;
            Value *x = v.v;
            if (t.kind == FType::F32) x = b.CreateFPExt(x, b.getDoubleTy());
            else if (t.kind == FType::Str) x = cstr(x);
            else if (t.kind == FType::Bool || ((t.isInt() || t.kind == FType::Char) && t.bits() < 32))
                x = b.CreateIntCast(x, b.getInt32Ty(), t.isSigned() || t.kind == FType::Char);
            ll.push_back(x);
            continue;
        }
        const AbiArg &a = pl.params[i];
        if (ps[i].kind == FType::Str) {
            ll.push_back(cstr(v.v));
        } else if (a.kind == AbiArg::Memory || a.kind == AbiArg::Indirect) {
            Value *s = tmpOf(ty(ps[i]));  // a copy the callee may change
            cast<AllocaInst>(s)->setAlignment(Align(16));
            b.CreateStore(v.v, s);
            ll.push_back(s);
        } else if (a.kind == AbiArg::Expand) {
            Value *s = abiSlot(*this, ps[i], a);
            b.CreateStore(v.v, s);
            for (size_t k = 0; k < a.parts.size(); k++)
                ll.push_back(b.CreateLoad(a.parts[k], b.CreateConstInBoundsGEP1_64(b.getInt8Ty(), s, 8 * k)));
        } else {
            ll.push_back(v.v);
        }
    }
    CallInst *call = b.CreateCall(pl.fty, fn, ll);
    AttributeList al = attrsFor(*this, pl, ret, ps);
    call->setAttributes(al);

    if (ret.kind == FType::Void) return {nullptr, FType::Void};
    if (pl.ret.kind == AbiArg::Memory) return {b.CreateLoad(ty(ret), sret), ret};
    if (pl.ret.kind == AbiArg::Expand) {
        Value *s = abiSlot(*this, ret, pl.ret);
        b.CreateStore(call, s);
        return {b.CreateLoad(ty(ret), s), ret};
    }
    if (ret.kind == FType::Str) return strFromC(call);  // borrowed: copied if it is kept
    return {call, ret};
}

// A C-callable door into a Finch function, for addr(fn) (callbacks like qsort's or libclang's).
Function *Codegen::cThunk(Fn &fn, Pos p) {
    if (fn.cThunk) return fn.cThunk;
    auto cOk = [](const FType &t) {
        return t.isNumber() || t.kind == FType::Bool || t.kind == FType::Char || t.isPtr() || t.kind == FType::Void ||
               (t.kind == FType::Struct && t.info->isC);
    };
    for (const FType &t : fn.params)
        if (!cOk(t))
            failAt(p.file, p.line, p.col, "addr(" + fn.decl->name + ") gives C a function pointer, so its parameters must be C types (numbers, ptr, C structs), not " + t.show());
    if (!cOk(fn.ret)) failAt(p.file, p.line, p.col, "addr(" + fn.decl->name + "): a function given to C can't return " + fn.ret.show());

    Plan pl = makePlan(*this, fn.ret, fn.params, false);
    Function *th = Function::Create(pl.fty, Function::InternalLinkage, "finch.c." + fn.decl->name, mod.get());
    th->setAttributes(attrsFor(*this, pl, fn.ret, fn.params));
    fn.cThunk = th;

    IRBuilderBase::InsertPointGuard guard(b);
    DISubprogram *savedFn = diFn;
    diFn = nullptr;
    b.SetInsertPoint(BasicBlock::Create(ctx, "entry", th));
    b.SetCurrentDebugLocation(DebugLoc());
    unsigned ai = 0;
    Value *sret = nullptr;
    if (pl.ret.kind == AbiArg::Memory) sret = th->getArg(ai++);
    std::vector<Value *> args;
    for (size_t i = 0; i < fn.params.size(); i++) {
        const AbiArg &a = pl.params[i];
        if (a.kind == AbiArg::Memory || a.kind == AbiArg::Indirect) {
            args.push_back(b.CreateLoad(ty(fn.params[i]), th->getArg(ai++)));
        } else if (a.kind == AbiArg::Expand) {
            Value *s = abiSlot(*this, fn.params[i], a);
            for (size_t k = 0; k < a.parts.size(); k++)
                b.CreateStore(th->getArg(ai++), b.CreateConstInBoundsGEP1_64(b.getInt8Ty(), s, 8 * k));
            args.push_back(b.CreateLoad(ty(fn.params[i]), s));
        } else {
            args.push_back(th->getArg(ai++));
        }
    }
    Value *r = b.CreateCall(fn.llvm, args);
    if (fn.ret.kind == FType::Void) {
        b.CreateRetVoid();
    } else if (pl.ret.kind == AbiArg::Memory) {
        b.CreateStore(r, sret);
        b.CreateRetVoid();
    } else if (pl.ret.kind == AbiArg::Expand) {
        Value *s = abiSlot(*this, fn.ret, pl.ret);
        b.CreateStore(r, s);
        b.CreateRet(b.CreateLoad(pl.ret.coerced, s));
    } else {
        b.CreateRet(r);
    }
    diFn = savedFn;
    return th;
}

}  // namespace finch
