# Finch for Engineers

**The complete internals of the Finch 1.0 compiler.** It covers how source becomes tokens,
tokens become an AST, and the AST becomes LLVM IR, which is then optimized, emitted and
linked. It describes the exact type rules, how every construct lowers to IR (with real
compiler output), how C headers are imported through libclang, the ABI details, and what
each function in the compiler is for and how it differs from its neighbours.

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
6. [Semantic model](#6-semantic-model)
7. [Type system](#7-type-system)
8. [Code generation](#8-code-generation)
9. [Runtime checks](#9-runtime-checks)
10. [C interop via libclang](#10-c-interop-via-libclang)
11. [Optimization, emission, linking](#11-optimization-emission-linking)
12. [Diagnostics](#12-diagnostics)
13. [Build system](#13-build-system)
14. [Testing](#14-testing)
15. [Semantics vs. C: defined and undefined behavior](#15-semantics-vs-c-defined-and-undefined-behavior)
16. [Function reference: who does what](#16-function-reference-who-does-what)
17. [Extending the compiler](#17-extending-the-compiler)
18. [Known limitations and roadmap](#18-known-limitations-and-roadmap)

---

## 1. Architecture

Finch is a classic single-pass front end in front of LLVM. Type checking and IR generation
are one walk over the AST. There is no separate semantic pass and no intermediate IR of its own.

```
 file.fn
    │  readFile()                                    src/main.cpp
    ▼
 lex()  ──────────► std::vector<Token>               src/lexer.cpp
    ▼
 parse() ─────────► Program (AST)                    src/parser.cpp, src/ast.h
    │                 ├─ imports ──► importHeaders() ──► CImports    src/cimport.cpp (libclang)
    │                 └─ links
    ▼
 hostMachine() ───► llvm::TargetMachine (triple, data layout)
    ▼
 generate() ──────► llvm::Module  (type check + IRBuilder, then verifyModule)   src/codegen.cpp
    ▼
 optimize() ──────► PassBuilder default<O2> pipeline        (skipped with -O0)
    ▼
 emitObject() ────► file.o   (legacy::PassManager + addPassesToEmitFile)
    ▼
 link() ──────────► cc file.o -o file -lm [-l… | pkg-config --libs …]
    ▼
 executable  (finch run: executed from a temp file, then deleted)
```

Design decisions that shape everything else:

- **Fail fast.** The first error prints a diagnostic and calls `exit(1)` (`fail()` in `src/error.h`).
  There is no error recovery, so the AST and codegen never deal with half-valid input.
- **One walk.** `Codegen` carries the scope, loop and function state. Every expression returns
  its IR value and its Finch type together (`Value_`).
- **Let LLVM do the work.** Locals are `alloca`s, and `mem2reg`/SROA promote them to SSA.
  Finch never builds SSA or φ-nodes itself, except for `&&`/`||`.
- **The system C toolchain is the runtime.** `printf`, `strcmp`, `dprintf`, `exit` come from libc.
  There is no Finch runtime library.

---

## 2. Source tree

| File | Lines | Responsibility |
|---|---|---|
| `src/error.h` | ~25 | `fail(line, col, msg)`: prints `file:line:col: error:` and a caret excerpt, then exits. Holds `g_file`, `g_source`. |
| `src/lexer.h/.cpp` | ~240 | `Tok` enum, `Token`, `lex()`, `tokName()` |
| `src/ast.h` | ~235 | `Type`, all `Expr`/`Stmt` nodes, `FnDecl`, `Import`, `Link`, `Program` |
| `src/parser.h/.cpp` | ~430 | Recursive-descent `Parser`, `typeFromName()` |
| `src/cimport.h/.cpp` | ~280 | libclang header import: `importHeaders()` → `CImports` (functions, constants, globals) |
| `src/codegen.h/.cpp` | ~900 | `Codegen`: type checking + LLVM IR generation, `generate()` |
| `src/main.cpp` | ~260 | Driver: CLI, target machine, optimization, object emission, linking, link-error analysis, `run` |
| `tests/run.sh` | | Golden-output and expected-failure tests |

---

## 3. Lexer

`lex(src, file)` makes one linear pass over the source and returns a `std::vector<Token>`
terminated by `Tok::End`.

```cpp
struct Token {
    Tok kind;
    std::string text;      // identifier / literal contents (escapes already decoded)
    int line, col;         // 1-based
    bool newlineBefore;    // a newline occurred between the previous token and this one
};
```

### Newlines as statement terminators

Finch has no semicolons. Instead of emitting `NEWLINE` tokens, as Python and Go do internally,
the lexer **marks** each token with `newlineBefore`. The parser decides what that means
(section 4). This keeps the grammar free of newline tokens, which would otherwise have to be
skipped everywhere: in argument lists, between `}` and `else`, and so on.

### Tokens

- **Positions:** `line` and `col` are 1-based, and `col` counts **characters**, not bytes:
  UTF-8 continuation bytes do not advance it, so carets line up under text such as `"Błąd"`.
  An unexpected non-ASCII character is reported whole (`'ż'`), with a hint that names are ASCII-only.
- **Identifiers / keywords:** `[A-Za-z_][A-Za-z0-9_]*`. Keywords: `fn return if else while for in break continue true false null import link`.
  **Type names are not keywords.** `int`, `u8`, `ptr`… lex as `Ident`, and the parser recognises them through `typeFromName()`.
  This keeps the lexer independent of the type list, and lets `u8(x)` parse as an ordinary call.
- **Integers:** decimal or `0x` hex, with `_` separators (removed from `text`). Leading zeros are decimal.
  Finch has no octal, so `010` is 10. Value parsing happens in the parser (`strtoull`, base 10 or 16).
- **Floats:** `digits.digits`. A dot is only part of a number when **followed by a digit**,
  so `0..10` lexes as `Int DotDot Int`, not `Float(0.) Dot …`.
- **Strings / chars:** `"…"` and `'…'`. Escapes `\n \t \r \0 \\ \" \'` are decoded into `text`.
  An unknown escape, an unterminated literal or a newline inside a literal is an error. A char literal must decode to exactly one byte.
- **Operators:** maximal munch: `->` before `-`, `<<` and `<=` before `<`, `&&` before `&`, `..` before `.`, `:=` (a lone `:` is an error that suggests `:=`).
- **Comments:** `//` to end of line, `/* … */` (not nested; an unterminated one is an error).

---

## 4. Grammar and parser

`Parser` (`src/parser.cpp`) is a hand-written recursive-descent parser with one token of
lookahead (`cur()`), plus a second (`peekTok()`) in two places.

### Grammar (EBNF)

```ebnf
program     = { import | link | function } ;
import      = "import" STRING ;
link        = "link" STRING ;
function    = "fn" IDENT "(" [ param { "," param } ] ")" [ "->" type ] block ;
param       = type IDENT ;
type        = TYPENAME | "ptr" "[" type "]" ;          (* TYPENAME: int float bool char str ptr i8..u64 f32 f64 *)

block       = "{" { statement TERMINATOR } "}" ;
statement   = if | while | for | return | "break" | "continue" | block
            | type IDENT [ "=" expr ]                  (* declaration *)
            | IDENT ":=" expr                          (* inferred declaration *)
            | target assignop expr                     (* target: IDENT or postfix ending in .value *)
            | call ;
if          = "if" expr block [ "else" ( if | block ) ] ;
while       = "while" expr block ;
for         = "for" IDENT "in" expr ".." expr block ;
return      = "return" [ expr ] ;                      (* expr only if on the same line *)
assignop    = "=" | "+=" | "-=" | "*=" | "/=" | "%=" ;

expr        = or ;
or          = and { "||" and } ;
and         = cmp { "&&" cmp } ;
cmp         = add { ( "==" | "!=" | "<" | "<=" | ">" | ">=" ) add } ;
add         = mul { ( "+" | "-" | "|" | "^" ) mul } ;
mul         = unary { ( "*" | "/" | "%" | "<<" | ">>" | "&" ) unary } ;
unary       = ( "-" | "!" | "~" ) unary | postfix ;
postfix     = primary { "." IDENT } ;
primary     = INT | FLOAT | STRING | CHAR | "true" | "false" | "null"
            | IDENT [ "(" [ expr { "," expr } ] ")" ]
            | "(" expr ")" ;
```

### The statement terminator rule

`TERMINATOR` is not a token. After every statement, `endOfStatement()` requires that the next
token is `}`, end-of-file, or has `newlineBefore == true`. Otherwise the error is
`put each statement on its own line`.

Inside expressions, `sameLine()` decides whether a binary operator, `.`, or a call's `(`
**continues** the current expression:

```cpp
bool sameLine() const { return parenDepth > 0 || !cur().newlineBefore; }
```

So:

```c
a := 1 +        // '+' is on this line → the expression continues to the next line
     2
b := (1         // inside parentheses (parenDepth > 0) newlines are ignored
      + 2)
c := 1
-5              // '-' starts a new line → the statement ended at '1'; "-5" is then an error
```

`parenDepth` is incremented around `( … )` groups, argument lists and parameter lists. It is
**saved and reset to 0** when entering a `{ }` block, so a block inside a parenthesised
context, which does not occur yet, would still use line rules.

A call's `(` must be on the same line as the callee. Otherwise `f⏎(x)` would silently parse as a call.

### Precedence: Go's, not C's

| Level | Operators |
|---|---|
| 5 | `*` `/` `%` `<<` `>>` `&` |
| 4 | `+` `-` `\|` `^` |
| 3 | `==` `!=` `<` `<=` `>` `>=` |
| 2 | `&&` |
| 1 | `\|\|` |

All binary operators are left-associative. Comparisons are also parsed left-associatively
(`a < b < c` parses), but the type checker then rejects comparing `bool < int`.
C puts `&`, `^`, `|` **below** `==`, which makes `x & MASK == 0` mean `x & (MASK == 0)`.
Finch adopts Go's table, which removes that whole class of bugs.

### How statements are recognised

`statement()` dispatches on the first token:

1. Keywords (`if`, `while`, `for`, `return`, `break`, `continue`, `{`) dispatch directly.
   `fn`, `import` and `link` inside a block produce targeted errors.
2. **Typed declaration:** `atDeclaration()` returns true when the current token is a type name
   and the next is an identifier (`int x`), or when it is `ptr` followed by `[` (`ptr[int] p`).
   This is the only place that needs two-token lookahead.
3. **Inferred declaration:** `IDENT` followed by `:=`.
4. Otherwise the parser **parses a full expression first**, then looks at the next token. If it is
   an assignment operator on the same line, the expression becomes the assignment target. It must
   be a `VarExpr` or `MemberExpr`, otherwise: `only a variable or p.value can be assigned to`.
   This "expression, then decide" approach lets arbitrary l-value forms (`pp.value.value = 1`)
   work without a separate l-value grammar.
5. A bare expression statement must be a `CallExpr`. Anything else is
   `this value is computed but never used`.

### Top level

`program()` loops over `import`, `link` and `fn`. A top-level item that starts with a type name
gets the hint `functions start with 'fn', like: fn add(int a, int b) -> int { ... }`, which catches
C-style function definitions. `link` arguments are validated: `"libfoo.so"` or a path is
rejected with a suggestion for the bare name.

---

## 5. AST

All nodes live in `src/ast.h`. Expressions and statements are small class hierarchies with
an explicit `kind` tag. Consumers `switch` on `kind` and `static_cast`, which avoids RTTI and
`dynamic_cast` and keeps dispatch obvious.

```cpp
struct Expr { ExprKind kind; Pos pos; virtual ~Expr(); };
using ExprPtr = std::unique_ptr<Expr>;
```

| ExprKind | Node | Fields |
|---|---|---|
| `Int` | `IntExpr` | `long long value` |
| `Float` | `FloatExpr` | `double value` |
| `Bool` / `Char` / `Str` | `BoolExpr` / `CharExpr` / `StrExpr` | `value` |
| `Null` | `NullExpr` | |
| `Var` | `VarExpr` | `name` |
| `Unary` | `UnaryExpr` | `char op` (`-` `!` `~`), `operand` |
| `Binary` | `BinaryExpr` | `BinOp op`, `lhs`, `rhs` |
| `Call` | `CallExpr` | `std::string callee`, `args` |
| `Member` | `MemberExpr` | `obj`, `field` (only `value` is meaningful in 1.0) |

| StmtKind | Node | Fields |
|---|---|---|
| `Block` | `BlockStmt` | `std::vector<StmtPtr> body` |
| `VarDecl` | `VarDeclStmt` | `hasType`, `type`, `name`, `init` (nullable if `hasType`) |
| `Assign` | `AssignStmt` | `target` (Var/Member), `char op` (`= + - * / %`), `value` |
| `Expr` | `ExprStmt` | `expr` (always a call) |
| `If` | `IfStmt` | `cond`, `then`, `otherwise` (null, `BlockStmt`, or nested `IfStmt` for `else if`) |
| `While` | `WhileStmt` | `cond`, `body` |
| `For` | `ForStmt` | `var`, `from`, `to`, `body` |
| `Return` | `ReturnStmt` | `value` (nullable) |
| `Break` / `Continue` | | |

Calls store the callee **by name**, not as an expression. Finch has no first-class functions,
and resolving names at codegen time lets one `CallExpr` mean a built-in (`print`, `addr`),
a conversion (`u8`), a Finch function or a C function (section 8.10).

### Example

```c
x += add(x, 2)
```

parses to:

```
AssignStmt (op '+')
├─ target: VarExpr "x"
└─ value:  CallExpr "add"
           ├─ VarExpr "x"
           └─ IntExpr 2
```

There is **no "load" node in the AST**. Whether a `VarExpr` means "the address of x" or "the
value in x" depends on where it appears. Codegen decides that (`place()` vs `expr()`, section 8.2).

### `Type`

```cpp
struct Type {
    enum Kind { Void, Bool, Char, I8, I16, I32, I64, U8, U16, U32, U64, F32, F64, Str, Ptr, Null };
    Kind kind;
    std::shared_ptr<Type> elem;   // Ptr only: pointee; null = untyped `ptr`
};
```

- `int` and `i64` are **the same type** (`I64`), and `name()` prints it as `int`. Likewise `float` is `F64`.
- `Null` is the type of the `null` literal only. No variable can have it.
- `Void` is the "type" of calls that return nothing, and of functions without `->`.
- Equality is structural (`ptr[ptr[int]] == ptr[ptr[int]]`).
- Kinds are ordered so that `isInt()` is `I8..U64`, `isSigned()` is `I8..I64`, `isUnsigned()` is `U8..U64`.

---

## 6. Semantic model

All semantic checking happens inside `Codegen` while it emits IR.

### Functions

`Codegen::run()` runs two passes over `Program::fns`:

1. `declare()` creates every `llvm::Function` (signature only), so **definition order does not matter**
   and mutual recursion works. It also checks: duplicate names, built-in names, and `main`'s
   signature (no parameters; returns nothing, `int` or `i32`).
2. `define()` generates each body.

Without a `main`, compilation fails with `there is no main function`.

### Scopes

`scopes` is a `std::vector<std::unordered_map<std::string, Var>>`. A new map is pushed for
the function's parameters, for every `{ }` block, and for every `for` loop (which holds the
loop variable).

```cpp
struct Var { AllocaInst *slot; FType type; bool readonly; };
```

**No shadowing:** `addVar()` rejects a name that `lookup()` finds in *any* enclosing scope,
not just the current one. It also rejects names of functions and built-ins. This is a
deliberate simplicity rule: a name means one thing in a whole function body.

**Read-only:** `for` variables are `readonly`. Assigning to them, or taking `addr()` of them,
is an error, so the loop counter cannot be changed behind the loop's back.

### Name resolution for identifiers

For a `VarExpr name` in value position, the order is:

1. a local variable (`lookup`),
2. a C constant (`cimports.consts`: enum values, `#define` numbers),
3. a C global (`cimports.globals`: `stdout`, …),
4. error `there is no variable named 'name'`.

For calls, the order is: `print` → `addr` → type name (conversion) → Finch function → C function → error.
Since Finch functions are checked before C functions, **a Finch function shadows a C function
of the same name**. The Finch one is emitted as `finch.<name>`, so both symbols coexist at link time.

---

## 7. Type system

### LLVM representation

| Finch | LLVM | Notes |
|---|---|---|
| `bool` | `i1` | |
| `char`, `i8`, `u8` | `i8` | signedness lives in the Finch type, not in LLVM |
| `i16`, `u16` | `i16` | |
| `i32`, `u32` | `i32` | |
| `int`/`i64`, `u64` | `i64` | |
| `f32` | `float` | |
| `float`/`f64` | `double` | |
| `str` | `ptr` | NUL-terminated C string |
| `ptr`, `ptr[T]` | `ptr` | LLVM 21 has opaque pointers; the pointee type exists only in the Finch type |
| `null` | `ptr` | `ConstantPointerNull` |
| nothing | `void` | |

LLVM integers are signless. Every operation where signedness matters (`sdiv`/`udiv`,
`srem`/`urem`, `ashr`/`lshr`, `icmp slt`/`ult`, `sext`/`zext`, `sitofp`/`uitofp`,
`fptosi`/`fptoui`) chooses the instruction from the **Finch** type.

### Values in codegen

```cpp
struct Value_ {
    Value *v;          // the IR value
    FType type;        // its Finch type
    bool literal;      // written as a number in the source (or an imported C constant)
};
```

### Literals ("untyped constants")

`literal` is set on `IntExpr`, `FloatExpr` and imported C constants. It is preserved through
unary minus and through arithmetic **between two literals** that folds to a constant
(`arith()` sets `literal = l.literal && r.literal && isa<Constant>(result)`). It is **cleared by
explicit conversions**: `u8(250)` is a typed `u8` constant, not a literal.

A literal integer may become **any integer type it fits in** (`fits()` checks the range,
accounting for the source literal's signedness). This is what makes `u8 b = 200`,
`malloc(4 * 10)` (to `u64`) and `glClear(GL_COLOR_BUFFER_BIT)` (an `i64` macro to a `u32` parameter)
work without casts, while `u8 b = 300` is rejected with `the number 300 doesn't fit in u8`.
A literal `f64` may become `f32` (`f32 h = 0.5`).

Why not simply check `isa<ConstantInt>`? `IRBuilder` constant-folds, so `u8(250)` is also a
`ConstantInt`. If "is a constant" were the rule, `u8(250) + 10` would quietly widen to
`int` and give 260, while the same code with a `u8` variable would wrap to 4. The explicit
flag makes the behavior depend on what you *wrote*, not on what the optimizer could see.

### Implicit conversions: `coerce(value, want, pos, what)`

Used wherever a value flows into a slot of known type: initializers, assignments, arguments,
return values, conditions (`want = bool`), range bounds (`want = int`). Allowed, in order:

1. identical types,
2. `null` → any `ptr…` or `str`,
3. any `ptr…` or `str` → untyped `ptr`, and untyped `ptr` → any `ptr[T]` (the `void*` rules; this is how `ptr[i32] n = malloc(4)` works),
4. int → int when the value is a fitting literal, **or** `widens(from, to)`:
   - same signedness, strictly more bits, **or**
   - unsigned → signed with strictly more bits,
   - (signed → unsigned never widens),
5. any int → any float (`sitofp`/`uitofp`),
6. `f32` → `f64` (`fpext`),
7. literal `f64` → `f32` (`fptrunc`, constant-folded).

Anything else is an error. The message suggests the explicit conversion
(`use i32(...) to convert`) and warns when it would truncate (`it cuts off the fraction`).

### Binary operands: `unify(l, r)`

Before arithmetic or comparison, both sides are brought to one type:

1. same type → done,
2. both ints: a literal side adapts to the other side if it fits; otherwise the side that
   `widens()` into the other is extended; otherwise **error** (`i32` + `u32`: neither widens
   into the other, and Finch refuses to guess),
3. int and float → the int converts to that float type,
4. `f32` and `f64` → both `f64`.

The result type of arithmetic is the unified type. **There is no C-style integer promotion:**
`u8 + u8` is computed in `u8` and wraps. This is deliberate. It is what the sized type asks for,
and it matches Rust and Zig.

### Explicit conversions: `convert(call, to)`

`T(x)` is parsed as a call named after a type and lowered here:

| From → To | Instruction |
|---|---|
| int/char → int/char | `CreateIntCast(signed = from.isSigned())` → `trunc` / `sext` / `zext` |
| bool → int/char | `zext` |
| float → int | `fptosi` / `fptoui` (by target signedness) |
| int → float | `sitofp` / `uitofp` (by source signedness) |
| float → float | `fpext` / `fptrunc` (`CreateFPCast`) |
| int → bool | `icmp ne x, 0` |
| ptr/str/null → ptr…, ptr → str | no instruction (all are `ptr`) |

`fptosi` of an out-of-range value is poison in LLVM (UB in C). Finch 1.0 does not check this. See section 15.

### Comparisons: `compare()`

- **Pointers:** if either side is a `ptr…` or `null`, only `==`/`!=` are allowed and they compile to
  `icmp eq/ne ptr`. A `str` compared with `null` also compares addresses.
- **`str == str`:** a call to `strcmp`, then `icmp eq/ne i32 %r, 0` (content equality). `<`/`>` on `str` are rejected.
- **Floats:** `oeq olt ole ogt oge` (ordered: false if NaN) and `une` for `!=` (unordered: `NaN != NaN` is true). This matches C.
- **Integers / char:** `eq ne`, and `slt sle sgt sge` or `ult ule ugt uge` by signedness. `char` compares unsigned.
- **bool:** only `==`/`!=`.

---

## 8. Code generation

`Codegen` owns an `llvm::IRBuilder<> b`, the `Module`, the function table `fns`
(`name → {Function*, const FnDecl*}`), `scopes`, the `loops` stack and `curFn`.

### 8.1 Variables are stack slots

Every variable, **including parameters**, gets an `alloca` created by `slot()`.
`slot()` always inserts at the **top of the entry block**, using a second `IRBuilder` positioned at
`entry.begin()`, no matter where the declaration appears. Allocas in the entry block are what
`mem2reg`/`SROA` can promote to SSA registers, so the O2 output contains no stack traffic for
scalars. Because each new slot goes to the very beginning, slots appear in **reverse declaration
order** in `-O0` IR (`%b2` before `%a1` below).

Parameters are copied into their slots at function entry (`store %a, %a1`), which makes
parameters ordinary mutable locals.

### 8.2 Loads and stores: `expr()` vs `place()`

This is the heart of how memory access is generated:

- `place(e)` returns a **`Place {Value *addr; FType type}`**, the *address* of an l-value.
  - `VarExpr` → the variable's `alloca` (or the C global's `GlobalVariable`).
  - `MemberExpr` `p.value` → `expr(p)` (which **loads the pointer** from p's slot), plus a null check
    (section 9). The address is the loaded pointer itself, and the type is `*p.type.elem`.
- `expr(e)` returns a **`Value_`**, an r-value. For `Var` and `Member` it calls `place()` and then
  emits **one `load`** of `ty(place.type)` from `place.addr`.

So the `load` for an identifier is created in `Codegen::expr()`, case `ExprKind::Var`, as
`b.CreateLoad(ty(pl.type), pl.addr, v.name)`. Naming it after the variable is why `-O0` IR shows
`%x1`, `%x2` and so on (LLVM uniquifies repeated names). `p.value` costs **two** loads: one for the
pointer, one for the pointee. `pp.value.value` costs three.

Stores are generated in exactly three places: `addVar()` (initialization), `assign()`, and the `for` step.

### 8.3 Assignment and evaluation order

```c
x += add(x, 2)
```

`assign()` computes `place(target)` **first**. For compound operators it then **loads the current
value**, then evaluates the right-hand side, then calls `arith()`, then `coerce()`s back to the
target type, then stores. Real `-O0` output:

```llvm
define i32 @main() {
entry:
  %x = alloca i64, align 8
  store i64 5, ptr %x, align 8                       ; x := 5
  %0 = load i64, ptr %x, align 8                     ; current x (for +=)
  %x1 = load i64, ptr %x, align 8                    ; argument x
  %1 = call i64 @finch.add(i64 %x1, i64 2)
  %2 = add i64 %0, %1
  store i64 %2, ptr %x, align 8
  %x2 = load i64, ptr %x, align 8
  %3 = call i32 (ptr, ...) @printf(ptr @fmt, i64 %x2)
  ret i32 0
}
```

Order of evaluation in Finch is **always left to right**: the target, then the operands of a
binary operator left to right, then arguments left to right. C leaves most of this unspecified.
Note that for `x += f()` the old value of `x` is read **before** `f()` runs. If `f` modified `x`
through a pointer, the modification would be overwritten. That is the same as `x = x + f()`
read left to right.

After O2 the whole program above is constant-folded into `printf(@fmt, i64 12)`.

### 8.4 Functions, linkage and `main`

```llvm
define internal i64 @finch.add(i64 %a, i64 %b) {
entry:
  %b2 = alloca i64, align 8
  %a1 = alloca i64, align 8
  store i64 %a, ptr %a1, align 8
  store i64 %b, ptr %b2, align 8
  %a3 = load i64, ptr %a1, align 8
  %b4 = load i64, ptr %b2, align 8
  %0 = add i64 %a3, %b4
  ret i64 %0
}
```

- User functions are named **`finch.<name>`** with **internal linkage**. The dot cannot appear in a C
  identifier, so a Finch function can never collide with a libc or library symbol, and internal
  linkage lets LLVM inline, specialise or delete them freely. In C, a non-`static` function is
  external and must be kept as written. This likely contributes to recursive `fib(40)` running
  slightly faster than the same C code at `clang -O2` (about 0.19 s vs 0.23 s on the author's machine).
- `main` is emitted as **`i32 @main()` with external linkage**. A `fn main()` returns `i32 0` on every
  exit path, including an explicit `return`. A `fn main() -> int` truncates its `i64` result with
  `CreateIntCast(…, i32, signed)`.
- Missing return: after the body, if the current block has no terminator, `void` functions get
  `ret void` (or `ret i32 0` for `main`). Non-void functions are a compile error:
  `function 'f' can reach its end without returning int`. This is checked on the CFG that was
  actually built (section 8.5), so `if/else` that returns on both branches is accepted without any
  extra analysis.
- Arithmetic is emitted **without `nsw`/`nuw` flags**, so integer overflow is defined two's-complement
  wraparound (section 15).

### 8.5 Control flow and the "terminated" state

The builder's insert block is the single source of truth for reachability:

```cpp
bool terminated() { return b.GetInsertBlock()->getTerminator() != nullptr; }
```

After `return`, `break` or `continue`, the current block has a terminator.
`blockBody()` checks `terminated()` **before each statement**. If it is set, the statement is
unreachable and that is a compile error (`this code can never run`). No dead blocks are ever created
for code after a jump.

`continueAt(bb, deadEnd)` is used after `if` and `while`. If the merge block `bb` has **no
predecessors** (all paths returned or broke), it is erased and the builder is left in `deadEnd`,
which is terminated. So the "terminated" state propagates outward through nested constructs, and
the missing-return check sees it.

**`if / else if / else`:** blocks `then`, `else` (if present), and `endif`. `else if` is an
`IfStmt` in `otherwise`, generated recursively inside the `else` block.

**`while`:** blocks `while.cond`, `while.body`, `while.end`. If the condition folds to the
constant `true`, an unconditional `br` is emitted, so `while.end` has predecessors only through
`break`. A `while true` without `break` therefore leaves the function "terminated", and a function
whose last statement is such a loop does not need a trailing `return`.

**`for i in a..b`:** `a` and `b` are evaluated **once**, before the loop, and coerced to `int`.
Blocks: `for.cond` (`icmp slt i, end`), `for.body`, `for.step` (`i + 1`), `for.end`. `continue`
jumps to `for.step`, not `for.cond`, so the increment is never skipped. The loop variable lives in
its own scope around the loop.

```llvm
  store i64 0, ptr %i, align 8
  br label %for.cond
for.cond:
  %3 = load i64, ptr %i, align 8
  %4 = icmp slt i64 %3, 2
  br i1 %4, label %for.body, label %for.end
for.body:
  ...
  br label %for.step
for.step:
  %6 = load i64, ptr %i, align 8
  %7 = add i64 %6, 1
  store i64 %7, ptr %i, align 8
  br label %for.cond
for.end:
```

**`break` / `continue`:** `loops` is a stack of `{continueTo, breakTo}`. `while` pushes
`{while.cond, while.end}` and `for` pushes `{for.step, for.end}`. Outside a loop it is an error.

### 8.6 Short-circuit `&&` / `||`: the only hand-built φ

```c
fn check(int a, int b) -> bool {
    return a > 0 && b > 0
}
```

```llvm
  %a3 = load i64, ptr %a1, align 8
  %0 = icmp sgt i64 %a3, 0
  br i1 %0, label %and.rhs, label %and.end
and.rhs:
  %b4 = load i64, ptr %b2, align 8
  %1 = icmp sgt i64 %b4, 0
  br label %and.end
and.end:
  %2 = phi i1 [ false, %entry ], [ %1, %and.rhs ]
```

`logic()` records the block where the left side **ended** (`from`) and the block where the right
side **ended** (`rhsEnd`). These are not necessarily the blocks where evaluation started, because a
nested `&&` creates its own blocks. The φ uses `!isAnd` as the short-circuit constant (`false` for
`&&`, `true` for `||`).

### 8.7 `print`

`print(a, b, …)` compiles to **one `printf` call**. The format string is built at compile time from
the argument types, joined with spaces and ending in `\n`:

| Finch type | Format | Argument conversion |
|---|---|---|
| signed ints | `%lld` | `sext` → `i64` |
| unsigned ints | `%llu` | `zext` → `i64` |
| `f32`, `f64` | `%g` | `fpext` → `double` |
| `char` | `%c` | `zext` → `i32` |
| `str` | `%s` | as is |
| `bool` | `%s` | `select i1, "true", "false"` |
| `ptr…` | `%p` | as is (glibc prints `(nil)` for null) |
| `null` | literal text `null` | none |

Since the format is generated, user strings are always passed as `%s` arguments, never as the
format, so a `%` in user text is safe. O2 often rewrites `printf("%s\n", s)` into `puts(s)`.

### 8.8 Strings

A string literal is a private `unnamed_addr` constant global (`CreateGlobalString`), and its value is
the `ptr` to it. Identical literals may be merged by the optimizer. A `str` variable declared
without an initializer points to a shared `""` constant. Strings are **immutable and static** in
1.0. There is no concatenation or allocation (section 18).

### 8.9 `addr()` and pointers

`addrOf()` requires a `Var` or `Member` argument and returns `place(arg).addr` with type
`ptr[T]`. No load happens: `addr(x)` *is* x's alloca. The `alloca` is therefore not promotable
by mem2reg, which is the expected cost of taking an address. `addr(p.value)` yields the pointer
already stored in `p`, after a null check. `addr` of a `for` variable is rejected (it is read-only).

### 8.10 Calls: `callFinch()` vs `callC()`

Both check the argument count and `coerce()` each argument to the parameter type. The differences:

| | `callFinch()` | `callC()` |
|---|---|---|
| Callee | `finch.<name>`, created in `declare()` | `getOrInsertFunction(name, …)`: declared lazily on first use, reused afterwards |
| Variadic | never | supported; extra args get C default promotions |
| Parameter attributes | none needed | `signext`/`zeroext` on params and return values narrower than 32 bits, on both the declaration and the call site |
| Unsupported signatures | impossible | rejected with the reason saved by cimport (`passes a C struct by value`, …) |
| Calling `main` | rejected | n/a |

Default argument promotions for the variadic part: `f32` → `double`; `bool`, `char` and integers
narrower than 32 bits → `i32` (sign- or zero-extended). Pointers, `str` and 32/64-bit integers
pass unchanged. This mirrors C's rules, which is what `printf`'s `va_arg` expects.

Why `signext`/`zeroext`: on x86-64 System V, clang passes `char`/`short`/`_Bool` already extended to
32 bits and marks them so. Some callees rely on it. Without the attribute LLVM would leave the upper
bits undefined, which would be an ABI mismatch with C code compiled by clang.

---

## 9. Runtime checks

Finch removes the two most common sources of C undefined behavior that can be checked cheaply.
All checks share one helper function per module:

```llvm
; Function Attrs: cold noinline noreturn
define internal void @finch.panic(ptr %0, i64 %1) #0 {
entry:
  %2 = call i32 (i32, ptr, ...) @dprintf(i32 2, ptr @panic.fmt, ptr @panic.file, i64 %1, ptr %0)
  call void @exit(i32 1)
  unreachable
}
```

`panicFn()` creates it on first use. `panicIf(cond, msg, pos)` emits
`br cond, %panic, %ok`, a call to `finch.panic(msg, line)` and `unreachable` in `%panic`, and
continues in `%ok`. It writes to fd 2 with `dprintf`, so it does not depend on libc's `stderr`
symbol. The function is `cold` and `noinline`, so branch layout favours the non-failing path and the
check costs one compare and a predicted branch.

### Integer division and remainder: `checkDivisor()`

```llvm
define internal i64 @finch.div(i64 %a, i64 %b) {
  ...
  %0 = icmp eq i64 %b4, 0
  br i1 %0, label %panic, label %ok
panic:
  call void @finch.panic(ptr @panic.msg, i64 2)
  unreachable
ok:
  %1 = icmp eq i64 %b4, -1
  %2 = icmp eq i64 %a3, -9223372036854775808
  %3 = and i1 %2, %1
  br i1 %3, label %panic5, label %ok6
panic5:
  call void @finch.panic(ptr @panic.msg.1, i64 2)
  unreachable
ok6:
  %4 = sdiv i64 %a3, %b4
```

- A divisor that is a **constant zero** is a **compile-time** error.
- A constant non-zero divisor (other than −1 for signed types) emits no check at all.
- For signed division, `MIN / -1` overflows (it traps with SIGFPE on x86). That is checked too.
- When everything is known, O2 removes the checks entirely: `div(10, 2)` above becomes `5` in `main`.

### Null `.value`: in `place()`

Every `p.value` whose pointer is not trivially non-null (not directly an `alloca` or a global)
gets `panicIf(icmp eq p, null, "used .value on a null pointer")`. Without this, LLVM treats a load
from null as UB and may **delete it**. Before this check existed, `print(p.value)` on a null `p`
printed a garbage value at O2 instead of crashing.

---

## 10. C interop via libclang

`importHeaders(imports, dir)` (`src/cimport.cpp`) runs **before** codegen and produces:

```cpp
struct CImports {
    std::unordered_map<std::string, CFunc>   fns;      // name, header, ret, params, variadic, unsupported reason
    std::unordered_map<std::string, CConst>  consts;   // enum values + numeric #defines
    std::unordered_map<std::string, CGlobal> globals;  // extern variables
};
```

### Parsing

For **each** `import "h"`, a separate translation unit is parsed from an in-memory file
`finch_import.c` containing `#include "h"`. Separate units give precise error attribution: a missing
header points at its own `import` line. Flags:

```
-x c -std=gnu11 -I<dir of the .fn file> -isystem<each system include dir>
CXTranslationUnit_DetailedPreprocessingRecord   (to see macros)
CXTranslationUnit_SkipFunctionBodies            (speed: inline bodies are irrelevant)
```

- **System include directories** come from running `$CC -E -v -x c /dev/null` and parsing the block
  between `#include <...> search starts here:` and `End of search list.`. This is what makes
  `stdio.h` resolve the same way it does for the system compiler. That matters on NixOS, where the
  cc wrapper injects store paths that an unwrapped libclang knows nothing about.
- **`gnu11`** rather than `c11`: glibc hides `M_PI` and other non-ISO names under strict modes.
- **Diagnostics** of severity ≥ error abort the import. `file not found` becomes
  `can't find the C header 'h'`.

### The visitor

`visit()` walks top-level cursors. The first declaration of a name wins.

| Cursor | Action |
|---|---|
| `FunctionDecl` | Map the canonical **function type** (`clang_getArgType`, so array parameters are already decayed to pointers). `static` functions are recorded as unsupported (`static inline` has no symbol to link). `FunctionNoProto` (K&R `f()`) is unsupported. Variadic-ness comes from `clang_isFunctionTypeVariadic`. |
| `VarDecl` with `extern` storage | → `CGlobal` if the type maps |
| `EnumDecl` | recurse into children |
| `EnumConstantDecl` | → `CConst` (`i64`, literal) |
| `MacroDefinition` | skip built-ins and function-like macros; `macroValue()` tries to read a number |

### Type mapping: `mapType()`

Works on the **canonical** type, so typedefs (`size_t`, `uint32_t`, `GLuint`, `FILE`) are resolved.
Integer types map **by size** (`clang_Type_getSizeOf`), not by name, so `long` becomes `i64` on LP64.

| C (canonical) | Finch |
|---|---|
| `void` | nothing |
| `_Bool` | `bool` |
| `char` (either signedness) | `char` |
| `signed char`, `short`, `int`, `long`, `long long`, `enum` | `i8`/`i16`/`i32`/`i64` by size |
| unsigned variants | `u8`…`u64` by size |
| `float` / `double` | `f32` / `f64` |
| `char *` (any qualifiers) | `str` |
| `T *` where T maps to a non-void type | `ptr[T]` |
| `void *`, pointer to struct/union/function/unmappable | `ptr` (opaque) |
| struct/union by value | **unsupported** ("passes a C struct by value") |
| `long double`, `__int128`, vectors, … | **unsupported** |

Unsupported functions stay in the table, so calling one produces a precise error instead of
"no such function".

### Macros: `macroValue()`

It tokenizes the macro's extent, drops the name token and comments, strips **balanced outer
parentheses**, accepts one leading `-`, and then requires **exactly one** numeric literal. Integers
are parsed with `strtoull(…, 0)`, i.e. **C rules**: `0x` hex and leading-zero **octal**, unlike
Finch's own literals. Floats are anything containing `.`, `e` or `E` that is not hex. Trailing
`u U l L f F` suffixes are accepted. That covers `M_PI`, `EOF` (`(-1)`), `RAND_MAX`, `INT_MAX`,
`GL_*` and `GLFW_*` constants. Expressions (`(1 << 4)`), references to other macros, and strings
are skipped.

### Calling

Imported functions are declared in the module **only when called**, through
`getOrInsertFunction`. This reuses an existing declaration if `print` already created `printf`.
Since the signatures agree (`i32 (ptr, ...)`), there is no conflict.

### Link errors

When the link step fails, `linkFailed()` in `main.cpp` scans the linker's output for
`undefined reference to \`sym'` (GNU ld) and `undefined symbol: sym` (lld). It groups the symbols by
`CFunc::header` and prints which header they came from, with a guessed `link` line (`guessLib()`:
basename, minus extension and trailing digits, lowercase: `GLFW/glfw3.h` → `glfw`).
`cannot find -lfoo` becomes `the library 'foo' wasn't found`.

---

## 11. Optimization, emission, linking

### Target

`hostMachine()` creates a `TargetMachine` for `sys::getDefaultTargetTriple()` with CPU
`"generic"`, no extra features, and `Reloc::PIC_`, which is required for the default PIE
executables on modern distributions. Its **data layout is installed on the module before any IR
is built**, in `Codegen`'s constructor. That matters: `IRBuilder` takes the alignment of loads and
stores from the data layout, and without it `i64` accesses would be emitted with `align 4`.

`generic` means the binary runs on any x86-64. Adding `-march=native` behavior would be a CPU
string and feature change here.

### Optimization

`optimize()` builds the new pass manager pipeline:

```cpp
PassBuilder pb(tm);
pb.register{Module,CGSCC,Function,Loop}Analyses(...);
pb.crossRegisterProxies(lam, fam, cgam, mam);
pb.buildPerModuleDefaultPipeline(OptimizationLevel::O2).run(mod, mam);
```

That is clang's `-O2` middle end: SROA/mem2reg, instcombine, inlining, GVN, LICM, loop
unrolling, vectorization and so on. `-O0` skips this call, which is useful to read the IR that
`Codegen` produced (`finch ir file.fn -O0`). Back-end codegen still runs at the TargetMachine's
default level.

### Emission

`emitObject()` uses the **legacy** pass manager, because LLVM's machine-code pipeline is still only
reachable through it: `tm->addPassesToEmitFile(pm, out, nullptr, CodeGenFileType::ObjectFile)`.

### Linking

```
$CC file.o -o file -lm  [pkg-config --libs NAME | -lNAME]...
```

The system C compiler driver (`cc` by default) is used as the linker. It knows the platform's crt
files, the dynamic loader path, libc and default library paths, which would be a lot to replicate.
`-lm` is always added. For each `link "NAME"` or `-l NAME`, `pkg-config --libs NAME` is tried
first, so `-L` paths and dependent libraries come along. On failure it falls back to `-lNAME`.
On Nix, the cc wrapper turns those `-L` paths into `RPATH` entries, so the result runs without
`LD_LIBRARY_PATH`. The object file is deleted afterwards whether linking succeeded or not.

### `finch run`

It compiles to a temporary file (`sys::fs::createTemporaryFile`), runs it with `system()`, deletes
it, and forwards the exit status. If the child died from a signal, it prints
`the program crashed: <strsignal>` and returns `128 + signo`, like a shell.

---

## 12. Diagnostics

`fail(line, col, msg)` is `[[noreturn]]`: it prints

```
file:line:col: error: msg
 line | source text
      |      ^
```

and exits with status 1. The excerpt is taken from `g_source`, and tabs in the prefix are kept so
the caret lines up. Conventions used across the code base:

- **Positions:** binary operators report the **operator's** position; calls report the callee's;
  type errors in arguments report the **argument's** position.
- **Messages** say what is wrong in plain words, and **how to fix it** when there is an obvious fix
  (`use int(...) to convert`, `did you mean ':='?`, `add import "math.h"`).
- `headerHint()` maps about two dozen common libc/libm names to their header, for calls to unknown functions.
- An internal inconsistency, if `verifyModule` ever fails, prints `internal compiler error (please report)`
  with the verifier's output and exits with status 2.

The compiler has no warnings. Everything that would be a warning is either fine or an error.

---

## 13. Build system

`CMakeLists.txt`:

- `find_package(LLVM CONFIG)`. **If the `LLVM` target exists (a shared `libLLVM.so`), Finch links that
  one library.** This is essential, not cosmetic: `libclang.so` itself links `libLLVM.so`. Linking
  Finch against the static component libraries (`libLLVMCore.a`, …) puts a **second copy of LLVM** in
  the process. Both copies register the same global objects, and the program crashed on exit in a
  global destructor (`std::vector<TensorSpec>::~vector`, a double free). It also made the binary 82 MB
  instead of 11 MB.
- libclang is found with `find_path(clang-c/Index.h)` and `find_library(clang)`, hinted by LLVM's dirs.
- `FINCH_VERSION` comes from `project(VERSION …)` and is printed by `finch version`, along with `LLVM_VERSION_STRING`.
- C++17, `-Wall -Wextra`, and the build is warning-free.

---

## 14. Testing

`tests/run.sh` (`FINCH=… ` overrides the compiler path):

- `tests/run/*.fn`: run with `finch run`. Combined stdout+stderr must equal `*.out` exactly.
- `tests/fail/*.fn`: must exit non-zero, and the output must contain the text after `// expect: ` on line 1.
  This covers compile errors, runtime panics (with line numbers), header and link failures.
  `tests/fail/lib_hint.h` declares a function that exists in no library, to test the link-hint path.

Current state: **34 passed, 0 failed.** Every new error message should get a `fail/` test, and every
feature a `run/` test.

---

## 15. Semantics vs. C: defined and undefined behavior

| Situation | C | Finch 1.0 |
|---|---|---|
| Signed integer overflow | UB | **Defined wraparound** (no `nsw` flags) |
| Unsigned overflow | wraps | wraps |
| Integer division by zero | UB | **Compile error** if constant, otherwise **runtime error** |
| `INT_MIN / -1` | UB | **Runtime error** |
| Load or store through null | UB | **Runtime error** for `.value` |
| `x & 1 == 0` | parses as `x & (1 == 0)` | parses as `(x & 1) == 0` |
| Implicit narrowing (`int` → `char`) | silent | **Compile error** |
| Signed/unsigned mixing | silent conversion | **Compile error** unless one side widens |
| Using a variable before it is set | UB | Impossible: every declaration initializes (zero by default) |
| Missing `return` in non-void function | UB if the value is used | **Compile error** |
| Shift by ≥ bit width | UB | **Still poison** (not checked in 1.0) |
| `float` → int out of range | UB | **Still poison** (not checked in 1.0) |
| Dangling pointer to a dead local | UB | **Still UB** (no lifetime checks yet) |
| Order of evaluation | mostly unspecified | Left to right |

---

## 16. Function reference: who does what

Groups of functions that sound alike, and how they differ.

### Lexing and parsing

| Function | Does | Differs from |
|---|---|---|
| `lex()` | text → tokens, sets `newlineBefore` | knows nothing about grammar or types |
| `tokName()` | token kind → human name for errors | |
| `Parser::cur()` / `peekTok()` / `next()` | current token / lookahead without consuming / consume | |
| `accept(k)` vs `expect(k)` | consume if it matches and return a bool / consume or **fail** with "expected …, found …" | |
| `unexpected(ctx)` | fail with "unexpected X (ctx)" at the current token | `expect` names the token it wanted; `unexpected` explains the context |
| `sameLine()` | may the current token continue the expression? | `endOfStatement()` is the statement-level counterpart (requires a newline or `}`) |
| `atDeclaration()` | `int x` / `ptr[...] p` lookahead | `type()` actually consumes a type |
| `typeFromName()` vs `isTypeName()` | name → `Type` / name → bool | |
| `statement()` vs `block()` | one statement / `{ statement* }` with terminator checks and `parenDepth` reset | |
| `callArgs()` | shared by calls and conversions: `( expr, … )` | |
| `primary()` / `postfix()` / `unary()` / `multiplicative()` / `additive()` / `compare()` / `andExpr()` / `orExpr()` | one function per precedence level | |

### Codegen: types and values

| Function | Does | Differs from |
|---|---|---|
| `ty(FType)` | Finch type → LLVM type | |
| `zero(FType)` | default value for an uninitialized declaration | |
| `widens(from, to)` | can every value of `from` be represented in `to`? (pure type rule) | `fits()` asks the same about one constant |
| `fits(c, from, to)` | does this literal's value fit in `to`? | |
| `coerce(v, want)` | **implicit** conversion into a known slot, or error | `convert()` is **explicit** `T(x)` and allows lossy casts; `unify()` converts **two** operands toward each other |
| `unify(l, r)` | bring binary operands to a common type | used by `arithOp()` and `compare()` |
| `intCast()` / `intToFloat()` | emit the signedness-correct cast | low-level helpers used by the three above |

### Codegen: expressions

| Function | Does | Differs from |
|---|---|---|
| `expr(e)` | r-value: emits code, returns `{value, type, literal}` | `place()` returns an **address** instead of loading |
| `place(e)` | l-value address of a variable, C global or `p.value` | used by `expr()` (then loads), `assign()` (then stores), `addrOf()` (returns as is) |
| `binary()` | dispatch by `BinOp` | |
| `arith()` vs `arithOp()` | `arith()` wraps `arithOp()` and computes the result's `literal` flag | `arithOp()` does `unify` and emits the instruction |
| `compare()` | all comparisons, pointer/str/float/int variants | always returns `bool` and is never a literal |
| `logic()` | `&&`/`\|\|` with branches and a φ | the only place that builds a φ by hand |
| `call()` | resolves a call name to one of the five kinds | |
| `callFinch()` vs `callC()` | see section 8.10 | |
| `print()` | builds a printf format from the types | |
| `addrOf()` | `addr(x)` → `place(x).addr` | |
| `convert()` | `T(x)` | |
| `libc()` | `getOrInsertFunction`: declare-or-reuse an external function | `declare()` creates **Finch** functions with `Function::Create` |
| `cGlobal()` | declare-or-reuse an `extern` global | |

### Codegen: statements and control

| Function | Does | Differs from |
|---|---|---|
| `declare()` vs `define()` | signature pass / body pass | |
| `block()` vs `blockBody()` | `block()` pushes and pops a scope around `blockBody()`; `blockBody()` emits statements and rejects unreachable ones | `define()` calls `blockBody()` directly, because parameters and the outer body share one scope |
| `stmt()` | dispatch by `StmtKind`; also handles `break`/`continue` | |
| `varDecl()` / `assign()` | declaration with optional inference / store to an existing place | |
| `addVar()` | create slot, store initial value, register name (with no-shadowing check) | `slot()` only creates the `alloca` |
| `lookup()` | search all scopes, innermost first | |
| `condition(e)` | `coerce(expr(e), bool)` with the message "a condition must be bool" | |
| `ifStmt()` / `whileStmt()` / `forStmt()` / `returnStmt()` | section 8.5 | |
| `terminated()` | does the current block already end in a terminator? | |
| `continueAt(bb, deadEnd)` | move to the merge block, or delete it if unreachable | |
| `newBlock()` | create a basic block in the current function | |

### Runtime checks

| Function | Does |
|---|---|
| `panicFn()` | get or create `finch.panic` |
| `panicIf(cond, msg, pos)` | conditional branch to a panic block |
| `checkDivisor(l, r, pos)` | zero and `MIN/-1` checks, or a compile-time error |

### Driver (`main.cpp`)

| Function | Does |
|---|---|
| `hostMachine()` | create the `TargetMachine` |
| `optimize()` | O2 pipeline |
| `emitObject()` | write the `.o` |
| `capture()` | run a shell command, collect its output and status |
| `libFlags()` | pkg-config or `-l` for one library |
| `link()` | run the linker, delete the `.o`, explain failures |
| `linkFailed()` / `guessLib()` | translate linker errors into Finch advice |

---

## 17. Extending the compiler

### A new built-in function (example: `len(str) -> int`)

1. Add the name to `Codegen::isBuiltin()`, so it cannot be redefined.
2. In `Codegen::call()`, dispatch `if (c.callee == "len") return len(c);`.
3. Implement it: check the argument count, `coerce(expr(arg), Str, …)`, emit `strlen` via `libc()`,
   return `{call, I64}`. `strlen` returns `u64`; convert with `intCast` if `int` is wanted.
4. Add `tests/run/len.fn` and its `.out`, plus a `tests/fail/` case for a wrong argument.

### A new binary operator

1. Lexer: add a `Tok`, its spelling in the operator `switch`, and `tokName()`.
2. AST: add a `BinOp`.
3. Parser: put it in the right precedence function.
4. Codegen: `opName()`, then emit it in `arithOp()` or `compare()`, and explain bad operands in `badOperands()`.

### A new statement

Add a `StmtKind` and node in `ast.h`, recognise it in `Parser::statement()`, and handle it in
`Codegen::stmt()`. If it can transfer control, make sure the result respects `terminated()` and
uses `continueAt()` for its merge block.

### Coding conventions

Match the existing style: early `fail()` with a fixing hint, comments only where the *why* is not
obvious, and one test per behavior.

---

## 18. Known limitations and roadmap

- **No aggregates yet:** `struct`, arrays/slices with `.len`, and C structs by value. These are the next
  milestones, in that order: arrays build on structs (pointer + length), and the memory model builds on both.
- **Memory model** (agreed design): values that own heap memory are freed automatically at the end of
  the block that owns them, `defer` runs code at block exit, and `free` remains for manual control.
  Implementation sketch: a per-block cleanup list, emitted at every exit edge (normal end, `return`,
  `break`, `continue`). `blockBody()` and the `loops` stack already know every exit edge.
- **Strings:** concatenation, `len`, `str(x)`. These depend on the memory model.
- **Input:** `input()`.
- **Modules:** `import name` for Finch code. One `.fn` = one program today.
- **Unchecked UB:** shift amounts, float→int range, dangling pointers. Shift and float→int could be
  checked like division at small cost.
- **Debug info:** no DWARF yet. `DIBuilder` would hook into `define()` and each statement's `pos`.
- **Targets:** only the host triple, with Linux/x86-64 tested. The code is target-neutral except for
  the `cc -E -v` include discovery and `dprintf`.
- **Bootstrapping:** rewriting the compiler in Finch is the long-term goal. Calling the LLVM-C API
  already works today: LLVM-C uses opaque handles (`LLVMModuleRef` and so on), which map to Finch's
  `ptr`. `examples/llvm.fn` builds and prints a module from Finch (`import "llvm-c/Core.h"`,
  `link "LLVM"`). What is still missing for a self-hosted compiler is structs, arrays, strings and file I/O.
