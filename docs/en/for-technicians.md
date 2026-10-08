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
10. [Arrays](#10-arrays)
11. [Text (str)](#11-text-str)
12. [Structs](#12-structs)
13. [Memory: who frees what](#13-memory-who-frees-what)
14. [Pointers](#14-pointers)
15. [Built-in functions](#15-built-in-functions)
16. [Modules](#16-modules)
17. [Using C libraries](#17-using-c-libraries)
18. [Errors](#18-errors)
19. [Debugging](#19-debugging)
20. [Troubleshooting](#20-troubleshooting)
21. [Project layout and tests](#21-project-layout-and-tests)
22. [Current limits](#22-current-limits)

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

Finch currently runs on **Linux, x86-64**. It has been tested with **LLVM 21**.
Other LLVM versions will likely fail to compile, because LLVM's C++ API changes between releases.

### Option A: Nix (recommended, nothing to install by hand)

```sh
git clone https://github.com/Nexoniarz/Finch.git
cd Finch
nix-shell                        # downloads LLVM, libclang, clang, cmake, ninja, pkg-config
cmake -S . -B build -G Ninja
ninja -C build
./build/finch version            # finch 2.0.0 (LLVM 21.x)
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
finch version                      show the version

options:
  -l <lib>   link a C library, same as  link "lib"  in the file
  -g         add debug info (for gdb / lldb)
  -O0        skip optimizations
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

---

## 10. Arrays

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

## 12. Structs

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
- Structs have no methods. Write functions that take the struct: `fn heal(Player p) -> Player`.

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
| `read_file(path)` | The whole file as `str`. Stops the program with a clear message if it can't be read. |
| `write_file(path, text)` | Writes (replaces) the file; gives back `true` if it worked. |
| `file_exists(path)` | `true` / `false` |
| `shell(command)` | Runs a shell command, gives back its exit code. |
| `exit(code)` | Ends the program right away. |
| `addr(x)` | A pointer to `x`; `addr(function)` gives C a function pointer (callback). |
| `new(value)`, `free(p)` | Manual heap memory (see [Memory](#13-memory-who-frees-what)). |
| `int(x)`, `str(x)`, `u8(x)`, … | Conversions (see [Conversions](#conversions)). |

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

### What does not work yet

- C **unions** by value, and structs with **bit fields** by value (pointers to them work).
- **Function-like macros** and macros that are not a single number.
- `long double`, 128-bit integers.

---

## 18. Errors

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
| `runtime error: can't read the file "…"` | `read_file` on a missing or unreadable file |

---

## 19. Debugging

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
visible; without it the optimizer may remove some.

---

## 20. Troubleshooting

| Problem | Solution |
|---|---|
| `can't find the C header 'x.h'` | The library's development files are missing. Install them (Debian: `libx-dev`; Nix: add to `shell.nix`). |
| `… come from "x.h", but its library isn't linked` | Add `link "name"` at the top of the file. |
| `the library 'x' wasn't found` | Not installed, or a wrong name. On Nix, add it to `shell.nix` and run inside `nix-shell`. |
| `the C function 'f' can't be used from Finch yet` | It uses a union, bit fields or another unsupported type by value. Look for a variant that takes pointers. |
| `can't find the module 'x'` | Put `x.fch` next to the importing file, or set `FINCH_PATH`. |
| `couldn't build the Finch runtime` | No working C compiler. Install gcc or clang, or set `CC`. |
| A library is not found when the program starts | It was linked from a folder the system doesn't search. Run inside the same `nix-shell`, or install it system-wide. |
| `clang` inside `nix-shell` can't find `stdio.h` | You have an old `shell.nix`: `llvmPackages.clang` must be listed before `llvmPackages.libclang`. |
| A program never ends | A loop condition never becomes false. Press Ctrl+C. |

---

## 21. Project layout and tests

```
Finch/
├── src/            the compiler (C++)
├── runtime/        finch_rt.c: the small runtime (text, arrays, input, files)
├── boot/           the Finch compiler written in Finch (see the engineers' guide)
├── examples/       hello, tour, types, structs, todo, guess, c_import, window, raylib, llvm
├── tests/
│   ├── run/        programs + the exact output they must print (.out), input (.in)
│   ├── fail/       programs that must fail, with the expected error in line 1
│   ├── run.sh      the test runner (MEMCHECK=1 also checks memory with valgrind)
│   └── boot.sh     builds the self-hosted compiler with itself and compares
├── docs/           this documentation (en, pl)
├── shell.nix       the Nix development environment
└── CMakeLists.txt
```

```sh
tests/run.sh                 # → 59 passed, 0 failed
MEMCHECK=1 tests/run.sh      # the same under valgrind: no leaks, no bad memory access
tests/boot.sh                # the self-hosting check (needs clang)
```

---

## 22. Current limits

Finch 2.0 is a complete small language, but not a finished one. Not there yet:

- Methods on structs, generics, maps/dictionaries, `match`. (Use arrays of structs and functions.)
- Error values: errors in `int("x")` or `read_file` stop the program. Check first (`file_exists`).
- Unicode-aware text: `.len`, `s[i]` and `upper()` work on bytes / ASCII.
- Threads.
- Platforms other than Linux x86-64.

See the [roadmap in the README](../../README.md#roadmap).
