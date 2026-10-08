# Finch for Engineers

**The complete internals of Finch 2.0.** How source becomes tokens, tokens an AST, and the AST
LLVM IR, which is then optimized, emitted and linked against a small C runtime. The exact type
rules; the ownership model and how it lowers to copies and drops; how every construct becomes IR
(with real compiler output); how C headers, C structs and the System V ABI are handled; modules,
debug info, and the self-hosted compiler in `boot/`. Every function in the compiler is listed with
what it does and how it differs from its neighbours.

Assumed: you know C/C++ and have at least seen LLVM IR.
For the language from a user's point of view, see the [technicians' guide](for-technicians.md).

> 🇵🇱 Polska wersja: [dla-inzynierow.md](../pl/dla-inzynierow.md)

---

## Contents

1. [Architecture](#1-architecture)
2. [Source tree](#2-source-tree)
3. [Lexer](#3-lexer)
4. [Grammar and parser](#4-grammar-and-parser)
5. [AST](#5-ast)
6. [Loading: files and modules](#6-loading-files-and-modules)
7. [Semantic model](#7-semantic-model)
8. [Type system](#8-type-system)
9. [Ownership and memory](#9-ownership-and-memory)
10. [Code generation](#10-code-generation)
11. [Runtime checks](#11-runtime-checks)
12. [The C runtime](#12-the-c-runtime)
13. [C interop: headers, structs, ABI](#13-c-interop-headers-structs-abi)
14. [Optimization, emission, linking](#14-optimization-emission-linking)
15. [Debug info](#15-debug-info)
16. [Diagnostics](#16-diagnostics)
17. [The self-hosted compiler (boot/)](#17-the-self-hosted-compiler-boot)
18. [Build system](#18-build-system)
19. [Testing](#19-testing)
20. [Semantics vs. C](#20-semantics-vs-c)
21. [Function reference](#21-function-reference)
22. [Extending the compiler](#22-extending-the-compiler)
23. [Targets and Windows](#23-targets-and-windows)
24. [The language server](#24-the-language-server)
25. [The VS Code extension](#25-the-vs-code-extension)
26. [Known limitations](#26-known-limitations)

---

## 1. Architecture

Finch is a single-pass front end in front of LLVM: type checking and IR generation are one walk
over the AST, with no intermediate representation of its own.

```
 main.fch ──► Loader::load() ── lex() ── parse() ──► Program  (+ each imported module, recursively)
                                                       │
                     all `import "x.h"` ──► importHeaders() (libclang) ──► CImports
                                                       │
 hostMachine() ──► TargetMachine (triple + data layout)│
                                                       ▼
 generate(progs, cimports, tm, debug) ──► Codegen::run()
     declareStructs()   all Finch + C structs, LLVM struct bodies, cycle check
     declareFns()       every function's signature (+ "borrow" analysis of parameters)
     define()           each body: type check + IRBuilder, scopes, drops, runtime checks
     defineMainWrapper  the C `main(argc, argv)`
     DIBuilder::finalize (with -g) ; verifyModule
                                                       ▼
 optimize()  PassBuilder default<O2>            (skipped with -O0)
 emitObject() legacy PM + addPassesToEmitFile ──► prog.o
 link()      cc prog.o ~/.cache/finch/rt-<hash>.o [your .c/.o/.a] -lm [pkg-config/-l…]
                                                       ▼
 executable   (finch run: executed from a temp file with the remaining arguments, then deleted)
```

Design decisions that shape the rest:

- **Fail fast.** The first error prints a diagnostic and exits (`failAt()` in `src/error.h`). No error
  recovery, so later phases never see half-valid input.
- **One walk.** `Codegen` carries scopes, loops, the current function and module. Each expression
  returns its IR value, its Finch type and two flags (`Value_`, §8).
- **Let LLVM do the work.** Locals are `alloca`s promoted by `mem2reg`/SROA. Finch builds φ-nodes by
  hand only for `&&`/`||`.
- **Values, not references.** Arrays, strings and structs have value semantics: assignment copies, the
  end of a block drops. Ownership is decided at compile time with no reference counting (§9).
- **A tiny C runtime** (`runtime/finch_rt.c`, ~430 lines) does what would be tedious in IR: string
  operations, array growth, input, files, panics. It is embedded in the compiler and cached.

---

## 2. Source tree

| File | Lines | Responsibility |
|---|---|---|
| `src/error.h` | 40 | `g_files` (all sources), `failAt(file, line, col, msg)`, `fail(line, col, msg)` |
| `src/lexer.h/.cpp` | 250 | `Tok`, `Token`, `lex(fileIndex)`, `tokName()` |
| `src/ast.h` | 310 | `Type` (incl. Array, Fixed, Struct, Named), all `Expr`/`Stmt` nodes, `FnDecl`, `StructDecl`, `Import`, `Link`, `Program` |
| `src/parser.h/.cpp` | 540 | Recursive-descent `Parser`, `typeFromName()` |
| `src/cimport.h/.cpp` | 420 | libclang import: functions, C structs (fields, offsets), constants, globals |
| `src/codegen.h` | 15 | `generate()` |
| `src/codegen_impl.h` | 265 | The `Codegen` class and its helper structs, shared by the four files below |
| `src/codegen.cpp` | 1090 | Program, declarations, scopes and cleanups, statements, expressions, places (`ref`), operators, mutation analysis |
| `src/codegen_types.cpp` | 500 | LLVM types, type resolution, struct layout, coercions, ownership (copy/drop helpers), printing, DWARF types |
| `src/codegen_builtins.cpp` | 710 | Calls, constructors, built-ins, conversions, array and str methods, runtime declarations, panics |
| `src/abi.cpp` | 310 | System V x86-64 classification, C calls with structs by value, C-callable thunks |
| `src/main.cpp` | 390 | Driver: CLI, loader, target machine, O2, object emission, runtime cache, linking, link-error advice, `run` |
| `runtime/finch_rt.c` | 440 | The runtime library (§12) |
| `boot/*.fch` | 3080 | The self-hosted compiler (§17) |

---

## 3. Lexer

`lex(file)` makes one pass over `g_files[file].text` and returns `std::vector<Token>` ending in `Tok::End`.

```cpp
struct Token { Tok kind; std::string text; int line, col; bool newlineBefore; };
```

- **Newlines are not tokens.** Each token records `newlineBefore`; the parser decides what it means (§4).
  This keeps newline handling out of every list rule in the grammar.
- **Positions:** 1-based; `col` counts characters (UTF-8 continuation bytes don't advance it), so
  carets line up under `"Błąd"`. A stray non-ASCII character is reported whole, with a hint that names are ASCII-only.
- **Identifiers / keywords:** `[A-Za-z_][A-Za-z0-9_]*`. Keywords:
  `fn return if else while for in break continue true false null import link struct defer`.
  **Type names are not keywords**: `int`, `u8`, `ptr`… are `Ident`s recognised by the parser through
  `typeFromName()`, so `u8(x)` is an ordinary call and the lexer doesn't depend on the type list.
- **Numbers:** decimal or `0x` hex with `_` separators. No octal (`010` is 10). A `.` belongs to a number
  only when a digit follows, so `0..10` is `Int DotDot Int`.
- **Strings / chars:** escapes `\n \t \r \0 \\ \" \'`. No newline inside a literal.
- **Operators:** maximal munch (`->` before `-`, `<<`/`<=` before `<`, `&&` before `&`, `..` before `.`,
  `:=` before `:`). `:` alone is a token (named arguments).
- **Comments:** `//` and non-nesting `/* */`.

---

## 4. Grammar and parser

`Parser` (`src/parser.cpp`) is hand-written recursive descent with one token of lookahead, plus up to
three in `atDeclaration()`.

```ebnf
program     = { import | link | struct | function } ;
import      = "import" ( STRING | IDENT ) ;                 (* "x.h" = C header, name = Finch module *)
link        = "link" STRING ;                               (* "glfw" | "file.c" | "file.o" | "lib.a" *)
struct      = "struct" IDENT "{" { type IDENT [ "=" expr ] NEWLINE } "}" ;
function    = "fn" IDENT "(" [ param { "," param } ] ")" [ "->" type ] block ;
param       = type IDENT ;
type        = "[" "]" type | "ptr" [ "[" type "]" ] | TYPENAME | IDENT [ "." IDENT ] ;

block       = "{" { statement TERMINATOR } "}" ;
statement   = if | while | for | return | "break" | "continue" | "defer" statement | block
            | type IDENT [ "=" expr ]                        (* declaration, see atDeclaration *)
            | IDENT ":=" expr
            | target assignop expr                           (* target: var, field, element, p.value *)
            | call | method ;
for         = "for" IDENT "in" expr ( ".." expr block | block ) ;   (* range | for-each *)
assignop    = "=" | "+=" | "-=" | "*=" | "/=" | "%=" ;

expr        = or ;            or = and { "||" and } ;       and = cmp { "&&" cmp } ;
cmp         = add { ( "==" | "!=" | "<" | "<=" | ">" | ">=" ) add } ;
add         = mul { ( "+" | "-" | "|" | "^" ) mul } ;
mul         = unary { ( "*" | "/" | "%" | "<<" | ">>" | "&" ) unary } ;
unary       = ( "-" | "!" | "~" ) unary | postfix ;
postfix     = primary { "." IDENT [ args ] | "[" expr "]" } ;
primary     = INT | FLOAT | STRING | CHAR | "true" | "false" | "null"
            | IDENT [ args ] | "(" expr ")" | "[" [ expr { "," expr } [","] ] "]" ;
args        = "(" [ arg { "," arg } [","] ] ")" ;  arg = [ IDENT ":" ] expr ;
```

**Statement termination.** After a statement, `endOfStatement()` requires `}`, end of file, or a token
with `newlineBefore`. Inside expressions `sameLine()` (`parenDepth > 0 || !newlineBefore`) decides whether
an operator, `.`, `[` or a call's `(` continues the expression. `parenDepth` grows inside `( )` and `[ ]`
and is saved/reset to 0 when entering `{ }`.

**Precedence is Go's**, not C's: `&` binds like `*`, and `|`/`^` like `+`, all above comparisons, so
`x & MASK == 0` means `(x & MASK) == 0`.

**Recognising statements.** `atDeclaration()` is true for `[` `]` (an array type), `TYPE IDENT`,
`ptr [`, and `IDENT . IDENT IDENT` (a module's type). Otherwise the parser **parses a whole expression,
then looks at the next token**: an assignment operator makes it a target (it must be a `Var`, `Member` or
`Index`), otherwise it must be a call or method call, or it's `this value is computed but never used`.

**Struct literals are calls**: `Point(1, 2)` or `Point(x: 1, y: 2)`. Brace-based literals (`Point{…}`)
would be ambiguous with blocks (`if done {`); a call needs no special grammar, and named arguments
(`IDENT ":"` in `args()`) are only accepted by constructors at code generation time.

---

## 5. AST

All nodes are in `src/ast.h`: small class hierarchies with an explicit `kind`, dispatched with
`switch` + `static_cast`.

| ExprKind | Node | Fields |
|---|---|---|
| `Int` `Float` `Bool` `Char` `Str` | literals | `value` |
| `Null` | `NullExpr` | |
| `Var` | `VarExpr` | `name` |
| `Unary` | `UnaryExpr` | `op` (`- ! ~`), `operand` |
| `Binary` | `BinaryExpr` | `BinOp op`, `lhs`, `rhs` |
| `Call` | `CallExpr` | `callee` (a name), `args`, `argNames` |
| `Member` | `MemberExpr` | `obj`, `field` (`.x`, `.len`, `.ptr`, `.value`) |
| `Index` | `IndexExpr` | `obj`, `index` |
| `ArrayLit` | `ArrayLitExpr` | `elems` |
| `Method` | `MethodExpr` | `obj`, `name`, `args`, `argNames`; also `module.fch(...)` |

| StmtKind | Node |
|---|---|
| `Block` `VarDecl` `Assign` `Expr` `If` `While` `For` `ForEach` `Return` `Break` `Continue` `Defer` | as named; `ForEachStmt` has `var`, `list`, `body`; `DeferStmt` holds one statement |

`math.sqrt(x)` parses as a `MethodExpr` on `VarExpr("math")`; whether `math` is a module or a variable
is decided during code generation. Calls name their callee instead of holding an expression: Finch has
no first-class functions, and resolving the name late lets one `CallExpr` be a built-in, a conversion
(`u8(x)`), a struct constructor, a Finch function or a C function.

There is no "load" node: whether `x` means its address or its value depends on context (`ref()` vs
`expr()`, §10.2).

### `Type`

```cpp
struct Type {
    enum Kind { Void, Bool, Char, I8, I16, I32, I64, U8, U16, U32, U64, F32, F64,
                Str, Ptr, Null, Array, Fixed, Struct, Named };
    Kind kind;
    std::shared_ptr<Type> elem;   // Ptr (null = untyped `ptr`), Array, Fixed
    long long count;              // Fixed
    std::string name, module;     // Named (as written) / Struct (resolved)
    StructInfo *info;             // Struct
};
```

The parser produces `Named` for struct names; `Codegen::resolve()` turns them into `Struct` with an
`info` pointer. `Fixed` (`[N]T`) only comes from C struct fields. `int` is `I64` and `float` is `F64`.
Equality is structural; two `Struct`s are equal iff they share `info`.

---

## 6. Loading: files and modules

`Loader::load(path, module, from)` in `main.cpp` reads a file, appends it to `g_files` (positions carry
the file index in `Pos::file`), lexes and parses it, then loads each `import name` that wasn't seen yet:
`name.fch` next to the importing file, or in a `FINCH_PATH` folder. The result is a `std::vector<Program>`,
main file first. Cycles are fine: a module is marked loaded before its own imports are followed.

All files become **one LLVM module**. Each Finch module has a `ModuleScope` (functions, structs, the set
of modules it imports). Names are looked up in the current module; another module's names are reachable
only qualified (`geometry.length`, `geometry.Vec`) and only if imported there. Functions are emitted as
`finch.<module>.<name>` (`finch.<name>` in the main file), struct types as `finch.<module>.<Name>`.

---

## 7. Semantic model

### Declaration order

`Codegen::run()` first declares **all structs** of all modules plus all imported C structs, resolves
every field type and sets the LLVM bodies (`declareStructs` → `resolveStruct`), then declares **all
function signatures** (`declare`), then defines bodies. So order inside a file and across modules never
matters, and recursion (also mutual, also across modules) works.

`resolveStruct` detects by-value cycles with a three-state flag (`struct A { A inner }` is an error with
the advice to use `ptr[A]` or `[]A`). Pointers and arrays of a struct don't need its layout, so
`struct Node { []Node kids; ptr[Node] parent }` is fine: they resolve the name "shallowly" (`resolveT(…, deep=false)`).

### Scopes and variables

```cpp
struct Var     { Value *slot; FType type; bool readonly; std::string readonlyWhy;
                 bool owned; int order; bool moved; };
struct Cleanup { bool isDefer; std::string var; const Stmt *body; int order; };
struct Scope   { unordered_map<string, Var> vars; vector<Cleanup> cleanups; };
```

A scope is pushed for the function's parameters, each `{ }`, each `for` (holds `i`), each `for … in`
(the hidden list temporary) and each iteration of it (holds `x`).

- **No shadowing:** `addVar()` rejects a name found in *any* enclosing scope, a function of the module,
  an imported module name or a built-in.
- **Read-only:** range and for-each variables (`readonlyWhy` gives the specific message).
- `order` numbers declarations; `defer` uses it to hide variables declared after it (§9.5).

### Name resolution

Value position `x`: local variable → C constant → C global → (module name: error with a hint) → error.
Call `n(...)`: built-ins (`print addr input new free exit shell read_file write_file file_exists`) →
type names (conversion) → function of the current module → struct of the current module / C struct
(constructor) → C function → error (with a hint if the function exists in another module, or a
`headerHint` for well-known libc names).

---

## 8. Type system

### LLVM representation

| Finch | LLVM |
|---|---|
| `bool` | `i1` |
| `char` `i8` `u8` / `i16` `u16` / `i32` `u32` / `int` `u64` | `i8` / `i16` / `i32` / `i64` |
| `f32` / `float` | `float` / `double` |
| `str`, `[]T` | `{ ptr, i64, i64 }` (pointer, length, capacity) |
| `ptr`, `ptr[T]`, `null` | `ptr` (opaque) |
| `[N]T` | `[N x T]` |
| Finch struct | named struct type, natural layout |
| C struct | named **packed** struct with explicit padding bytes (§13.2) |

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
  a literal, a concatenation, a constructor. It must be stored (moved) or released (§9).

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

## 9. Ownership and memory

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

## 10. Code generation

### 10.1 Variables

Every variable, including parameters, gets an `alloca` at the top of the entry block (`slot()`;
C structs get their C alignment). `addVar()` stores the initial value, registers the variable, adds a
drop cleanup if it owns its value, and (with `-g`) declares it to the debugger.

### 10.2 Places: `ref()`, `place()`, `expr()`

```cpp
struct LRef { bool isPlace; Place pl; Value_ val; };   // an address, or (if none) a value
```

`ref(e, forWrite)` walks a `Var`/`Member`/`Index` chain and returns an **address** when one exists:

- `Var` → its `alloca` (or a C global).
- `.field` on a struct place → `getelementptr` to the field (`llvmIndex`; C structs skip padding elements).
- through a pointer (`p.value`, `p.field`, `p[i]`) → load the pointer, check it isn't null, address the target.
  `p.value` on a pointer to a struct that has its own `value` field means that field.
- `a[i]` on an array/str place → load `{ptr,len,cap}`, `boundsCheck(i, len)`, `getelementptr`. For
  `forWrite` on a `str`, `finch_str_own` first (copy-on-write for literals). `[N]T` checks against `N`.
- `.len` / `.ptr` → values.
- a temporary (call result…) has no address: its field/element is extracted, copied if owning, and the
  temporary released.

`expr()` on these kinds calls `ref()` and loads from the address: this is where every `load` of a
variable, field or element comes from. `place()` demands an address (assignment targets, `addr()`).

### 10.3 Assignment

`x = v`: evaluate `v` **first** (it may reallocate memory the target lives in: `a[0] = f()` where `f`
pushes to `a`), then `place(target, forWrite)`, `coerce`, `own`, load the old value, store, drop the old
one. Compound `x += v`: evaluate `v`, load the target, `arith()`, store, drop the old value (for `str +=`).

### 10.4 Functions and `main`

Functions are `internal` with their natural LLVM signature (aggregates by value). Owning parameters are
copied at entry only if `borrowParam` is false. `fn main` is the internal `finch.main`; a separate
external `i32 main(i32, ptr)` (`defineMainWrapper`) builds `[]str args` with `finch_args` if requested,
calls it, drops the arguments and returns the exit code. A function that falls off its end gets the
scope cleanups and `ret void`, or the error `can reach its end without returning T`.

### 10.5 Control flow

The builder's current block is the source of truth for reachability (`terminated()`): statements after
a terminator are errors (`this code can never run`). `continueAt()` deletes a merge block nobody jumps
to. `if`/`while`/`for` are the classic block layouts; `while true` without `break` leaves the function
terminated (no `return` needed after it). `for i in a..b` evaluates both ends once; `continue` goes to
`for.step`. `for x in list` reads the list's length from its address **every round** (pushing inside the
loop is safe) and keeps the index in a stack slot. `break`/`continue` emit the cleanups of the scopes
opened inside the loop (`Loop::scopeDepth`) before jumping.

### 10.6 Short-circuit `&&` / `||`

The only hand-built φ: the left side ends in block `from`, the right side in `rhsEnd`;
`phi i1 [!isAnd, from], [rhs, rhsEnd]`.

### 10.7 Calls

`callFinch()` coerces arguments to parameter types (array literals via `exprWant`), passes owning ones
shallowly, calls, then releases fresh arguments. Results of owning type are fresh.
`construct()` builds a struct from `zeroinitializer` with `insertvalue`: positional (all fields) or named
(missing ones take the field's default expression, evaluated in the struct's module with the caller's
scopes swapped out, or zero). C calls are in §13.

### 10.8 Printing

`print` emits `printf` per argument with a format chosen by type (`%lld`, `%llu`, `%g`, `%c`, `%.*s` for
`str` with its length, `%p`), separated by spaces, then `\n`. Arrays, fixed arrays and structs call
generated helpers `finch.print.<key>` that print `[1, 2]` / `Point(x: 1, y: 2)`, quoting strings and chars inside.

### 10.9 Array and str methods

Arrays: `push` (`finch_arr_reserve` + store + len++), `pop`/`remove` (move out, `remove_gap`),
`insert` (`insert_gap`), `clear`/`resize` (drop the elements that go away, `finch_arr_resize`),
`find`/`contains` (a loop with `compare(Eq)`), `slice` (new array, `copyAt` per element), `reverse`
(swap loop), `sort` (libc `qsort` with a generated comparator `finch.cmp.<key>`), `join` (runtime).
Methods that change the array require an address (a variable/field/element).
Strings: `sub find contains starts_with ends_with split trim upper lower replace repeat bytes`, all in the runtime.

---

## 11. Runtime checks

All runtime errors go through `finch_panic(file, line, msg)` / `finch_panic_index(file, line, i, len)`
(declared `noreturn cold`), so the failing branch is laid out cold and the check costs a compare and a
well-predicted branch. LLVM removes checks it can prove (constant indexes in range, loop-hoisted bounds).

| Check | Where |
|---|---|
| division/remainder by zero (constant zero: compile error) | `checkDivisor()` |
| `MIN / -1` for signed types | `checkDivisor()` |
| array/str/fixed-array index out of range (`icmp uge`, so negative indexes too) | `boundsCheck()` |
| `.value`, `.field`, `[i]` through null | `member()`, `index()` |
| `pop()` on empty, `slice()`/`sub()` out of range | methods, runtime |
| bad text in `int(s)`/`float(s)`, unreadable file in `read_file` | runtime |

A 20-million-element sieve with a bounds check on every access runs as fast as the same C (≈0.09 s):
the checks are hoisted or folded.

---

## 12. The C runtime

`runtime/finch_rt.c` is plain C with no global state. CMake embeds it into the compiler as a raw string
(`rt_source.inc`); on first use `runtimeObject()` compiles it with `$CC -O2 -fPIC -c` into
`~/.cache/finch/rt-<hash>.o`, where the hash covers the source, the Finch version and the C compiler.
It is linked statically into every program.

The shared layout:

```c
typedef struct { char *ptr; int64_t len; int64_t cap; } FStr;   // ptr always NUL-terminated
typedef struct { void *ptr; int64_t len; int64_t cap; } FArr;
// str cap: > 0 heap (owned), 0 literal (static), -1 borrowed from C (copy before keeping)
```

Functions take and return these **by pointer**, so their C ABI is trivial (no struct classification):
string building (`concat`, `from_int/uint/float/char/bool`, `sub`, `trim`, `upper`, `lower`, `replace`,
`split`, `join`, `repeat`, `from_bytes`), comparison (`eq`, `cmp`, `find`, `starts`, `ends`), ownership
(`copy`, `own`, `drop`, `from_c`), arrays (`reserve`, `make`, `resize`, `clone_raw`, `free`,
`insert_gap`, `remove_gap`), `args`, `input`, files, `shell`, and the panics. Allocation failure prints
`out of memory` and exits.

---

## 13. C interop: headers, structs, ABI

### 13.1 Importing headers

`importHeaders(imports, dirs)` parses each `import "x.h"` as its own translation unit from an in-memory
`#include "x.h"`, with `-x c -std=gnu11`, `-I` for every `.fch` file's folder, and `-isystem` for each
directory the system C compiler searches (read from `$CC -E -v -x c /dev/null`, which is what makes it work
on NixOS). `gnu11` because glibc hides `M_PI` and friends in strict ISO mode.

The `Importer` visits top-level cursors:

| Cursor | Result |
|---|---|
| `FunctionDecl` | `CFunc` (signature mapped in **signature context**; `static` and K&R functions recorded as unsupported) |
| `TypedefDecl` of a record | names the record (`typedef struct {…} CXCursor` → `CXCursor`) |
| `StructDecl` (definition) | names the record by its tag |
| `VarDecl` `extern` | `CGlobal` (mapped in **data context**) |
| `EnumConstantDecl`, numeric `MacroDefinition` | `CConst` (literal) |

`mapType()` works on canonical types and maps integers by size. The context decides `char *`: a `str`
in function signatures (Finch converts at the call), but `ptr[char]` inside structs, globals and behind
pointers (where the memory layout must stay a single pointer). Records by value become
`Named{name, "C"}` and are defined with `defineRecord()`: every field (`clang_Type_visitFields`) with its
byte offset, size and alignment, nested records, constant arrays (`Fixed`). Unions and bit fields are kept
as hidden bytes and mark the struct unsupported for by-value calls; pointers to anything else (void,
functions, incomplete structs) become `ptr`.

### 13.2 Laying out C structs

`resolveStruct()` builds a **packed** LLVM struct that mirrors C byte for byte: an `[n x i8]` pad before
each field whose offset is ahead of the current position, the field itself (only if its LLVM size matches
C's), and tail padding up to C's `sizeof`. Field accesses use the recorded `llvmIndex`. Packing makes the
layout exact regardless of LLVM's alignment rules; allocas and temporaries get the C alignment explicitly.

### 13.3 The System V x86-64 calling convention

`classify(T)` (in `abi.cpp`) flattens a struct into scalar leaves with byte offsets (through nested
structs and fixed arrays) and:

- **MEMORY** if it is larger than 16 bytes or has a misaligned leaf: passed as a pointer with
  `byval(T) align ≥8`, returned through a hidden first `sret(T)` pointer.
- otherwise each eightbyte is **INTEGER** if any leaf in it is an integer/pointer, else **SSE**. Parts:
  INTEGER → `iN` for the bytes that are left (`i32` for a 4-byte `Color`), SSE → `double`, `<2 x float>`
  or `float`. One part is passed as is; two parts as two arguments, or returned as `{a, b}`.

`makePlan()` walks the parameters counting the 6 integer and 8 SSE argument registers; a struct whose
parts don't fit any more goes to the stack (MEMORY) as a whole, like clang does. Small integer
parameters get `signext`/`zeroext`. `emitCCall()` stores each struct argument into a slot big enough for
both the struct and its register form, loads the parts at offsets 0 and 8, calls with the planned function
type and attributes, and rebuilds struct results the same way.

`cThunk(fn)` is the reverse for `addr(fn)`: an internal function with the C signature that reassembles
struct parameters from registers or `byval` memory, calls the Finch function and lowers its result. Only
C-compatible signatures are accepted.

`tests/run/c_structs.fch` exercises `{float,float}`, `{float,float,float}`, `{u8×4}`, `{double,int}`,
`{i64,i64}`, a 24-byte struct, a struct with a `char[8]`, nested structs, register exhaustion with 13
struct arguments, and callbacks taking and returning structs, against a C library compiled with
`link "abi.c"`. The results match the same calls made from C.

### 13.4 Linking C code

`link "name"` tries `pkg-config --libs name`, then `-lname`. `link "file.c"` compiles the file (relative
to the `.fch` file) with `$CC -O2 -fPIC -c` into a temporary object; `.o` and `.a` files are passed
through. When linking fails, `linkFailed()` maps undefined symbols back to the header that declared them
and prints the `link` line to add.

---

## 14. Optimization, emission, linking

- **Target:** `sys::getDefaultTargetTriple()`, CPU `generic`, `Reloc::PIC_`. The data layout is set on the
  module before any IR is built (alignment of loads/stores depends on it).
- **Optimization:** `PassBuilder::buildPerModuleDefaultPipeline(O2)`: clang's `-O2` middle end. `-O0`
  skips it (`finch ir file.fch -O0` shows `Codegen`'s raw output).
- **Emission:** legacy `PassManager` + `addPassesToEmitFile(ObjectFile)`.
- **Linking:** `$CC prog.o rt.o [user objects] -lm [libs]`. On Nix the cc wrapper turns `-L` paths into
  `RPATH`, so programs run without `LD_LIBRARY_PATH`.
- **`run`:** a temporary executable, run with the remaining command-line arguments, deleted afterwards;
  a signal is reported as `the program crashed: <name>` with exit status `128 + n`.

Performance notes: user functions are `internal`, so LLVM inlines and specializes them freely
(recursive `fib(40)`: ≈0.19 s vs. ≈0.23 s for `clang -O2` on the same C on the author's machine);
bounds-checked array code matches C on a sieve.

---

## 15. Debug info

With `-g`, `Codegen` creates a `DIBuilder`, a compile unit (`DW_LANG_C`, so gdb's C expression syntax
works for `print p.x`), a `DIFile` per source file, a `DISubprogram` per function, parameter variables
(`createParameterVariable`, so `bt` shows `length (v=…)`) and local variables (`insertDeclare` on their
allocas), and a `DILocation` per statement and call (`setLoc`). Types: basic types with Finch names
(`int`, `u8`, `float`), `str`/arrays as `{ptr, len, cap}` structs, pointers, fixed arrays, and structs with
member offsets from the LLVM `StructLayout`. Recursive structs use a replaceable forward declaration.
Generated helpers carry no locations. Use `-g -O0` for the best experience; with O2 some variables are optimized out.

---

## 16. Diagnostics

`failAt(file, line, col, msg)` prints `path:line:col: error: msg` and the source line with a caret
(tabs preserved, columns counted in characters), then exits with 1. Conventions: binary operators report
the operator, calls the callee, argument errors the argument. Messages say what is wrong and, when
there's an obvious fix, how to fix it. There are no warnings. An LLVM verifier failure prints
`internal compiler error (please report)` and exits with 2.

---

## 17. The self-hosted compiler (`boot/`)

`boot/` is a second Finch compiler written in Finch (≈3,100 lines):

| File | Contents |
|---|---|
| `boot/lexer.fch` | the lexer (tokens are `{kind, text, line, col, nl}`; kinds are words, keywords and operators are their own kind) |
| `boot/ast.fch` | `Type` (`kind`, `elem` as a 0/1-element array, `name`, `module`), one generic `Node` for everything, `Program` |
| `boot/parser.fch` | the same grammar and newline rules as the C++ parser, precedence climbing for binary operators |
| `boot/gen.fch` | type checker + **textual LLVM IR** generator: scopes, places, coercions, copies, helpers, printing, runtime checks, array/str methods |
| `boot/main.fch` | loader for modules and the driver: writes `.ll`, runs `clang -O2 file.ll runtime/finch_rt.c` |

It supports the core language: `int float bool char str`, `[]T`, structs (defaults, named/positional
constructors), `ptr[T]`/`addr`/`new`/`free`/`null`, all statements except `defer`, modules, and the
built-ins and methods the C++ compiler has, minus C imports, sized integers and `defer`.
It implements the same value semantics and borrow analysis (copies where the main compiler copies) but
**never frees**: as a bootstrap compiler it leaves memory to the operating system.

The compiler state is one `Gen` struct passed as `ptr[Gen]`: Finch has no globals, and passing a
pointer is how a function changes its caller's value. Without `defer` or a map type, scopes are arrays of
arrays searched linearly; helper functions (copy/print/compare per type) are generated by temporarily
swapping the output buffers (`beginHelper`/`endHelper`).

**The fixpoint.** `tests/boot.sh`:

1. `build/finch` (C++) compiles `boot/` → **stage 1**,
2. stage 1 compiles `boot/` → **stage 2** (and its IR),
3. stage 2 compiles `boot/` → **stage 3** IR,

and requires the stage 2 and stage 3 IR to be **byte-identical** (≈32,000 lines). It then compiles every
test in `tests/run` that stays within the subset with the self-hosted compiler and compares the output
with the expected one. It takes about 2.3 s for the self-hosted compiler to compile itself.

---

## 18. Build system

- `find_package(LLVM CONFIG)`; **if the shared `LLVM` target exists Finch links that single library.**
  `libclang.so` links `libLLVM.so` itself; linking the static component libraries as well put two copies of
  LLVM in one process, and the duplicated global objects crashed at exit (a double free in a global
  destructor). It also made the binary 82 MB instead of ~20 MB.
- libclang: `find_path(clang-c/Index.h)` + `find_library(clang)`.
- `runtime/finch_rt.c` is read at configure time into `build/rt_source.inc` as a raw string literal;
  `CMAKE_CONFIGURE_DEPENDS` re-runs the configure step when it changes.
- `FINCH_VERSION` from `project(VERSION 2.3.0)`. C++17, `-Wall -Wextra` (MSVC: `/W3`), warning-free.
- `shell.nix` lists `llvmPackages.clang` before `llvmPackages.libclang`: the latter also ships an
  unwrapped `clang` that can't find the system headers.

---

## 19. Testing

- `tests/run.sh`: every `tests/run/*.fch` with `fn main` must print exactly its `.out` (stdin from `.in`
  if present); files without `main` are modules or helpers. Every `tests/fail/*.fch` must fail with the text
  from its `// expect:` line. Currently **59 passed, 0 failed**.
- `MEMCHECK=1 tests/run.sh`: the same, plus valgrind on every program (no leaks, no invalid access).
- `tests/boot.sh`: the self-hosting fixpoint and the subset run (§17).

---

## 20. Semantics vs. C

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

## 21. Function reference

### Driver (`main.cpp`)

| Function | Does |
|---|---|
| `Loader::load` / `findModule` | read, lex, parse a file and recursively its modules / locate `name.fch` |
| `hostMachine`, `optimize`, `emitObject` | target machine, O2 pipeline, object file |
| `runtimeObject` | compile and cache the runtime |
| `capture`, `libFlags`, `link` | run a command / pkg-config or `-l` / the link step, compiling `link "x.c"` files |
| `linkFailed`, `guessLib` | turn linker output into advice |

### Lexer and parser

| Function | Does | Differs from |
|---|---|---|
| `lex` / `tokName` | text → tokens / names for messages | |
| `accept` / `expect` / `unexpected` | consume if it matches / consume or fail / fail with context | |
| `sameLine` / `endOfStatement` | may this token continue the expression? / did the statement end? | |
| `atDeclaration` / `type` | lookahead for a declaration / parse a type | |
| `args` | `( [name:] expr, … )` for calls, methods and constructors | |
| `postfix` | `.field`, `.method(…)`, `[i]` chains | `primary` parses the start of a chain |

### Codegen: program and statements (`codegen.cpp`)

| Function | Does | Differs from |
|---|---|---|
| `declareStructs` / `declareFns` / `declare` | all types / all signatures / one signature + borrow analysis | |
| `define` / `defineMainWrapper` | one body / the C entry point | |
| `pushScope` / `popScope` | open / close a scope (`popScope` emits its cleanups if reachable) | |
| `emitCleanups(downTo)` / `emitScopeCleanups(i)` | cleanups of several scopes without popping (return/break) / of one scope | |
| `slot` / `tmpOf` / `tmp` | a variable's alloca / an anonymous one / an anonymous one holding a value | |
| `lookup` / `addVar` | find a visible variable (respects `deferLimit`) / declare one | |
| `stmt`, `varDecl`, `assign`, `ifStmt`, `whileStmt`, `forStmt`, `forEachStmt`, `returnStmt`, `jump` | statements | |
| `rootVar` / `mutates` / `mutatesExpr` | the variable an l-value starts from / does a statement or expression change it? | |

### Codegen: expressions

| Function | Does | Differs from |
|---|---|---|
| `expr` / `exprWant` | an r-value / the same, with an expected type for array literals | |
| `ref` / `place` | an address if any, else a value / an address or an error | `expr` loads from `ref` |
| `member` / `index` | `.x`, `.len`, `.ptr`, `.value`, auto-deref / `[i]` with bounds checks and copy-on-write | |
| `arrayLit` | `[…]` → heap array | |
| `unary`, `binary`, `arith`/`arithOp`, `compare`, `logic` | operators (`arith` adds the literal flag) | |
| `unify` / `coerce` / `convert` | two operands toward each other / one value into a slot / explicit `T(x)` | |

### Codegen: types and ownership (`codegen_types.cpp`)

| Function | Does |
|---|---|
| `ty`, `resolve`/`resolveT`, `findStruct`, `resolveStruct` | LLVM type; Named → Struct (deep or shallow); struct lookup; layout + cycle check |
| `owning`, `zero`, `defaultValue` | does a type own memory; zero constant; zero or the struct's defaults |
| `own` / `release` | something storable (move or copy) / drop an unkept fresh value |
| `copyValue`, `dropValue`, `copyAt`, `dropAt`, `copyFn`, `dropFn`, `typeKey` | copying and dropping by value, by address, and the per-type helpers |
| `forN` | an internal `for i in 0..n` loop used by helpers |
| `strConst`, `cstr`, `strFromC`, `elemSize` | literal str constant; str → `char*`; `char*` → borrowed str; element size |
| `emitPrint`, `printFn`, `printf_` | printing |
| `diType`, `setLoc` | debug info |

### Codegen: calls (`codegen_builtins.cpp`) and ABI (`abi.cpp`)

| Function | Does |
|---|---|
| `call` / `method` | resolve a call / a method or module-qualified call |
| `callFinch` / `construct` / `callC` | Finch function / struct constructor / C function |
| `print`, `addrOf`, `convert`, `arrayMethod` | built-ins |
| `rt` / `libc` | declare a runtime / libc function |
| `fileName`, `panicIf`, `boundsCheck`, `checkDivisor` | runtime checks |
| `classify`, `makePlan`, `declareC`, `emitCCall`, `cThunk` | System V classification, register plan, declaration, call, callback thunk |

---

## 22. Extending the compiler

**A built-in function:** add the name to `isBuiltin()`, handle it in `call()`, implement it (in IR, or as
a runtime function declared in `rt()`'s table), add `tests/run/` and `tests/fail/` cases. If it should
exist in the self-hosted compiler too, add it to `boot/gen.fch`'s `call()` and `runtimeDecls()`.

**An array or str method:** add a branch in `arrayMethod()` or the str part of `method()`; if it changes
the array, add its name to the `changing` sets in `method()` and `mutatesExpr()`, so borrowing stays correct.

**An operator:** lexer token + `tokName`, `BinOp`, the right precedence function in the parser, `opName`,
emission in `arithOp`/`compare`, an explanation in `badOperands`.

**A statement:** `StmtKind` + node, `Parser::statement`, `Codegen::stmt`; if it transfers control, respect
`terminated()`, use `continueAt()` for merge blocks, and emit cleanups (`emitCleanups`) before jumping out of scopes.

**A new owning type:** extend `owning()`, `dropAt`/`copyAt` (and their helpers), `emitPrint`, `diType`.

---

## 23. Targets and Windows

`src/target.h` holds `g_target`: the LLVM triple, `windows` / `msvc` / `cross` flags, the C compiler (`cc`)
and the executable suffix. `setTarget()` picks the C compiler: `FINCH_CC`, else `CC` (not when
cross-compiling), else `x86_64-w64-mingw32-gcc` for `--target windows` from Linux, `clang` on Windows,
`cc` on Unix. Everything platform-dependent goes through it:

- **Headers:** libclang parses with `--target=<triple>` (so `long` is 32 bits on Windows and the right
  headers are used) and, on Windows, `-D_USE_MATH_DEFINES`. Include folders come from `<cc> -E -v`.
- **Processes:** `capture()` / `runCommand()` wrap `popen` / `system` (`_popen` on Windows) and decode exit
  statuses (POSIX `WIFEXITED`/signals, or Windows exit codes and NTSTATUS crashes like `0xC0000005`).
  `shellQuote()` quotes for `sh` (`'…'`) or `cmd.exe` (`"…"`); a command line handed to `cmd /c` that
  starts with a quote gets one extra pair of quotes.
- **Linking:** no `-lm` with MSVC; `.exe` names; the runtime cache key includes the triple and the C compiler.
  MSVC linker messages (`LNK2019 unresolved external symbol`, `LNK1104`/`LNK1181 cannot open file`) are
  translated like GNU ld's.
- **Running:** a Windows program built on Linux runs with `wine`.
- **Debug info:** CodeView for MSVC targets, DWARF elsewhere.
- **Runtime:** no `getline` or `sys/wait.h` on Windows; `stdin`/`stdout`/`stderr` are macros there, so
  `finch_std_stream(i)` gives them to Finch when a header has no `extern` variable for them.
- **Printing pointers** is the same everywhere: `null` or `0x…`.

### The Microsoft x64 calling convention

`classify()` switches on `g_target.windows`. A struct of exactly 1, 2, 4 or 8 bytes travels as **one
integer** of that size, even if it holds floats (`{float, float}` → `i64`). Any other struct is
**Indirect**: the caller copies it to its own stack and passes the copy's address (no `byval`: the callee
receives a plain pointer). Results of 1/2/4/8 bytes come back in `rax` as an integer; anything else
through a hidden `sret` pointer. Each argument takes one slot, so there is no register counting.
`cThunk()` mirrors this for callbacks.

`tests/windows.sh` builds every test with `--target windows`, runs it with Wine and compares the output:
all of them pass, including `c_structs` (which then exercises the Microsoft rules against a C library built
with MinGW). CI additionally builds `finch.exe` with MSVC on Windows Server and runs `tests/run.sh` there.

### Building finch.exe

CI uses Visual Studio 2022, Ninja and the official `clang+llvm-21.x-x86_64-pc-windows-msvc` package
(static libraries, `/MT`, so `CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded`). Two Windows details in
`CMakeLists.txt`: that package names the DIA SDK library at the path of the machine LLVM was built on, so
it is redirected to the installed Visual Studio; and the embedded runtime is written as several raw
string pieces, because MSVC limits one literal to 16 KB. LLVM is initialized with the native target only,
so the static build doesn't pull in every backend.

---

## 24. The language server

`finch lsp` (`src/lsp.cpp`) speaks LSP over stdin/stdout with `llvm::json`. On `didOpen`/`didChange`
it runs the real front end on the document: `Loader` (with the open documents' text as overrides),
`importHeaders` (cached per set of imports, since reading headers is the slow part), and `generate()`
without optimizing or emitting. Two switches make the compiler usable inside a server:

- `g_throwErrors`: `failAt()` throws `FinchError{file, line, col, msg}` instead of printing and exiting.
  The server publishes it as a diagnostic (for the right file, also inside an imported module) and clears
  diagnostics that disappeared.
- `g_index`: while generating code, `Codegen::note()` records a `SymRef` for every name it resolves
  (variables, parameters, fields, functions, constructors, C functions and constants, built-ins, methods,
  module members): its span, a markdown hover, and the definition position. `define()` and
  `declareStructs()` add `FnInfo` / `StructIndex`; `addVar()` adds `VarInfo` with the enclosing function's
  line range. Positions of names come from `namePos` fields the parser records.

Requests use that index:

| Request | From |
|---|---|
| hover, definition | the `SymRef` under the cursor |
| completion | keywords, types, built-ins, variables of the enclosing function, functions, structs, imported modules, C names matching the typed prefix; after `name.`: struct fields (also through `ptr[T]`), array/str methods, or a module's members |
| signatureHelp | scans back to the unmatched `(`, counts top-level commas, looks up the Finch / C / built-in signature |
| documentSymbol | functions and structs (with fields) of the file |

Half-typed code (`p.`, `add(1, `) doesn't compile, so the server keeps the **last successful** analysis of
each document and merges its functions, structs and variables in for completion and signature help.
`tests/lsp_test.py` drives the server like an editor and checks every feature.

`src/builtins_doc.h` is the single description of built-ins and methods, used for hover, completion and
signature help (and mirrored by the cheat sheets).

---

## 25. The VS Code extension

`editors/vscode` is a plain JavaScript extension: `extension.js` starts `finch lsp` through
`vscode-languageclient` (path from the `finch.path` setting) and adds **Run** / **Build** as
`ProcessExecution` tasks (no shell quoting, the `$finch` problem matcher turns `file:line:col: error:`
lines into Problems). `syntaxes/finch.tmLanguage.json` is the TextMate grammar, tested by `npm test` with
`vscode-textmate` + `vscode-oniguruma` (the engine VS Code itself uses);
`language-configuration.json` gives comments, brackets, auto-closing and indentation; `snippets/` the
templates. CI packages it with `vsce` into `finch-lang.vsix`.

## 26. Known limitations

- No methods on structs, generics, maps, `match`, closures, error values (`int("x")`, `read_file` panic).
- Strings are bytes: `.len`, `s[i]`, `upper()` are not Unicode-aware.
- Dangling pointers (`addr` of a local that went away, use after `free`) are not detected.
- Shift amounts and float→int conversions are not range-checked (LLVM poison, as in C).
- C: unions and bit-field structs by value, function-like macros, `long double`.
- Two systems: Linux and Windows, both x86-64 (System V and Microsoft x64 ABIs). No macOS or ARM yet.
- The self-hosted compiler has no C imports, sized integers or `defer`, and doesn't free memory.
