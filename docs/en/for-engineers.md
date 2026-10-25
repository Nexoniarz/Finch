# Finch for Engineers

**The internals of Finch 2.4, whose compiler is written in Finch.** How source becomes tokens, an AST and LLVM IR *as text*,
which `clang` compiles and links with a small C runtime; how C headers are read with libclang from Finch itself and how
the C calling conventions are reproduced by hand; the type and ownership rules; how the compiler builds itself from a
seed; and the language server.

Assumed: you can read Finch (see the [technicians' guide](for-technicians.md)) and have at least seen LLVM IR.
The compiler's first version, in C++ on LLVM's C++ API, is kept on the `before-bootstrap` branch; this guide describes
the Finch one.

> 🇵🇱 Polska wersja: [dla-inzynierow.md](../pl/dla-inzynierow.md)

---

## Contents

1. [Architecture](#1-architecture)
2. [Source tree](#2-source-tree)
3. [Front end: lexer, parser, modules](#3-front-end-lexer-parser-modules)
4. [Code generation](#4-code-generation)
5. [C import and the C ABI](#5-c-import-and-the-c-abi)
6. [Driver, linking and targets](#6-driver-linking-and-targets)
7. [Type system](#7-type-system)
8. [Ownership and memory](#8-ownership-and-memory)
9. [Generics, enums and match](#9-generics-enums-and-match)
10. [Bootstrapping](#10-bootstrapping)
11. [The language server](#11-the-language-server)
12. [Semantics vs. C](#12-semantics-vs-c)
13. [Known limitations](#13-known-limitations)

---

## 1. Architecture

The compiler has no LLVM library inside it. It writes LLVM IR as **text** and lets `clang` do optimisation, code
generation and linking. That is what makes it small (about 8k lines) and what lets it be written in Finch itself.

```
 main.fch ─► loader ── lex ── parse ──► Program   (+ every imported module, recursively)
                                          │
        `import "x.h"` ─► cimport.fch (libclang, called from Finch) ─► C functions, structs, constants
                                          │
                                          ▼
                       gen.fch ─► generate(): declare structs, declare functions, define bodies
                                  type check and emit IR in one walk; ABI classification for C calls
                                          │   (IR text)
                                          ▼
        linker.fch ─► clang -c prog.ll  ─►  clang prog.o rt-<hash>.o [your .c/.o/.a] -lm [pkg-config / -l]
                                          │
                                          ▼
                        executable   (finch run: run from a temp file, then delete it)
```

Design decisions that shape the rest:

- **Fail fast.** The first error prints `file:line:col: error: …` with the source line and a caret, and the compiler stops.
  No error recovery, so later phases never see half-valid input.
- **One walk.** `gen.fch` type checks and emits IR in one pass over the AST. Each expression yields an IR operand, its
  Finch type (a string kind: `int`, `f32`, `str`, `array`, `map`, `struct`, `enum`, `named`…) and flags.
- **Let clang do the work.** Locals are `alloca`s promoted by `mem2reg`. Optimisation is clang's `-O2` (or `-O0` with `-g`).
- **Values, not references.** Arrays, strings and structs have value semantics; ownership is decided at compile time (§8).
- **A tiny C runtime** (`runtime/finch_rt.c`, ~760 lines) does what is tedious in IR: strings, array growth, maps, input,
  files, panics. It is compiled once and cached in `~/.cache/finch`.

---

## 2. Source tree

| File | Lines | Responsibility |
|---|---|---|
| `src/lexer.fch` | 220 | Tokens, positions, indentation-free layout |
| `src/ast.fch` | 120 | One `Node` struct with a `kind` string, children, type names, positions (`line`, `endLine`) |
| `src/parser.fch` | 780 | Recursive descent: declarations, statements, expressions, generics (`[T]`), `enum`, `match`, `export`, `link` |
| `src/gen.fch` | 5200 | Type check + IR: structs, functions, methods, scopes and drops, statements, expressions, built-ins, maps, errors as values, generics (monomorphisation), enums and `match`, `-g` debug metadata, the `index` mode |
| `src/cimport.fch` | 560 | libclang: functions, structs (fields, offsets, padding), enums, constants, globals, macros |
| `src/platform.fch` | 90 | `Target`: triple, data layout, C compiler, signedness of `char` per system |
| `src/linker.fch` | 300 | The cache, runtime object, libraries (`link`, pkg-config), link-error advice |
| `src/main.fch` | 310 | Driver: CLI, loader with module lookup and overlays, `build/run/ir/index/lsp` |
| `src/lsp.fch` | 875 | The language server (§11) |
| `src/docs.fch` | 110 | One description of the built-ins, shared by hover and completion |
| `std/*.fch` | 900 | `json`, `path`, `time` (+`_windows`), `proc` (+`_windows`) |
| `runtime/finch_rt.c` | 760 | The C runtime |
| `bootstrap/` | | `build.sh`, `install.sh`, the seeds (§10) |

---

## 3. Front end: lexer, parser, modules

**Lexer.** Newlines end statements; there is no indentation rule. Tokens carry file, line and column, which every error uses.

**Parser.** Recursive descent, one function per construct. Notable points: types are parsed into a name string
(`[]str`, `map[str]int`, `ptr[char]`, `Stack[int]`); `fn Type.method` declares a method; `fn name[T]` and `struct Name[T]`
declare templates; `enum` and `match` have their own nodes; `export fn` marks a C-visible function; a block records
its `endLine` (used by the language server). The old `fn name(...) type` order is a parse error with a hint.

**Modules.** `import name` looks, in order, at `name_<os>.fch` next to the importing file (`windows`, `linux`, `macos`),
`name.fch` there, the folders of `FINCH_PATH`, then the standard library `std/`. Each module is parsed once; its names
are used as `module.name`. `import "x.h"` goes to the C importer instead.

---

## 4. Code generation

`gen.fch` is one big `Gen` struct plus functions over it. The passes are:

1. `declareStructs`: every Finch and C struct gets an LLVM type (C structs with their real padding); recursive-by-value
   structs are refused.
2. `declareFns`: signatures, with a *borrow analysis* for parameters (a parameter that is never stored is passed without
   a copy).
3. `define`: each body, type checked and emitted. Scopes track what must be dropped when they end (`defer` runs first).
4. `defineMainWrapper`: the C `main(argc, argv)`, and `finch_on_fatal` for embedders.
5. With `-g`, DWARF metadata is attached as the IR is written (use `-O0`).

Generics are **monomorphised**: a call `max(1, 2)` infers `T`, then the template's AST is generated again with `T` bound,
under a mangled name (`max$int`). `match` over an enum is exhaustive (a missing variant is an error, `else` covers the
rest); on a value it is a chain of comparisons. Enum values are `i32`; `Color(n)` is range checked; printing uses a
generated name table.

Errors as values: `-> T!` returns a pair (value, error text); `try` returns early on error, `or` handles it, `error("…")`
makes one.

---

## 5. C import and the C ABI

`cimport.fch` calls **libclang** directly from Finch (it is itself written with `import "clang-c/Index.h"`), parses the
header with the target's flags, and walks the cursors. Types map as: `const char *` parameters → `str`, other
`char *` → `ptr[char]`, a `char *` result → `str`; integer types by size and signedness (`char` is unsigned on ARM64
Linux); structs and enums by layout.

LLVM IR does not do the C calling convention for you, so `gen.fch` reproduces it:

- `classify` – System V x86-64: a struct up to 16 bytes goes in registers by eightbyte class (INTEGER/SSE), larger in memory;
- `classifyArm64` – AAPCS64, including homogeneous float aggregates (HFA);
- the Microsoft x64 rules (1, 2, 4, 8-byte structs by value in a register, the rest by pointer);
- `makePlan` turns the classification into a call plan; `cThunk` makes `addr(fn)` callable from C (callbacks).

The same code is what lets the compiler call libclang, whose API passes `CXCursor`, `CXType` and `CXString` by value.

---

## 6. Driver, linking and targets

`finch build|run|ir|index file.fch [-o out] [-O0] [-g] [-l lib] [-I dir] [--target t] [--shared] [--overlay path=file]`.
`ir` prints the IR; `index` prints the records the language server uses (§11); `--shared` makes a shared library of the
`export fn`s.

`--target windows|linux|arm64|macos|<triple>` changes the triple and data layout, the C `char` signedness, the ABI rules
above, the OS variant of modules (`name_windows.fch`), and the C compiler: `FINCH_CC` or `clang --target=…`. Linking
asks `pkg-config` for flags for `link "lib"`; when the linker fails the output is analysed and the likely missing
`link "…"` is suggested. The runtime object is keyed by a hash of its source and the target.

Environment: `FINCH_PATH` (module folders), `FINCH_RT` (runtime source), `FINCH_CC` (C compiler/linker; the IR is always
compiled by `clang`, never by `CC`), `FINCH_LIBCLANG` (for `bootstrap/build.sh`).

The compiler finds `runtime/` and `std/` next to itself (`../`, `./`) or under `../share/finch/` (`bootstrap/install.sh`).

---

## 7. Type system

### LLVM representation

| Finch | LLVM |
|---|---|
| `bool` | `i1` |
| `char` `i8` `u8` / `i16` `u16` / `i32` `u32` / `int` `u64` | `i8` / `i16` / `i32` / `i64` |
| `f32` / `float` | `float` / `double` |
| `str`, `[]T` | `{ ptr, i64, i64 }` (pointer, length, capacity) |
| `map[K]V` | `{ ptr, i64, i64, i64, ptr, i64 }` (entries, len, used, cap, index, icap; see src/gen.fch) |
| `ptr`, `ptr[T]`, `null` | `ptr` (opaque) |
| `[N]T` | `[N x T]` |
| Finch struct | named struct type, natural layout |
| a method's `self` | `ptr` (the first parameter) |
| the result of `-> T!` | `{ i1 ok, T value, str error }` (`i8` in place of `T` for `-> !`) |
| C struct | named **packed** struct with explicit padding bytes (§5) |

Signedness lives only in the Finch type and selects `sdiv/udiv`, `ashr/lshr`, `icmp s*/u*`,
`sext/zext`, `sitofp/uitofp`, `fptosi/fptoui`.

### Values

```cpp
struct Value_ { Value *v; FType type; bool literal; bool fresh; };
```

- **`literal`**: a number written in the code or an imported C constant. It may become any integer type
  it fits (`fits()`), or `f32`. It survives unary minus and literal-only arithmetic that folds to a
  constant; explicit conversions clear it (`u8(250)` is a typed constant, so `u8(250) + 10` wraps to 4).
- **`fresh`**: an owning value (str/array/owning struct) that nobody holds yet: the result of a call,
  a literal, a concatenation, a constructor. It must be stored (moved) or released (§8).

### Implicit conversions: `coerce(v, want)`

Identical types; `null` → any pointer; typed pointer ↔ untyped `ptr`; integer → integer if the value is a
fitting literal or the type `widens()` (same signedness and more bits, or unsigned → strictly bigger
signed); integer → float; `f32` → `f64`; literal `f64` → `f32`. Nothing else: the error suggests the
explicit conversion. Owning types convert only to themselves. Array literals take their element type
from the slot they flow into (`exprWant()`), so `[]f32 xs = [1, 2.5]` works.

### Binary operands: `unify(l, r)`

A literal side adapts to the other; otherwise the side that widens grows; `i32` + `u32` is an error;
int + float → float; f32 + f64 → f64. There is no C integer promotion: `u8 + u8` stays `u8`.

### Explicit conversions: `convert()`

| From → To | IR |
|---|---|
| int/char ↔ int/char | `trunc`/`sext`/`zext` by source signedness |
| bool → int | `zext` |
| float ↔ int | `fptosi`/`fptoui`, `sitofp`/`uitofp` |
| float ↔ float | `fpext`/`fptrunc` |
| int → bool | `icmp ne 0` |
| number/bool/char → str | `finch_str_from_int/uint/float/char/bool` (fresh) |
| str → int/float | `finch_str_to_int/float` (panics on bad text) |
| `[]u8`/`[]char` → str | `finch_str_from_bytes` |
| ptr → str | `finch_str_from_c` (borrowed, `cap = -1`) |
| ptr ↔ ptr | nothing |
| int → ptr / ptr → int | `inttoptr` / `ptrtoint` |

### Comparisons

Pointers/null: `icmp eq/ne ptr` only. `str`: `finch_str_eq` (length + `memcmp`) for `==`/`!=`,
`finch_str_cmp` for ordering. Floats: `oeq olt ole ogt oge` and `une` for `!=`. Integers/char by
signedness (char unsigned). Bool: equality only. Arrays/structs: error.

---

---

## 8. Ownership and memory

### 9.1 The model

Every value of an **owning type** (`str`, `[]T`, or a Finch struct with an owning field) has exactly one
owner: a variable, an array element, a struct field, or a heap cell from `new()`. The owner drops the
value when it goes away. There is no reference counting and no garbage collector; every decision is static.

| Situation | What happens |
|---|---|
| `x := expr` / `T x = expr` / `x = expr` / field, element, `push` | **`own(v)`**: a fresh value is moved in; a borrowed one is deep-copied |
| `x = …` (owning) | the new value is computed first, the old one dropped after the store |
| end of a block, `return`, `break`, `continue` | owned variables of every scope being left are dropped (newest first), defers run |
| `return local` | the variable is **moved out** (marked `moved` while the cleanups for that return are emitted) |
| passing an argument | **lent**: the callee gets a shallow copy and never drops it; fresh arguments are released by the caller after the call |
| a parameter the function changes | copied on entry and owned by the callee (decided by `mutates()`, below) |
| a temporary nobody keeps (`print(a + b)`, `f(g())`, an expression statement) | **`release(v)`**: dropped right after use |
| `a.pop()`, `a.remove(i)` | the element is moved out (fresh) |
| `for x in list` | `x` borrows each element; if the body may change `list`, each element is copied instead |
| a method call `v.m()` | `self` **is** `v` (its address is passed): no copy; changes are visible to the caller |
| maps | own their keys and values like arrays own elements; `m[k] = v` moves/copies `v` in and drops the old value |

A string literal is a fresh value with `cap = 0`: moving it costs nothing, dropping it is skipped
(`release()` ignores constants), and writing into it (`s[i] = c`) first makes a heap copy
(`finch_str_own`). Strings from C are borrowed with `cap = -1` and always copied before being kept.

### 9.2 Parameter borrowing: `mutates()`

When declaring a function, every owning parameter is checked with `mutates(name, body)`: an
assignment whose root variable (`rootVar()`: through `.field` and `[i]`) is the parameter, a changing
method on it (`push pop insert remove clear resize sort reverse`), or `addr(…)` of it. Unchanged
parameters are borrowed (`Fn::borrowParam`); changed ones are copied once at entry. The analysis is
syntactic and conservative: anything that *might* change the parameter makes a copy, which is always correct.
The same analysis decides whether `for x in list` must copy elements.

Methods take part: `declareFns()` first finds which methods change `self` (`mutates("self", body)`),
repeating until nothing changes, because calling a changing method on `self` changes `self` too. Their
names go into `mutatingMethods`, which `mutatesExpr()` treats like `push`: `p.move(1)` changes `p`.
Only then is `borrowParam` computed for every function and method.

### 9.3 Copy and drop

`dropAt(addr, T)` / `copyAt(dst, src, T)` work on addresses:

- `str`: `finch_str_drop` (frees if `cap > 0`) / `finch_str_copy` (shares literals, duplicates the rest).
- `[]T` and owning structs: generated helpers `finch.drop.<key>` and `finch.copy.<key>`, created once per
  type (`dropFn`, `copyFn`, keyed by `typeKey()`, e.g. `arr_S_main_Player`). Arrays drop each owning
  element and then `finch_arr_free`; copies call `finch_arr_clone_raw` and then deep-copy owning
  elements over the raw bits. Structs drop/copy only their owning fields.

Helpers are emitted in their own functions (`HelperScope` saves the builder position and switches off
debug locations), may call each other recursively (`struct Node { []Node kids }`), and are inlined by O2.

### 9.4 What it looks like

```c
fn greet(str name) -> str {
    msg := "Hi, " + name
    return msg
}

fn main() {
    a := ["x"]
    b := a
    b.push(greet("Ola"))
    print(b[1])
}
```

The `-O0` IR of `main` (abridged):

```llvm
  %3 = call ptr @finch_alloc(i64 24)                         ; ["x"]: one element on the heap
  store { ptr, i64, i64 } { ptr @str.1, i64 1, i64 0 }, ptr %4  ; the literal, cap 0
  ...
  store { ptr, i64, i64 } %7, ptr %a                          ; moved into a (fresh)
  %a1 = load { ptr, i64, i64 }, ptr %a
  store { ptr, i64, i64 } %a1, ptr %1
  call void @finch.copy.arr_str(ptr %2, ptr %1)               ; b := a  copies
  %8 = load { ptr, i64, i64 }, ptr %2
  store { ptr, i64, i64 } %8, ptr %b
  %9 = call { ptr, i64, i64 } @finch.greet({ ptr, i64, i64 } { ptr @str.2, i64 3, i64 0 })
  ...
  call void @finch_arr_reserve(ptr %b, i64 %12, i64 24)       ; push: grow if needed
  store { ptr, i64, i64 } %9, ptr %15                         ; greet's result moved in, no copy
  ...
  %20 = icmp uge i64 1, %19                                   ; b[1]: bounds check
  br i1 %20, label %out.of.range, label %in.range
  ...
  call void @finch.drop.arr_str(ptr %b)                       ; end of main: newest first
  call void @finch.drop.arr_str(ptr %a)
  ret void
```

and `greet` returns `msg` without copying (`ret … %5` after loading the slot: the variable was moved out).

### 9.5 defer

`defer stmt` pushes a `Cleanup{isDefer, body, order = current varOrder}` into the current scope; it
emits nothing there. Whenever the scope's cleanups are emitted (falling off the end of the block,
`return`, `break`/`continue` leaving it) the body is generated **at that exit**, with `deferLimit = order`
so that `lookup()` hides variables declared after the defer: those are already dropped by then
(cleanups run in reverse). `return`/`break`/`continue` inside a deferred statement are errors (`inDefer`).
Defer bodies are therefore duplicated per exit path, which is what makes them free on the normal path.

### 9.6 Manual memory

`new(v)` allocates `sizeof(T)` with `finch_alloc`, stores `own(v)` and returns `ptr[T]`. `free(p)` drops
the pointee if `T` is owning (skipping null) and calls libc `free`. Pointers never own anything:
`addr(x)` can dangle after `x`'s block ends; this is not checked.

### 9.7 Verifying it

`MEMCHECK=1 tests/run.sh` builds every test program and runs it under valgrind with
`--leak-check=full --errors-for-leak-kinds=all`: zero leaks and zero invalid accesses across all of them,
including returns from inside loops, `continue`/`break` with owned locals, defers, mutation during
iteration, nested arrays of structs, and C struct passing.

---

---

## 9. Generics, enums and match

See §4 for how they are generated. For the rules a user sees, read sections 23 and 24 of the
[technicians' guide](for-technicians.md). The tests are `tests/new/generics.fch`, `tests/new/enums.fch` and their refusals
in `tests/new/fail/`.

---

## 10. Bootstrapping

The compiler is built from a **seed**: its own IR, committed as `bootstrap/<system>.ll` (see
[bootstrap/README.md](../../bootstrap/README.md)). `bootstrap/build.sh`:

```
seed .ll ──clang──► finch0 ──builds src/──► finch1 ──builds src/──► build/finch
```

and stops unless `finch1` and `finch` produce byte-identical IR (a fixpoint: the compiler compiles itself the same way
whatever built it). `bootstrap/build.sh seed` rewrites the seed from the current compiler; do it whenever the compiler's
own source starts to use a feature the committed seed lacks.

A seed is per system because the C calling convention for the libclang calls is baked into it (§5).
Linux x86-64 and Linux ARM64 have seeds. macOS and Windows would need seeds made with Apple's and Microsoft's headers.

---

## 11. The language server

`finch lsp` speaks JSON-RPC over stdin/stdout (`Content-Length` framing, `std/json`). It does not type check; it runs
`finch index file.fch --overlay path=copy …` for every change, where the overlays are the editor's unsaved text. In `index`
mode the compiler prints, besides IR-free output, tab-separated records:

| Record | Meaning |
|---|---|
| `@R file line col len defFile defLine defCol hover` | a reference: what hover shows and where the definition is |
| `@V name type file line col fnFile fnLine fnEnd` | a variable and the function it lives in |
| `@F module name recv file line col endLine signature params` | a function or method |
| `@S` / `@SF`, `@E`, `@I` | structs (and fields), enums, imports |

The first error is printed as `path:line:col: error: message` and becomes a diagnostic. When code does not compile, the
server keeps answering completion and signature help from the last version that did. `tests/lsp_test.py` is the acceptance
test (22 checks).

---

## 12. Semantics vs. C

| Situation | C | Finch 2.0 |
|---|---|---|
| Signed integer overflow | UB | defined wraparound (no `nsw`) |
| Integer division by zero, `INT_MIN / -1` | UB | compile error if constant, otherwise runtime error |
| Array index out of range | UB | runtime error (also for `[N]T` fields of C structs) |
| Null dereference | UB | runtime error for `.value`, `.field`, `[i]` |
| Use after free / double free of arrays, strings, structs | common bugs | impossible: no manual freeing of those |
| Memory leak of arrays, strings, structs | common bug | impossible without `new` |
| `x & 1 == 0` | `x & (1 == 0)` | `(x & 1) == 0` |
| Implicit narrowing, signed/unsigned mixing | silent | compile errors |
| Uninitialized variables | UB | impossible (zero or defaults) |
| Missing return | UB if used | compile error |
| Order of evaluation | mostly unspecified | left to right; in an assignment the right side first |
| Shift ≥ width, out-of-range float→int | UB | still poison (not checked yet) |
| Dangling pointer from `addr()` / `new` + `free` | UB | still UB |

---

---

## 13. Known limitations

- The compiler runs on Linux x86-64 and ARM64 only (no seed for macOS or Windows yet). Programs can be built for all three.
- macOS output is only checked as IR; Windows and ARM64 are run under Wine and qemu in CI.
- One error per run, no recovery.
- `-g` debug info is best with `-O0`.
- The driver uses `sh` utilities (`rm`, `mkdir`), so it needs a POSIX shell.
- Generics are monomorphised (code size grows with instantiations); there are no trait bounds, a wrong use is an error
  inside the instantiated body.

