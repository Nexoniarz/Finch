# Finch Cheat Sheet

Everything in Finch on one page. For explanations, see the [guides](../../README.md#documentation).

> 🇵🇱 Polska wersja: [sciaga.md](../pl/sciaga.md)

---

## The `finch` command

| Command | Does |
|---|---|
| `finch run file.fch [args…]` | compile and run (args go to the program) |
| `finch build file.fch [-o name]` | make a program (`name.exe` on Windows) |
| `finch ir file.fch` | print the LLVM IR |
| `finch lsp` | the language server (editors use it) |
| `finch version` | version |
| `-l lib` | link a C library (like `link "lib"`) |
| `--target windows` / `linux` / `arm64` / `macos` / triple | build for another system |
| `-g` | debug info (gdb, lldb, Visual Studio) |
| `-O0` | no optimizations |

Environment: `FINCH_PATH` (folders with modules), `FINCH_CC` / `CC` (the C compiler that links).

---

## A file

```c
import "stdio.h"        // a C header
import shapes           // a Finch module: shapes.fch
link "m"                // a C library, or "mine.c" / ".o" / ".a"

struct Point {
    int x
    int y = 0           // default value
}

fn main() {
    print("Hello")
}
```

`main` can be `fn main()`, `fn main() -> int` (exit code) or `fn main([]str args)`.
One statement per line, no semicolons, `{` on the same line, no brackets around conditions.

---

## Variables

| Code | Meaning |
|---|---|
| `x := 5` | new variable, type inferred |
| `int x = 5` | new variable, type written |
| `int x` | zero / `""` / `[]` / `null` / the struct's defaults |
| `x = 7` | assign |
| `x += 1` `-=` `*=` `/=` `%=` | update |

---

## Types

| Type | Example |
|---|---|
| `int` (= `i64`) | `42`, `-7`, `0xFF`, `1_000_000` |
| `float` (= `f64`) | `3.14` |
| `bool` | `true`, `false` |
| `char` | `'A'`, `'\n'` |
| `str` | `"text\t\"quoted\""` |
| `i8 i16 i32 i64` | signed, sized |
| `u8 u16 u32 u64` | unsigned, sized |
| `f32 f64` | floats |
| `[]T` | array: `[1, 2, 3]`, `[]str names` |
| `map[K]V` | map: `["a": 1]`, `map[str]int counts`, `[:]` |
| `ptr[T]`, `ptr` | pointer, untyped pointer |
| `Name` / `module.Name` | a struct |

Escapes: `\n \t \r \0 \\ \" \'`.

---

## Operators

| Priority | Operators |
|---|---|
| 5 | `*` `/` `%` `<<` `>>` `&` |
| 4 | `+` `-` `\|` `^` (`+` also joins text) |
| 3 | `==` `!=` `<` `<=` `>` `>=` |
| 2 | `&&` |
| 1 | `\|\|` |
| unary | `-x` `!x` `~x` |

`x & 1 == 0` means `(x & 1) == 0`.

---

## Control flow

```c
if a > b { … } else if a == b { … } else { … }
while x > 0 { … }
for i in 0..10 { … }          // 0 … 9
for item in list { … }        // every element (or char of a str)
for i, item in list { … }     // with its index
for key, value in m { … }     // every map entry, in insertion order
break     continue     return value
defer cleanup()               // runs when the block ends
```

---

## Functions

```c
fn add(int a, int b) -> int {
    return a + b
}
fn greet(str name) {          // no result
    print("Hi", name)
}
```

Parameters behave like copies. Order in the file doesn't matter. A comma after the last argument is allowed.

---

## Methods

```c
fn Point.move(int dx, int dy) {   // self is the Point it's called on (not a copy)
    self.x += dx
    self.y += dy
}
fn Point.dist() -> float { … }

p.move(1, 2)       list[0].move(1, 1)       ptr_to_point.move(0, 1)
```

Also on structs from C: `fn Vector2.length() -> f32 { … }`.

---

## Errors as values

```c
fn parse(str s) -> int! {         // ! = can fail
    if s.len == 0 {
        return error("empty")
    }
    return int(s) or { return error("not a number: " + s) }
}
fn save(str p, str t) -> ! {      // can fail, no result
    try write_file(p, t)
}
```

| Code | On failure |
|---|---|
| `x := parse(s) or 0` | use 0 |
| `x := parse(s) or { print(err); return }` | run the block (`err`: the message) |
| `x := try parse(s)` | fail the current function too |
| `fn main() -> !` | a failure prints `error: …`, exit code 1 |

Fallible built-ins: `int(s)`, `float(s)`, `read_file`, `write_file`, `delete_file`.

---

## Structs

| Code | Meaning |
|---|---|
| `struct P { … }` | define (one field per line, `= default` optional) |
| `P(1, 2)` | create, all fields in order |
| `P(x: 1)` | create by name, the rest default/zero |
| `p.x`, `p.x = 5` | read, change a field |
| `b := a` | a full copy |

---

## Arrays `[]T`

| Code | Gives |
|---|---|
| `a.len` | number of elements |
| `a[i]`, `a[i] = x` | element (checked) |
| `a.push(x)` | add at the end |
| `a.pop()` | remove the last, give it back |
| `a.insert(i, x)` | put at position i |
| `a.remove(i)` | remove position i, give it back |
| `a.clear()` | empty it |
| `a.resize(n)` | length n (new elements are zero) |
| `a.find(x)` | index or -1 |
| `a.contains(x)` | bool |
| `a.sort()` | sort (numbers, chars, str) |
| `a.reverse()` | reverse |
| `a.slice(s, e)` | new array of s … e-1 |
| `a.join(sep)` | `[]str` → one str |
| `a.ptr` | address of the first element (for C) |

---

## Maps `map[K]V`

| Code | Gives |
|---|---|
| `m := ["a": 1, "b": 2]` | a map (keys: numbers, char, bool, str) |
| `m[k]` | value (missing key: the program stops) |
| `m[k] = v` | add / replace |
| `m[k] += 1`, `m[k].push(x)` | a missing key starts at the default (0, `[]`, …) |
| `m.len` | number of keys |
| `m.has(k)` | bool |
| `m.get(k, default)` | value or default |
| `m.remove(k)` | bool: was it there |
| `m.clear()` | empty it |
| `m.keys()`, `m.values()` | arrays, insertion order |

---

## Text `str`

| Code | Gives |
|---|---|
| `s.len` | bytes |
| `s[i]`, `s[i] = 'x'` | char |
| `a + b` | joined text |
| `s.sub(a, b)` | part a … b-1 |
| `s.find(t)` | position or -1 |
| `s.contains(t)` | bool |
| `s.starts_with(t)`, `s.ends_with(t)` | bool |
| `s.split(sep)` | `[]str` |
| `s.trim()` | no spaces at the ends |
| `s.upper()`, `s.lower()` | A–Z / a–z |
| `s.replace(a, b)` | every a → b |
| `s.repeat(n)` | n times |
| `s.bytes()` | `[]u8` |
| `s.ptr` | `ptr[char]` (for C) |
| `==` `!=` `<` `>` | compare |

---

## Conversions

| Code | Result |
|---|---|
| `int(3.9)` | `3` |
| `int("42")` | `42` (stops the program if it isn't a number) |
| `int('A')` | `65` |
| `float(7)`, `float("2.5")` | `7`, `2.5` |
| `u8(300)` | `44` (keeps the low bits) |
| `char(66)` | `'B'` |
| `bool(0)` | `false` |
| `str(42)`, `str(1.5)`, `str(true)`, `str('c')` | text |
| `str([1, 2])`, `str(m)`, `str(point)` | the same text `print` shows |
| `int("x") or 0`, `float(s) or 0.0` | a number, or the fallback |
| `str(bytes)` | `[]u8` → text |
| `str(p)` | C `char*` → text |
| `ptr(p)`, `ptr(16)` | untyped pointer, from a pointer or a number |
| `int(p)` | pointer → address |

Automatic only when nothing is lost: smaller → bigger number, any int → float, `f32` → `f64`.

---

## Built-in functions

| Function | Does |
|---|---|
| `print(a, b, …)` | print, spaces between, newline at the end |
| `input()`, `input("Question? ")` | read a line |
| `read_file(path)` | file → str |
| `write_file(path, text)` | str → file, `true` if it worked |
| `file_exists(path)` | bool |
| `delete_file(path)` | bool |
| `shell(command)` | run a command, its exit code |
| `exit(code)` | stop the program now |
| `error(message)` | `return error("…")` in a `-> T!` function |
| `addr(x)` | pointer to x; `addr(fn)` = C callback |
| `new(value)` | value on the heap → `ptr[T]` |
| `free(p)` | give it back |

---

## Pointers

| Finch | C |
|---|---|
| `ptr[int] p` | `int *p` |
| `addr(x)` | `&x` |
| `p.value` | `*p` |
| `p.field` | `p->field` |
| `p[i]` | `p[i]` |
| `null` | `NULL` |
| `ptr` | `void *` |

---

## Memory

- Arrays, text and structs free themselves at the end of their block. No `free` needed.
- `b := a` copies; a call's result moves; `return x` moves.
- `new(...)` + `free(...)` (often with `defer`) for linked lists, trees, sharing.
- C memory: C's own functions (`malloc` / `free`, `fclose`, …).

---

## Modules

```c
import shapes            // shapes.fch next to this file (or in FINCH_PATH)
shapes.area(b)           // its functions
shapes.Box(2, 3)         // its structs
shapes.Box other         // in declarations
```

---

## C

| Code | Meaning |
|---|---|
| `import "x.h"` | functions, structs, enums, `#define` numbers, globals |
| `link "name"` | libname.so / name.lib (pkg-config first) |
| `link "file.c"` | compile your own C file |
| `addr(myFn)` | pass a Finch function as a callback |

| C type | Finch |
|---|---|
| `int` / `long long` / `unsigned` | `i32` / `i64` / `u32` (by size) |
| `float` / `double` | `f32` / `f64` |
| `char *` (parameter, result) | `str` |
| `char *` (field, behind a pointer) | `ptr[char]` |
| `struct X` | `X` |
| `T *` | `ptr[T]` |
| `void *`, handles | `ptr` |
| `T x[N]` (in a struct) | `[N]T` |

---

## Runtime errors (the program stops with file and line)

| Message | Cause |
|---|---|
| `division by zero` | `/` or `%` by 0 |
| `index 5 is out of range (the length is 3)` | `a[5]` |
| `pop() on an empty array` | |
| `used .value on a null pointer` | `.value` / `.field` / `[i]` through `null` |
| `can't turn "x" into int` | `int("x")` |
| `can't read the file "…"` | `read_file` (without `or`) |
| `the key "x" is not in the map` | `m["x"]` |

---

## Editors

Kate: `editors/kate/install.sh`, then enable the **LSP Client** plugin; run with **Build & Run** → `finch run %f`.
Any LSP editor: `finch lsp`.

### VS Code (extension "Finch")

| Key / action | Does |
|---|---|
| ▶ / `Ctrl+F5` | run the file |
| `Finch: Build This File` | build; errors go to Problems |
| `F12` | go to definition |
| hover | type / signature / docs |
| `Ctrl+Space` | completion (after `.`: fields, methods, module members) |
| `Ctrl+Shift+O` | functions and structs in the file |
| `main`, `fn`, `fnr`, `fnf`, `method`, `struct`, `for`, `foreach`, `forkv`, `map`, `orb`, `if`, `ife`, `while`, `input` + Tab | snippets |
| setting `finch.path` | where finch / finch.exe is |

---

## Keywords

`fn return if else while for in break continue true false null import link struct defer or try`
(and `self` inside methods)
