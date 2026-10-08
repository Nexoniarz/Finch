// Code generation, part 4: maps.
//
// map[K]V is { entries, len, used, cap, index, icap } (see runtime/finch_rt.c). The runtime finds and
// places entries; the compiler stores, copies and drops the keys and values in them, like it does
// for array elements. An entry is { i64 hash, K key, V value }; hash 0 marks a removed entry.

#include "codegen_impl.h"
#include "builtins_doc.h"

using namespace llvm;

namespace finch {

StructType *Codegen::mapEntryTy(const FType &t) {
    return StructType::get(ctx, {b.getInt64Ty(), ty(*t.key), ty(*t.elem)});
}

// The runtime's (map, key, entry size, key size, key is str) arguments.
std::vector<Value *> Codegen::mapArgs(Value *map, Value *keyAddr, const FType &t) {
    return {map, keyAddr, b.getInt64(dl->getTypeAllocSize(mapEntryTy(t))), elemSize(*t.key),
            b.getInt32(t.key->kind == FType::Str ? 1 : 0)};
}

Value *Codegen::mapEntry(Value *map, const FType &t, Value *i) {
    Value *entries = b.CreateLoad(b.getPtrTy(), b.CreateStructGEP(ty(t), map, 0));
    return b.CreateInBoundsGEP(mapEntryTy(t), entries, i);
}

// The entry for `key`, added if it's missing: the key is copied in, and the value is its type's
// default when `fill` (m[k] += 1 starts from 0).
Value *Codegen::mapSlot(Value *map, const FType &t, const Value_ &key, Pos p, bool fill) {
    StructType *ET = mapEntryTy(t);
    Value *keyAddr = tmp(key.v);
    Value *added = tmpOf(b.getInt32Ty());
    std::vector<Value *> args = mapArgs(map, keyAddr, t);
    args.push_back(added);
    Value *entry = mapEntry(map, t, b.CreateCall(rt("finch_map_slot"), args));
    BasicBlock *fresh = newBlock("map.new"), *have = newBlock("map.have");
    b.CreateCondBr(b.CreateICmpNE(b.CreateLoad(b.getInt32Ty(), added), b.getInt32(0)), fresh, have);
    b.SetInsertPoint(fresh);
    copyAt(b.CreateStructGEP(ET, entry, 1), keyAddr, *t.key);
    if (fill) b.CreateStore(own(defaultValue(*t.elem, p)), b.CreateStructGEP(ET, entry, 2));
    b.CreateBr(have);
    b.SetInsertPoint(have);
    return entry;
}

// Every live entry, in insertion order: body(entry number, entry address).
void Codegen::forEachEntry(Value *map, const FType &t, const std::function<void(Value *, Value *)> &body) {
    Value *used = b.CreateLoad(b.getInt64Ty(), b.CreateStructGEP(ty(t), map, 2));
    forN(used, [&](Value *i) {
        Value *entry = mapEntry(map, t, i);
        Value *hash = b.CreateLoad(b.getInt64Ty(), b.CreateStructGEP(mapEntryTy(t), entry, 0));
        BasicBlock *live = newBlock("entry.live"), *next = newBlock("entry.next");
        b.CreateCondBr(b.CreateICmpNE(hash, b.getInt64(0)), live, next);
        b.SetInsertPoint(live);
        body(i, entry);
        b.CreateBr(next);
        b.SetInsertPoint(next);
    });
}

// A key as text for error messages: "bob" or 42
Value *Codegen::keyText(const Value_ &key) {
    Value *out = tmpOf(ty(FType::Str));
    const FType &t = key.type;
    if (t.kind == FType::Str) {
        Value *q = tmp(strConst("\"")), *half = tmpOf(ty(FType::Str));
        b.CreateCall(rt("finch_str_concat"), {half, q, tmp(key.v)});
        b.CreateCall(rt("finch_str_concat"), {out, half, q});
    } else if (t.isSigned()) {
        b.CreateCall(rt("finch_str_from_int"), {out, b.CreateSExt(key.v, b.getInt64Ty())});
    } else if (t.isUnsigned()) {
        b.CreateCall(rt("finch_str_from_uint"), {out, b.CreateZExt(key.v, b.getInt64Ty())});
    } else if (t.kind == FType::Char) {
        b.CreateCall(rt("finch_str_from_char"), {out, key.v});
    } else {
        b.CreateCall(rt("finch_str_from_bool"), {out, b.CreateZExt(key.v, b.getInt32Ty())});
    }
    return out;
}

// m[key]: reading a missing key stops the program; writing (m[k] = v, m[k] += 1, m[k].push(x)) adds it
LRef Codegen::mapIndex(const IndexExpr &e, LRef &obj, const FType &t, bool forWrite) {
    Pos p = e.pos;
    FType kt = *t.key, vt = *t.elem;
    Value_ key = coerce(exprWant(*e.index, kt), kt, e.index->pos, "the key");
    if (forWrite) {
        if (!obj.isPlace) failAt(p.file, p.line, p.col, "this is a temporary map, it can't be changed");
        Value *entry = mapSlot(obj.pl.addr, t, key, p, true);
        release(key);
        return {true, {b.CreateStructGEP(mapEntryTy(t), entry, 2), vt}, {}};
    }
    Value *map = obj.isPlace ? obj.pl.addr : tmp(obj.val.v);
    Value *i = b.CreateCall(rt("finch_map_find"), mapArgs(map, tmp(key.v), t));
    BasicBlock *missing = newBlock("map.missing"), *found = newBlock("map.found");
    b.CreateCondBr(b.CreateICmpSLT(i, b.getInt64(0)), missing, found);
    b.SetInsertPoint(missing);
    b.CreateCall(rt("finch_map_missing"), {fileName(p), b.getInt64(p.line), keyText(key)});
    b.CreateUnreachable();
    b.SetInsertPoint(found);
    release(key);
    Value *valueAddr = b.CreateStructGEP(mapEntryTy(t), mapEntry(map, t, i), 2);
    if (obj.isPlace) return {true, {valueAddr, vt}, {}};
    Value *v = b.CreateLoad(ty(vt), valueAddr);  // from a temporary map: take the value, drop the rest
    Value_ out{v, vt};
    if (owning(vt) && obj.val.fresh) out = {copyValue(v, vt), vt, false, true};
    release(obj.val);
    return {false, {}, out};
}

// ["a": 1, "b": 2], or [:] where the type is known
Value_ Codegen::mapLit(const MapLitExpr &e, const FType *want) {
    Pos p = e.pos;
    FType t;
    Value_ k0, v0;  // the first entry, when it decides the types
    bool first = false;
    if (want && want->kind == FType::Map) {
        t = *want;
    } else if (e.keys.empty()) {
        failAt(p.file, p.line, p.col, "an empty [:] needs a type, like: map[str]int m = [:]");
    } else {
        // number values written in the code are all floats if any of them has a dot
        bool allNum = true, anyFloat = false;
        for (auto &x : e.values) {
            const Expr *y = x.get();
            if (y->kind == ExprKind::Unary && static_cast<const UnaryExpr *>(y)->op == '-') y = static_cast<const UnaryExpr *>(y)->operand.get();
            if (y->kind == ExprKind::Float) anyFloat = true;
            else if (y->kind != ExprKind::Int) allNum = false;
        }
        k0 = expr(*e.keys[0]);
        v0 = expr(*e.values[0]);
        Pos kp = e.keys[0]->pos, vp = e.values[0]->pos;
        if (k0.type.kind == FType::Void || k0.type.kind == FType::Null) failAt(kp.file, kp.line, kp.col, "a map key needs a value with a type");
        if (v0.type.kind == FType::Void || v0.type.kind == FType::Null) failAt(vp.file, vp.line, vp.col, "a map value needs a value with a type");
        t = resolve(FType::mapOf(k0.type, allNum && anyFloat ? FType(FType::F64) : v0.type), kp);
        first = true;
    }
    Value *map = tmp(zero(t));
    StructType *ET = mapEntryTy(t);
    Held held(*this);
    holdAddr(map, t);  // the entries so far, if a later one leaves early
    for (size_t i = 0; i < e.keys.size(); i++) {
        Held entry(*this);
        Value_ k = i == 0 && first ? k0 : exprWant(*e.keys[i], *t.key);
        k = coerce(k, *t.key, e.keys[i]->pos, "every key of this map");
        hold(k);
        Value_ v = i == 0 && first ? v0 : exprWant(*e.values[i], *t.elem);
        Value *vv = own(coerce(v, *t.elem, e.values[i]->pos, "every value of this map"));
        Value *valueAddr = b.CreateStructGEP(ET, mapSlot(map, t, k, p, false), 2);
        dropAt(valueAddr, *t.elem);  // a key given twice: the last value wins
        b.CreateStore(vv, valueAddr);
        release(k);
    }
    return {b.CreateLoad(ty(t), map), t, false, true};
}

Value_ Codegen::mapMethod(const MethodExpr &m, Value_ *recv) {
    Pos p = m.pos;
    const std::string &n = m.name;
    FType t = recv->type, kt = *t.key, vt = *t.elem;
    StructType *ET = mapEntryTy(t);
    Value *map = recv->v;
    auto need = [&](size_t k) { checkArgs(m.args, m.argNames, k, p, n); };
    auto keyArg = [&]() { return coerce(exprWant(*m.args[0], kt), kt, m.args[0]->pos, "the key"); };
    if (g_index)
        for (const BuiltinDoc &d : kMapMethods)
            if (n == d.name) note(Pos{p.line, p.col + 1, p.file}, n.size(), "```finch\n" + std::string(d.signature) + "\n```\n" + d.doc);

    if (n == "has") {
        need(1);
        Value_ k = keyArg();
        Value *i = b.CreateCall(rt("finch_map_find"), mapArgs(map, tmp(k.v), t));
        release(k);
        return {b.CreateICmpSGE(i, b.getInt64(0)), FType::Bool};
    }
    if (n == "get") {  // m.get(key, default): the default is only computed when the key is missing
        need(2);
        Value_ k = keyArg();
        Value *i = b.CreateCall(rt("finch_map_find"), mapArgs(map, tmp(k.v), t));
        release(k);
        BasicBlock *found = newBlock("get.found"), *missing = newBlock("get.missing"), *end = newBlock("get.end");
        b.CreateCondBr(b.CreateICmpSGE(i, b.getInt64(0)), found, missing);
        b.SetInsertPoint(found);
        Value *v = b.CreateLoad(ty(vt), b.CreateStructGEP(ET, mapEntry(map, t, i), 2));
        v = copyValue(v, vt);
        BasicBlock *foundEnd = b.GetInsertBlock();
        b.CreateBr(end);
        b.SetInsertPoint(missing);
        Value *d = own(coerce(exprWant(*m.args[1], vt), vt, m.args[1]->pos, "the default"));
        BasicBlock *missingEnd = b.GetInsertBlock();
        b.CreateBr(end);
        b.SetInsertPoint(end);
        PHINode *phi = b.CreatePHI(ty(vt), 2);
        phi->addIncoming(v, foundEnd);
        phi->addIncoming(d, missingEnd);
        return {phi, vt, false, owning(vt)};
    }
    if (n == "remove") {  // true if the key was there
        need(1);
        Value_ k = keyArg();
        Value *i = b.CreateCall(rt("finch_map_find"), mapArgs(map, tmp(k.v), t));
        release(k);
        BasicBlock *hit = newBlock("remove.hit"), *end = newBlock("remove.end");
        Value *was = b.CreateICmpSGE(i, b.getInt64(0));
        b.CreateCondBr(was, hit, end);
        b.SetInsertPoint(hit);
        Value *entry = mapEntry(map, t, i);
        dropAt(b.CreateStructGEP(ET, entry, 1), kt);
        dropAt(b.CreateStructGEP(ET, entry, 2), vt);
        b.CreateCall(rt("finch_map_remove_at"), {map, i, b.getInt64(dl->getTypeAllocSize(ET))});
        b.CreateBr(end);
        b.SetInsertPoint(end);
        return {was, FType::Bool};
    }
    if (n == "clear") {
        need(0);
        if (owning(kt) || owning(vt))
            forEachEntry(map, t, [&](Value *, Value *entry) {
                dropAt(b.CreateStructGEP(ET, entry, 1), kt);
                dropAt(b.CreateStructGEP(ET, entry, 2), vt);
            });
        b.CreateCall(rt("finch_map_clear"), {map});
        return {nullptr, FType::Void};
    }
    if (n == "keys" || n == "values") {  // a new array, in insertion order
        need(0);
        FType et = n == "keys" ? kt : vt;
        unsigned field = n == "keys" ? 1 : 2;
        FType at = FType::arrayOf(et);
        Value *out = tmpOf(ty(at));
        Value *len = b.CreateLoad(b.getInt64Ty(), b.CreateStructGEP(ty(t), map, 1));
        b.CreateCall(rt("finch_arr_make"), {out, len, elemSize(et)});
        Value *data = b.CreateLoad(b.getPtrTy(), b.CreateStructGEP(ty(at), out, 0));
        Value *j = tmp(b.getInt64(0));
        forEachEntry(map, t, [&](Value *, Value *entry) {
            Value *jv = b.CreateLoad(b.getInt64Ty(), j);
            copyAt(b.CreateInBoundsGEP(ty(et), data, jv), b.CreateStructGEP(ET, entry, field), et);
            b.CreateStore(b.CreateAdd(jv, b.getInt64(1)), j);
        });
        return {b.CreateLoad(ty(at), out), at, false, true};
    }
    failAt(p.file, p.line, p.col, "a map has no method '" + n + "' (it has: has, get, remove, clear, keys, values; and m[key], .len)");
}

}  // namespace finch
