# Finch for Technicians

**For people who know their way around a computer, but little or no programming.**
This guide covers installing Finch, using the `finch` tool, the whole language, memory,
modules, C libraries, debugging and fixing common problems. It does not cover how the
compiler works inside. For that, see the [guide for engineers](for-engineers.md).

> 🇵🇱 Polska wersja: [dla-technikow.md](../pl/dla-technikow.md)

---

## Contents

1. [What Finch is](#1-what-finch-is)
2. [Installing](#2-installing)
3. [The finch command](#3-the-finch-command)
4. [How a Finch file looks](#4-how-a-finch-file-looks)
5. [Variables](#5-variables)
6. [Types](#6-types)
7. [Operators](#7-operators)
8. [Control flow](#8-control-flow)
9. [Functions](#9-functions)
10. [Arrays and maps](#10-arrays-and-maps)
11. [Text (str)](#11-text-str)
12. [Structs and methods](#12-structs-and-methods)
13. [Memory: who frees what](#13-memory-who-frees-what)
14. [Pointers](#14-pointers)
15. [Built-in functions](#15-built-in-functions)
16. [Modules](#16-modules)
17. [Using C libraries](#17-using-c-libraries)
18. [Errors](#18-errors)
19. [Editors: VS Code, Kate and others](#19-editors-vs-code-kate-and-others)
20. [Debugging](#20-debugging)
21. [Troubleshooting](#21-troubleshooting)
22. [Project layout and tests](#22-project-layout-and-tests)
23. [Current limits](#23-current-limits)

---

## 1. What Finch is

Finch is a **compiled** programming language. The `finch` tool translates your `.fch`
file into a real program (a native executable, like the ones made from C), using the
**LLVM** compiler toolkit, the same backend that powers clang, Rust and Swift.

What that gives you:

- **Speed close to C.** No interpreter, virtual machine or garbage collector. Array bounds
  checks and other safety checks cost almost nothing after optimization.
- **Small, standalone programs** that you can copy and run.
- **Memory handled for you.** Lists and text are freed automatically at the end of the block
  that owns them, with no garbage collector involved.
- **Direct access to C libraries.** `import "stdio.h"` and call anything in it, even functions
  that take structs.

The design rules: one way to do each thing, C-like syntax without C's traps, and error
messages that tell you how to fix the problem.

---

## 2. Installing

Finch runs on **Linux** (x86-64 and ARM64), **macOS** (Apple Silicon and Intel) and **Windows** (x86-64),
and is built with **LLVM 21**. Each [release](https://github.com/Nexoniarz/Finch/releases) has a ready
`finch` for Windows, Linux x86-64, Linux ARM64 and macOS ARM64.

### Windows

1. Download **`finch-windows-x64.zip`** from the [releases](https://github.com/Nexoniarz/Finch/releases)
   and unpack it, for example to `C:\finch`. It contains `finch.exe` and `libclang.dll`.
2. Install **LLVM 21** (`LLVM-21.x.x-win64.exe` from the
   [LLVM releases](https://github.com/llvm/llvm-project/releases)) and tick *Add LLVM to the system PATH*.
   Finch uses its `clang` to link programs.
3. Install **Visual Studio Build Tools 2022** with *Desktop development with C++*: it provides the Windows
   libraries every program is linked with.
4. Add `C:\finch` to your PATH, open a new terminal, and run `finch version`.

Programs are normal `.exe` files. To build from source on Windows instead, follow `.github/workflows/ci.yml`
(Visual Studio 2022 + the `clang+llvm-21.x-x86_64-pc-windows-msvc` package from LLVM's releases).

### macOS

```sh
brew install llvm@21                # Finch's LLVM; programs are linked with Apple's clang
```

Then either unpack **`finch-macos-arm64.tar.gz`** from the releases (it uses Homebrew's `llvm@21`),
or build it yourself (Xcode command line tools: `xcode-select --install`):

```sh
git clone https://github.com/Nexoniarz/Finch.git && cd Finch
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DLLVM_DIR="$(brew --prefix llvm@21)/lib/cmake/llvm"
ninja -C build
./build/finch version
```

Apple frameworks link with `link "Cocoa.framework"` (`-framework Cocoa`), and their headers import as usual:
`import "OpenGL/gl.h"`.

### Linux

The ready `finch-linux-x64.tar.gz` / `finch-linux-arm64.tar.gz` need LLVM 21's libraries
(Debian/Ubuntu: `libllvm21 libclang1-21` from [apt.llvm.org](https://apt.llvm.org)) and a C compiler (`cc`).
Building it yourself works the same on x86-64 and ARM64 (Raspberry Pi 4/5, Graviton, …).
Other LLVM versions will likely fail to compile, because LLVM's C++ API changes between releases.

### Option A: Nix (recommended, nothing to install by hand)

```sh
git clone https://github.com/Nexoniarz/Finch.git
cd Finch
nix-shell                        # downloads LLVM, libclang, clang, cmake, ninja, pkg-config
cmake -S . -B build -G Ninja
ninja -C build
./build/finch version            # finch 2.4.0 (LLVM 21.x)
```

### Option B: your distribution's packages

You need: a C/C++ compiler, CMake ≥ 3.20, Ninja (or Make), LLVM 21 development files,
libclang 21 development files, and optionally `pkg-config`.

| Distribution | Packages (names can differ slightly by version) |
|---|---|
| Debian / Ubuntu | `build-essential cmake ninja-build pkg-config llvm-21-dev libclang-21-dev` |
| Fedora | `gcc-c++ cmake ninja-build pkgconf llvm-devel clang-devel` |
| Arch | `base-devel cmake ninja pkgconf llvm clang` |

These have not been tested; if CMake cannot find LLVM, point it there:

```sh
cmake -S . -B build -G Ninja -DLLVM_DIR=/usr/lib/llvm-21/lib/cmake/llvm
ninja -C build
```

### Building for another system

`--target` builds a program for another system. Finch needs that system's C compiler to link:

| `--target` | Builds | C compiler used | `finch run` uses |
|---|---|---|---|
| `windows` | a Windows `.exe` | `x86_64-w64-mingw32-gcc` (MinGW-w64) | Wine |
| `arm64` | an ARM64 Linux program | `aarch64-linux-gnu-gcc` | `qemu-aarch64` |
| `linux` | an x86-64 Linux program | `clang --target=…` | |
| `macos` | a macOS program (on a Mac: for the other processor) | `clang --target=…` | |
| any LLVM triple | e.g. `aarch64-unknown-linux-gnu` | `clang --target=<triple>` | |

```sh
finch build game.fch --target windows      # game.exe
finch run game.fch --target arm64          # builds for ARM64 and runs it with QEMU
FINCH_CC=aarch64-unknown-linux-gnu-gcc finch build tool.fch --target arm64   # another compiler name
```

On Nix the compilers are `pkgsCross.mingwW64.buildPackages.gcc` and
`pkgsCross.aarch64-multiplatform.buildPackages.gcc`.

### Putting `finch` on your PATH (optional)

```sh
sudo cp build/finch /usr/local/bin/
finch version
```

At run time Finch needs a C compiler (`cc`). It is used to link programs, and once to build
Finch's small runtime library, which is kept in `~/.cache/finch`. Set `CC` to use a different one.

---

## 3. The finch command

```
finch run   <file.fch> [args...]    compile and run right away
finch build <file.fch> [-o name]    compile into a program (default name: the file's name)
finch ir    <file.fch>              print the LLVM IR (for the curious)
finch lsp                          the language server, for editors
finch version                      show the version

options:
  -l <lib>          link a C library, same as  link "lib"  in the file
  --target <name>   build for another system: windows, linux, arm64, macos, or an LLVM triple
  -g                add debug info (for gdb / lldb / Visual Studio)
  -O0               skip optimizations
```

Examples:

```sh
finch run hello.fch                 # prints the output
finch run tool.fch input.txt -v     # arguments after the file go to the program
finch build game.fch -o mygame      # creates ./mygame
finch build game.fch -g -O0         # a build for the debugger
```

`finch run` passes on your program's exit code. If the program crashes, it says how
(for example `the program crashed: Segmentation fault`).

---

## 4. How a Finch file looks

```c
// comments start with two slashes
/* or span
   several lines */

import "math.h"          // C headers (optional), at the top
import shapes            // Finch modules (optional): shapes.fch next to this file
link "m"                 // C libraries or C files to link (optional)

struct Point {           // your own types
    int x
    int y
}

fn main() {              // the program starts here
    print("Hello")
}
```

Rules worth knowing:

- **One statement per line.** There are no semicolons. To split a long expression, break the line
  **after** an operator (`a +⏎ b`) or anywhere inside brackets `( )` or `[ ]`.
- **Curly braces are always required** after `if`, `else`, `for`, `while`, and the opening `{`
  goes on the same line.
- **No brackets around conditions:** `if x > 5 {`, not `if (x > 5) {`.
- Functions and structs can be in any order in the file.
- `main` is `fn main()`, `fn main() -> int` (returns the exit code) or `fn main([]str args)`
  (gets the command-line arguments; `args[0]` is the program's own path).

---

## 5. Variables

```c
x := 5               // new variable, type inferred (int)
int y = 10           // new variable, type written explicitly
int z                // no value: starts at zero ("" for str, [] for arrays, null for pointers)
Point p              // a struct with no value: zero, or the struct's default values

x = 7                // assign
x += 1               // also -= *= /= %=
```

- A name can be declared **once** in a function. Finch does not allow reusing a name in an
  inner block (no "shadowing"), because that is a common source of confusion.
- Variables exist from their declaration to the end of the `{ }` block they are in.
- Names: ASCII letters, digits and `_`, not starting with a digit. Non-ASCII letters are allowed only inside strings.
  Type names (`int`, `u8`, …), built-in function names and keywords are reserved.

**Keywords:** `fn return if else while for in break continue true false null import link struct defer`

---

## 6. Types

### Everyday types

| Type    | Holds | Example |
|---------|-------|---------|
| `int`   | whole number, 64-bit (same as `i64`) | `42`, `-7`, `0xFF`, `1_000_000` |
| `float` | decimal number, 64-bit (same as `f64`) | `3.14` |
| `bool`  | `true` or `false` | |
| `char`  | one byte-sized character | `'A'`, `'\n'` |
| `str`   | text | `"hello\tworld"` |
| `[]T`   | a list (array) of `T` | `[1, 2, 3]` |
| `map[K]V` | values of type `V` found by a key of type `K` | `["a": 1, "b": 2]` |
| your structs | named groups of values | `Point(1, 2)` |

String and char escapes: `\n` new line, `\t` tab, `\r`, `\0`, `\\`, `\"`, `\'`.

### Sized types (for files, hardware, C libraries)

| Signed | Range | Unsigned | Range |
|---|---|---|---|
| `i8`  | −128 … 127 | `u8`  | 0 … 255 |
| `i16` | −32 768 … 32 767 | `u16` | 0 … 65 535 |
| `i32` | about ±2.1 billion | `u32` | 0 … about 4.29 billion |
| `i64` | about ±9.2 × 10¹⁸ | `u64` | 0 … about 1.8 × 10¹⁹ |

`f32` is a 32-bit decimal number (about 7 significant digits), `f64` a 64-bit one (about 15–16).

### Conversions

Finch converts automatically **only when nothing can be lost**:

| From | To | Automatic? |
|---|---|---|
| a smaller whole number | a bigger one of the same signedness (`i8`→`i32`, `u8`→`u64`) | ✅ |
| unsigned | a strictly bigger signed (`u8`→`i16`, `u32`→`i64`) | ✅ |
| any whole number | `float` / `f32` | ✅ |
| `f32` | `f64` | ✅ |
| `null` | any pointer | ✅ |
| any pointer | `ptr`, and `ptr` to any pointer | ✅ |
| anything else | | ❌ write it: `type(value)` |

**Number literals adapt.** A number written in the code fits into any type that can hold it:
`u8 b = 200` works, `u8 b = 300` is an error. The same applies to constants imported from C headers.

**Explicit conversions** use the type name like a function:

```c
int(3.99)        // 3     (cuts off the fraction)
float(7) / 2     // 3.5
u8(300)          // 44    (keeps the lowest 8 bits)
int('A')         // 65
char(66)         // 'B'
bool(0)          // false (any non-zero number is true)
str(42)          // "42"  (also floats, bools, chars; str([]u8) turns bytes into text)
int("42")        // 42    (stops the program if the text isn't a number)
float("2.5")     // 2.5
ptr(p)           // a typed pointer as an untyped one
ptr(16)          // a number as a pointer (C APIs that pass offsets as pointers, like OpenGL)
int(p)           // a pointer as a number (its address)
str(p)           // a C char pointer as text
```

**Fixed-size numbers wrap around**, like real hardware: a `u8` holding 255 plus 1 becomes 0.
Calculations happen in the type of the values involved, so `u8(250) + 10` is `4`, not `260`.

---

## 7. Operators

### By priority (higher binds tighter)

| Priority | Operators | Meaning |
|---|---|---|
| 5 | `*` `/` `%` `<<` `>>` `&` | multiply, divide, remainder, shift left/right, bitwise AND |
| 4 | `+` `-` `\|` `^` | add (and join text), subtract, bitwise OR, bitwise XOR |
| 3 | `==` `!=` `<` `<=` `>` `>=` | comparisons |
| 2 | `&&` | logical AND |
| 1 | `\|\|` | logical OR |

Unary (in front of a value): `-x` negate, `!x` logical NOT, `~x` flip all bits.

> **Different from C, on purpose:** bit operators bind tighter than comparisons, so
> `flags & 4 != 0` means `(flags & 4) != 0`.

Notes:

- `/` on whole numbers rounds toward zero: `7 / 2` is `3`. `%` takes the sign of the left side.
- Dividing by zero **stops the program** with `runtime error: division by zero` and the line number.
- `&&` and `||` only evaluate the right side if needed.
- Text: `+` joins, `==` `!=` compare contents, `<` `>` compare alphabetically (byte by byte).
- Pointers can be compared with `==` and `!=`, including against `null`. A `str` is never `null`.
- Arrays and structs can't be compared with `==`; compare their parts.

---

## 8. Control flow

```c
if x > 10 {
    ...
} else if x > 0 {
    ...
} else {
    ...
}

while count > 0 {
    count -= 1
}

for i in 0..10 {          // i = 0, 1, …, 9  (the end is not included)
    if i == 3 { continue }
    if i == 8 { break }
}

for name in names {       // every element of an array (or every char of a str)
    print(name)
}

for i, name in names {    // with its index: 0, 1, 2, ...
    print(i, name)
}

for key, value in ages {  // every entry of a map, in the order the keys were added
    print(key, value)     // (for key in ages: just the keys)
}
```

- A condition must be a `bool`. `if x {` with a number is an error; write `if x != 0 {`.
- In `for i in a..b`, both ends are evaluated **once**. `i` is an `int` and cannot be changed in the loop.
- In `for x in list`, `x` cannot be changed either. To change elements, loop over the indexes:
  `for i in 0..list.len { list[i] = ... }`. Adding to the list inside the loop is fine.
- `while true { ... }` loops until a `break` or `return`.
- Code after `return`, `break` or `continue` in the same block is an error, because it can never run.

### defer

`defer` runs a statement when the current block ends, however it ends: normally, by `return`,
`break` or `continue`. Several defers run in reverse order. Handy for cleanup:

```c
f := fopen("data.txt", "r")
defer fclose(f)
// ... use f; it is closed when the block ends, on every path
```

A deferred statement can't contain `return`, `break` or `continue`.

---

## 9. Functions

```c
fn name(type1 param1, type2 param2) -> return_type {
    ...
    return value
}

fn no_result(str message) {       // no "->": returns nothing
    print(message)
}
```

- Arguments behave like copies: changing a parameter never changes the caller's variable.
  (Finch only really copies when needed: a function that doesn't change an array gets it for free.)
  To change the caller's variable, pass a pointer: see [Pointers](#14-pointers).
- A function with `-> type` must return a value on every path. Finch checks this.
- A function cannot be defined inside another function.
- A function that can fail is written `-> int!` (or `-> !` with no result); see
  [Errors as values](#errors-as-values).
- Long calls can be split over several lines inside the brackets; a comma after the last argument is fine.

---

## 10. Arrays and maps

```c
nums := [5, 3, 8]          // an array of int
[]str names                // an empty array of str
grid := [[1, 2], [3, 4]]   // arrays of arrays
[]f32 xs = [1, 2.5]        // the type decides what the numbers become
```

| Operation | Meaning |
|---|---|
| `a.len` | number of elements |
| `a[i]` | element `i` (from 0); out of range stops the program with a clear message |
| `a[i] = x` | change an element |
| `a.push(x)` | add at the end |
| `a.pop()` | remove the last one and give it back |
| `a.insert(i, x)` | put `x` at position `i`, moving the rest |
| `a.remove(i)` | remove position `i` and give it back |
| `a.clear()` | remove everything |
| `a.resize(n)` | make it `n` long (new elements are zero) |
| `a.contains(x)`, `a.find(x)` | is `x` there? its index, or -1 (numbers, chars, bools, str) |
| `a.sort()`, `a.reverse()` | in place (numbers, chars, str) |
| `a.slice(s, e)` | a new array with elements `s` … `e-1` |
| `a.join(sep)` | for `[]str`: one text with `sep` between the parts |
| `a.ptr` | the address of the first element (for C) |
| `print(a)` | prints `[1, 2, 3]` |

**Assigning copies:** after `b := a`, changing `b` does not change `a`.

### Maps

A `map[K]V` finds values by a key. Keys can be whole numbers, `char`, `bool` or `str`; values can be
anything (arrays, structs, other maps). Iterating goes in the order the keys were first added.

```c
ages := ["anna": 31, "bob": 25]     // a map literal: map[str]int
map[str][]str groups                // an empty map
map[int]str names = [:]             // [:] is an empty map where the type is known
```

| Operation | Meaning |
|---|---|
| `m[k]` | the value for `k`; a missing key stops the program with `the key "k" is not in the map` |
| `m[k] = v` | add or replace |
| `m[k] += 1`, `m[k].push(x)`, `m[k].field = …` | changing a missing key adds it first, with the value type's default (0, `""`, `[]`, …) |
| `m.len` | the number of keys |
| `m.has(k)` | is `k` there? |
| `m.get(k, default)` | the value, or `default` (only computed when needed) |
| `m.remove(k)` | removes `k`; `true` if it was there |
| `m.clear()` | removes everything |
| `m.keys()`, `m.values()` | new arrays, in insertion order |
| `print(m)`, `str(m)` | `{"anna": 31, "bob": 25}` |

```c
map[str]int counts
for word in text.split(" ") {
    counts[word] += 1                 // counting: a new word starts at 0
}
for word, n in counts {
    print(word, n)
}
```

Lookups are hash-based (average constant time). Like arrays, maps own their keys and values,
copy on assignment, and are freed at the end of their block. Don't add or remove keys of a map
inside a `for` over that same map; collect them in an array and change the map after the loop.

---

## 11. Text (str)

```c
s := "Hello"
s += ", world"            // join
print(s.len, s[0])        // 12 H   (.len counts bytes; ą, ż, … take 2)
s[0] = 'J'                // change a character
```

| Method | Gives back |
|---|---|
| `s.sub(a, b)` | the part from `a` to `b-1` |
| `s.find(t)` | where `t` starts, or -1 |
| `s.contains(t)`, `s.starts_with(t)`, `s.ends_with(t)` | `bool` |
| `s.split(sep)` | `[]str`; `split("")` gives single characters |
| `s.trim()` | without spaces/newlines at both ends |
| `s.upper()`, `s.lower()` | A–Z / a–z changed (ASCII letters) |
| `s.replace(a, b)` | every `a` replaced by `b` |
| `s.repeat(n)` | `s` n times |
| `s.bytes()` | `[]u8` with the bytes |
| `s.ptr` | a `ptr[char]` for C (valid while `s` lives) |

`str` keeps its length, so `.len` is instant, and it is always NUL-terminated, so passing it
to C as `char*` works.

---

## 12. Structs and methods

```c
struct Player {
    str name
    int lives = 3            // a default value
    []int scores
    Point pos                // structs inside structs
}

p := Player(name: "Ola")                     // by name: missing fields get their default (or zero)
q := Point(1, 2)                             // in order: then every field must be given
p.lives -= 1
p.scores.push(10)
p.pos.x = 5
print(p)       // Player(name: "Ola", lives: 2, scores: [10], pos: Point(x: 5, y: 0))
```

- Assigning or passing a struct copies it, together with its arrays and text.
- A struct can't contain itself directly; use `ptr[Node]` or `[]Node` for that field (trees, lists).

### Methods

A method is a function that belongs to a struct: `fn Struct.name(...)`. Inside, `self` is the struct
it was called on, and it is the caller's value itself, not a copy: changes to `self` stay.

```c
fn Player.heal(int amount) {
    self.lives += amount
}

fn Player.is_alive() -> bool {
    return self.lives > 0
}

p := Player(name: "Ola")
p.heal(2)                  // p.lives is now 5
if p.is_alive() { ... }

team[0].heal(1)            // on an array element, a field, m[key], through a ptr[Player] ...
```

- A method goes in the same file (module) as its struct, and is available wherever the struct is.
- Methods can also be added to **structs from C headers**: `fn Vector2.length() -> f32 { ... }`.
- A method can't have the name of a field.
- Calling a method that changes `self` on a loop variable (`for p in team { p.heal(1) }`) is an error,
  because the loop variable is read-only; use `for i in 0..team.len { team[i].heal(1) }`.
- Like functions, methods can fail (`-> !`); see [Errors as values](#errors-as-values).

---

## 13. Memory: who frees what

You never free arrays, text or structs. Finch does it for you, at a predictable moment:
**when the block that owns the value ends** (`}`), or earlier on `return`, `break`, `continue`.

- A variable **owns** its value. Assigning copies it, so two variables never share one list.
- A value produced by a function call or an expression is **moved** where it's stored: no copy.
- `return list` moves the variable out of the function: no copy.
- Function parameters are **lent**: the function gets the caller's value without a copy. If the
  function changes the parameter, Finch makes it a private copy first. You never notice except in speed.

For memory you manage yourself (linked lists, trees, sharing between structures):

```c
node := new(Node(value: 1))     // put a value on the heap, get a ptr[Node]
defer free(node)                // free(...) gives it back (and frees its arrays and text)
```

Memory from C (`malloc`, library objects) is freed with C's functions, as in C.

---

## 14. Pointers

A pointer holds the **address** of a value. Finch uses words instead of C's `*` and `&`:

| C | Finch | Meaning |
|---|---|---|
| `int *p` | `ptr[int] p` | p points to an int |
| `&x` | `addr(x)` | the address of x (also `addr(a[i])`, `addr(s.field)`) |
| `*p` | `p.value` | the value p points to |
| `p->field` | `p.field` | a field of the struct p points to |
| `p[i]` | `p[i]` | C-style indexing (no bounds check) |
| `NULL` | `null` | points nowhere |
| `void *` | `ptr` | points to something of unknown type |

```c
fn double(ptr[int] p) {
    p.value = p.value * 2
}

x := 21
double(addr(x))           // x is 42
```

If a struct has its own field called `value`, then `p.value` on a pointer to it means that field.

Using `.value`, `.field` or `[i]` on a `null` pointer **stops the program** with a clear message.

⚠️ A pointer to a variable must not be used after the variable's block has ended. Finch does not check this.

---

## 15. Built-in functions

| Function | What it does |
|---|---|
| `print(a, b, ...)` | Prints all values separated by spaces, then a new line. Works with every type, including arrays and structs. |
| `input()`, `input("question")` | Reads one line typed by the user (without the newline). Empty text at end of input. |
| `read_file(path)` | The whole file as `str`. Stops the program with a clear message if it can't be read, unless handled: `read_file(p) or ...`. |
| `write_file(path, text)` | Writes (replaces) the file; gives back `true` if it worked. With `or` / `try`, the failure carries the reason. |
| `file_exists(path)` | `true` / `false` |
| `delete_file(path)` | Deletes the file; `true` if it worked (or the reason, with `or` / `try`). |
| `error(message)` | The failure a fallible function returns: `return error("...")`. |
| `shell(command)` | Runs a shell command, gives back its exit code. |
| `exit(code)` | Ends the program right away. |
| `addr(x)` | A pointer to `x`; `addr(function)` gives C a function pointer (callback). |
| `new(value)`, `free(p)` | Manual heap memory (see [Memory](#13-memory-who-frees-what)). |
| `int(x)`, `str(x)`, `u8(x)`, … | Conversions (see [Conversions](#conversions)). `str(x)` works on every type and gives the same text `print` shows. `int(text) or 0` handles text that isn't a number. |

---

## 16. Modules

Split a program into files. `import shapes` loads `shapes.fch` from the same folder (or from a
folder listed in the `FINCH_PATH` environment variable, separated by `:`).

```c
// shapes.fch
struct Box {
    float w
    float h
}

fn area(Box b) -> float {
    return b.w * b.h
}
```

```c
// main.fch
import shapes

fn main() {
    b := shapes.Box(2, 3)
    print(shapes.area(b))
    shapes.Box other         // a module's type in a declaration
}
```

- Everything in a module is used with the module's name in front: `shapes.area`, `shapes.Box`.
- A module doesn't have `main`. Modules can import other modules.

---

## 17. Using C libraries

### Step 1: import the header

```c
import "stdio.h"
import "GLFW/glfw3.h"     // a path inside the system include folders
import "mylib.h"          // a header next to your .fch file
```

Finch reads the header with **libclang** (clang's C parser) and makes these usable:

- **functions**, including ones with a variable number of arguments, like `printf`,
- **structs**, by value and by pointer (`Vector2`, `SDL_Rect`, `CXCursor`, …), with their fields,
- **`enum` values** and **simple `#define` constants** (`M_PI`, `GL_COLOR_BUFFER_BIT`),
- **global variables**, like `stdout` and `stderr`.

### Step 2: link the library

```c
link "glfw"              // uses libglfw.so (pkg-config is asked first, if installed)
link "helpers.c"         // compile and link your own C file (next to the .fch file)
link "prebuilt.a"        // or an object file / static library
```

If you forget the `link` line, Finch tells you exactly what is missing:

```
error: glfwInit, glfwCreateWindow, glfwMakeContextCurrent and 5 more come from "GLFW/glfw3.h", but its library isn't linked
  ...
      link "glfw"
```

### How C types appear in Finch

| C | Finch |
|---|---|
| `char`, `signed char`, `unsigned char` | `char`, `i8`, `u8` |
| `short`, `int`, `long`, `long long` | `i16`, `i32`, `i64`, `i64` (by actual size) |
| `unsigned …` | `u16`, `u32`, `u64` |
| `float`, `double` | `f32`, `f64` |
| `_Bool` / `bool` | `bool` |
| `enum` | the matching signed integer |
| `char *` as a **parameter or result** | `str` |
| `char *` inside a struct or behind a pointer | `ptr[char]` (use `str(p)` to read it) |
| `struct X` by value | `X`, a struct with the same fields |
| `struct X *`, `int *`, … | `ptr[X]`, `ptr[i32]`, … |
| `int arr[16]` inside a struct | `[16]i32`: index it and use `.len` |
| `void *`, function pointers, incomplete structs | `ptr` |

### Example: structs and callbacks

```c
import "raylib.h"
link "raylib"

fn main() {
    InitWindow(800, 450, "Finch")
    while !WindowShouldClose() {
        BeginDrawing()
        ClearBackground(Color(r: 30, g: 30, b: 40, a: 255))
        DrawCircleV(Vector2(400, 225), 50, Color(255, 136, 0, 255))
        EndDrawing()
    }
    CloseWindow()
}
```

To give C a function to call back (like `qsort`'s comparator), use `addr(myFunction)`.
Its parameters must be C types (numbers, pointers, C structs).

### Graphics: OpenGL and Vulkan

Both work directly; `examples/opengl.fch` and `examples/vulkan.fch` are complete programs.

- **OpenGL 3+** on Linux: the modern functions (`glCreateShader`, `glGenVertexArrays`, …) are only
  declared when `GL_GLEXT_PROTOTYPES` is defined. Finch can't define C macros, so put the includes in a
  tiny header next to your program and import that:

  ```c
  // opengl.h
  #define GL_GLEXT_PROTOTYPES
  #include <GL/gl.h>
  #include <GL/glext.h>
  #include <GLFW/glfw3.h>
  ```

  Then `import "opengl.h"`, `link "glfw"`, `link "GL"`. Out-parameters use `addr(...)`
  (`glGenBuffers(1, addr(vbo))`), buffer data uses `.ptr` (`vertices.ptr`), and byte offsets use
  `ptr(...)` (`glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, ptr(8))`).
- **Vulkan**: `import "vulkan/vulkan.h"` and `link "vulkan"`. Vulkan's create-info structs are built with
  named fields (missing ones are zero), handles are `ptr`, and lists come from the usual "call twice"
  pattern: once for the count, once with `list.ptr` after `list.resize(count)`. Function-like macros such as
  `VK_MAKE_API_VERSION` aren't available; compute the number instead (`1 << 22` is Vulkan 1.0).

### What does not work yet

- C **unions** by value, and structs with **bit fields** by value (pointers to them work).
- **Function-like macros** and macros that are not a single number.
- `long double`, 128-bit integers.

---

## 18. Errors

### Errors as values

Some failures are normal: the user types `abc`, a file is missing. Finch handles them without
exceptions and without hidden control flow. A function that can fail says so with `!` after its result
type, and fails with `return error("message")`:

```c
fn parse_port(str text) -> int! {
    n := int(text) or { return error("'" + text + "' is not a number") }
    if n < 1 || n > 65535 {
        return error("port " + str(n) + " is out of range")
    }
    return n
}

fn save(str path, str text) -> ! {     // can fail, gives back nothing
    try write_file(path, text)
}
```

The caller **must** handle the failure, in one of three ways (forgetting is a compile error):

| Code | On failure |
|---|---|
| `port := parse_port(s) or 8080` | use this value instead (it's only computed on failure; it can be another call with `or`) |
| `port := parse_port(s) or { print(err); return }` | run the block; `err` is the message (a `str`). The block must leave (`return`, `break`, `continue`, `exit`) unless the result isn't used. |
| `port := try parse_port(s)` | fail the current function with the same error (it must be fallible too) |

```c
fn load(str path) -> Config! {
    text := try read_file(path)
    port := try parse_port(text.trim())
    return Config(port: port)
}

fn main() -> ! {                // main can fail too: the error is printed, and the exit code is 1
    config := try load("app.conf")
    ...
}
```

Built-ins that can fail work the same way: `int(text)`, `float(text)`, `read_file`, `write_file`,
`delete_file`. Without `or` / `try` they behave as before (`int("x")` stops the program,
`write_file` gives back a `bool`).

Under the hood a fallible function returns its value plus a flag and the message: no allocation on
success, no unwinding, and the cost of a failure is building its message.

### Compile errors

```
game.fch:12:9: error: 'score' must be int, but this is str
   12 |     score = "high"
      |             ^
```

Format: `file:line:column: error: explanation`, then the line with a `^` under the spot.

### Runtime errors

Instead of undefined behavior (as in C), Finch stops with a message and exit code 1:

| Message | Cause |
|---|---|
| `runtime error: division by zero` | `/` or `%` by a variable that was 0 |
| `runtime error: division overflows …` | the smallest signed number divided by −1 |
| `runtime error: index 5 is out of range (the length is 3)` | `a[5]` on a shorter array or str |
| `runtime error: pop() on an empty array` | |
| `runtime error: used .value on a null pointer` | also `.field` and `[i]` through `null` |
| `runtime error: can't turn "x" into int` | `int(...)` / `float(...)` on text that isn't a number |
| `runtime error: can't read the file "…"` | `read_file` on a missing or unreadable file (handle it with `or`) |
| `runtime error: the key "x" is not in the map …` | `m["x"]` when the map has no such key (use `.has` or `.get`) |
| `runtime error: called .f() on a null pointer` | a method called through a `null` `ptr[T]` |

---

## 19. Editors: VS Code, Kate and others

Every editor feature comes from the same language server, `finch lsp`, which uses the real compiler:
errors as you type, types and docs on hover, go to definition, completion (fields, methods, map and array
methods after `.`, module members, names from C headers), the file's outline, and parameter hints.

### Kate (and KWrite, KDevelop)

```sh
editors/kate/install.sh
```

It installs the highlighting (`~/.local/share/org.kde.syntax-highlighting/syntax/finch.xml`) and adds `finch`
to Kate's LSP client settings, keeping your other servers. Restart Kate and enable the **LSP Client** plugin.
To run programs, add `finch run %f` as a target in the **Build & Run** plugin; Finch's
`file:line:column: error:` messages are then clickable. Details: `editors/kate/README.md`.

### VS Code

The **Finch** extension (`editors/vscode`, also attached to each release as a `.vsix` file) gives you:
highlighting, errors as you type, completion (also fields and methods after `.`), types on hover,
go to definition (F12), an outline of the file, parameter hints, snippets, and a ▶ **Run** button
(`Ctrl+F5`). Errors from **Finch: Build This File** appear in the Problems panel.

Install: Extensions → `…` → *Install from VSIX…* → pick `finch-lang-*.vsix`. The extension runs `finch`
from your PATH; if it is somewhere else, set **Finch: Path** in the settings.

### Others

Neovim, Helix, Zed, Emacs, Sublime and any other editor that speaks the Language Server Protocol:
run `finch lsp` for files ending in `.fch` (stdin/stdout, no options).

## 20. Debugging

```sh
finch build game.fch -g -O0 -o game
gdb ./game
(gdb) break game.fch:12
(gdb) run
(gdb) bt                # where are we, through which functions
(gdb) info locals       # all variables
(gdb) print player.lives
```

`-g` adds debug info (lines, functions, variables, struct fields). `-O0` keeps every variable
visible; without it the optimizer may remove some. On Windows, `-g` writes CodeView, which the
Visual Studio debugger and WinDbg read.

---

## 21. Troubleshooting

| Problem | Solution |
|---|---|
| `can't find the C header 'x.h'` | The library's development files are missing. Install them (Debian: `libx-dev`; Nix: add to `shell.nix`). |
| `… come from "x.h", but its library isn't linked` | Add `link "name"` at the top of the file. |
| `the library 'x' wasn't found` | Not installed, or a wrong name. On Nix, add it to `shell.nix` and run inside `nix-shell`. |
| `the C function 'f' can't be used from Finch yet` | It uses a union, bit fields or another unsupported type by value. Look for a variant that takes pointers. |
| `can't find the module 'x'` | Put `x.fch` next to the importing file, or set `FINCH_PATH`. |
| `couldn't build the Finch runtime` | No working C compiler. Install gcc or clang, or set `CC`. |
| A library is not found when the program starts | It was linked from a folder the system doesn't search. Run inside the same `nix-shell`, or install it system-wide. |
| Windows: `finch` can't link (`LNK…` errors about `libcmt`, `kernel32`) | Install Visual Studio Build Tools with *Desktop development with C++*. |
| Windows: `finch.exe` doesn't start (missing `libclang.dll`) | Keep `libclang.dll` next to `finch.exe`. |
| macOS: `finch` doesn't start (`Library not loaded: …libLLVM…`) | `brew install llvm@21`. |
| macOS: `can't find the C header 'stdio.h'` | Install the command line tools: `xcode-select --install`. |
| `--target arm64`: `no C compiler found to link with` | Install `aarch64-linux-gnu-gcc` (Debian: `gcc-aarch64-linux-gnu`), or set `FINCH_CC`. |
| `clang` inside `nix-shell` can't find `stdio.h` | You have an old `shell.nix`: `llvmPackages.clang` must be listed before `llvmPackages.libclang`. |
| A program never ends | A loop condition never becomes false. Press Ctrl+C. |

---

## 22. Project layout and tests

```
Finch/
├── src/            the compiler (C++)
├── runtime/        finch_rt.c: the small runtime (text, arrays, input, files)
├── boot/           the Finch compiler written in Finch (see the engineers' guide)
├── examples/       hello, tour, types, structs, todo, guess, c_import, window, opengl, vulkan, raylib, llvm
├── tests/
│   ├── run/        programs + the exact output they must print (.out), input (.in)
│   ├── fail/       programs that must fail, with the expected error in line 1
│   ├── run.sh      the test runner (MEMCHECK=1 also checks memory with valgrind)
│   └── boot.sh     builds the self-hosted compiler with itself and compares
├── editors/        the VS Code extension (vscode/), Kate highlighting and LSP setup (kate/)
├── docs/           this documentation (en, pl), including the cheat sheet
├── shell.nix       the Nix development environment
└── CMakeLists.txt
```

```sh
tests/run.sh                 # → 74 passed, 0 failed
MEMCHECK=1 tests/run.sh      # the same under valgrind: no leaks, no bad memory access
tests/boot.sh                # the self-hosting check (needs clang)
tests/cross.sh windows       # every test built for Windows and run with Wine
tests/cross.sh arm64         # every test built for ARM64 Linux and run with QEMU
python3 tests/lsp_test.py    # the language server
```

---

## 23. Current limits

Not there yet:

- Generics (your own `List[T]`), interfaces, `match`, enums written in Finch.
- Unicode-aware text: `.len`, `s[i]` and `upper()` work on bytes / ASCII.
- Threads.
- Windows on ARM64, 32-bit systems, WebAssembly.

See the [roadmap in the README](../../README.md#roadmap).
