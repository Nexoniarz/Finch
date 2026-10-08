# Finch for Technicians

**For people who know their way around a computer, but little or no programming.**
This guide covers installing Finch, using the `finch` tool, the whole language, using
C libraries, and fixing common problems. It does not cover how the compiler works inside.
For that, see the [guide for engineers](for-engineers.md).

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
10. [Pointers](#10-pointers)
11. [Using C libraries](#11-using-c-libraries)
12. [Errors](#12-errors)
13. [Troubleshooting](#13-troubleshooting)
14. [Project layout and tests](#14-project-layout-and-tests)
15. [Current limits](#15-current-limits)

---

## 1. What Finch is

Finch is a **compiled** programming language. The `finch` tool translates your `.fn`
file into a real program (a native executable, like the ones made from C), using the
**LLVM** compiler toolkit, the same backend that powers clang, Rust and Swift.

What that gives you:

- **Speed close to C.** There is no interpreter, virtual machine or garbage collector.
- **Small, standalone programs** that you can copy and run.
- **Direct access to C libraries.** `import "stdio.h"` and call anything in it.

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
nix-shell                        # downloads LLVM, libclang, cmake, ninja, pkg-config
cmake -S . -B build -G Ninja
ninja -C build
./build/finch version            # finch 1.0.0 (LLVM 21.x)
```

### Option B: your distribution's packages

You need: a C/C++ compiler, CMake ≥ 3.20, Ninja (or Make), LLVM 21 development files,
libclang 21 development files, and optionally `pkg-config`.

| Distribution | Packages (names can differ slightly by version) |
|---|---|
| Debian / Ubuntu | `build-essential cmake ninja-build pkg-config llvm-21-dev libclang-21-dev` |
| Fedora | `gcc-c++ cmake ninja-build pkgconf llvm-devel clang-devel` |
| Arch | `base-devel cmake ninja pkgconf llvm clang` |

If CMake cannot find LLVM, point it there:

```sh
cmake -S . -B build -G Ninja -DLLVM_DIR=/usr/lib/llvm-21/lib/cmake/llvm
ninja -C build
```

### Putting `finch` on your PATH (optional)

```sh
sudo cp build/finch /usr/local/bin/
finch version
```

Finch needs a working C compiler (`cc`) at run time. It uses it as the **linker** that
turns compiled code into a program. Set the `CC` environment variable to use a different one.

---

## 3. The finch command

```
finch run   <file.fn>              compile and run right away
finch build <file.fn> [-o name]    compile into a program (default name: the file's name)
finch ir    <file.fn>              print the LLVM IR (for the curious)
finch version                      show the version

options:
  -l <lib>   link a C library, same as  link "lib"  in the file
  -O0        skip optimizations
```

Examples:

```sh
finch run hello.fn                 # prints the output
finch build game.fn -o mygame      # creates ./mygame
./mygame
finch run window.fn -l glfw        # with a C library
```

`finch run` passes on your program's exit code. If the program crashes, it tells you
how (for example `the program crashed: Segmentation fault`).

---

## 4. How a Finch file looks

```c
// comments start with two slashes
/* or span
   several lines */

import "math.h"          // C headers (optional), always at the top
link "m"                 // C libraries to link (optional)

fn main() {              // the program starts here
    print("Hello")
}
```

Rules worth knowing:

- **One statement per line.** There are no semicolons. To split a long expression,
  break the line **after** an operator (`a +⏎ b`) or anywhere inside brackets `( )`.
- **Curly braces are always required** after `if`, `else`, `for`, `while`, and the opening
  `{` goes on the same line.
- **No brackets around conditions:** `if x > 5 {`, not `if (x > 5) {`.
- Functions can be in any order in the file.
- `main` is either `fn main()` or `fn main() -> int` (returns the exit code).

---

## 5. Variables

```c
x := 5               // new variable, type inferred (int)
int y = 10           // new variable, type written explicitly
int z                // no value: starts at zero ("" for str, null for pointers)

x = 7                // assign
x += 1               // also -= *= /= %=
```

- A name can be declared **once** in a function. Finch does not allow reusing a name in an
  inner block (no "shadowing"), because that is a common source of confusion.
- Variables exist from their declaration to the end of the `{ }` block they are in.
- Names: ASCII letters, digits and `_`, not starting with a digit. Non-ASCII letters are allowed only inside strings. Type names (`int`, `u8`, …) and
  keywords are reserved.

**Keywords:** `fn return if else while for in break continue true false null import link`

---

## 6. Types

### Everyday types

| Type    | Holds | Example |
|---------|-------|---------|
| `int`   | whole number, 64-bit (same as `i64`) | `42`, `-7`, `0xFF`, `1_000_000` |
| `float` | decimal number, 64-bit (same as `f64`) | `3.14` |
| `bool`  | `true` or `false` | |
| `char`  | one byte-sized character | `'A'`, `'\n'` |
| `str`   | text (a C string) | `"hello\tworld"` |

String and char escapes: `\n` new line, `\t` tab, `\r`, `\0`, `\\`, `\"`, `\'`.

### Sized types (for files, hardware, C libraries)

| Signed | Range | Unsigned | Range |
|---|---|---|---|
| `i8`  | −128 … 127 | `u8`  | 0 … 255 |
| `i16` | −32 768 … 32 767 | `u16` | 0 … 65 535 |
| `i32` | about ±2.1 billion | `u32` | 0 … about 4.29 billion |
| `i64` | about ±9.2 × 10¹⁸ | `u64` | 0 … about 1.8 × 10¹⁹ |

`f32` is a 32-bit decimal number (about 7 significant digits), `f64` a 64-bit one (about 15–16).

### Pointers

`ptr[T]` points to a value of type `T`. Plain `ptr` points to "something" with unknown
type, like `void*` in C. See [Pointers](#10-pointers).

### Conversions

Finch converts automatically **only when nothing can be lost**:

| From | To | Automatic? |
|---|---|---|
| a smaller whole number | a bigger one of the same signedness (`i8`→`i32`, `u8`→`u64`) | ✅ |
| unsigned | a strictly bigger signed (`u8`→`i16`, `u32`→`i64`) | ✅ |
| any whole number | `float` / `f32` | ✅ |
| `f32` | `f64` | ✅ |
| `null` | any pointer | ✅ |
| any pointer, `str` | `ptr` | ✅ |
| `ptr` | any `ptr[T]` | ✅ |
| anything else | | ❌ write it: `type(value)` |

**Number literals adapt.** A number written in the code fits into any type that can hold it:
`u8 b = 200` works, `u8 b = 300` is an error (`the number 300 doesn't fit in u8`).
The same applies to constants imported from C headers.

**Explicit conversions** use the type name like a function:

```c
int(3.99)      // 3     (cuts off the fraction)
int(-3.99)     // -3
float(7) / 2   // 3.5
u8(300)        // 44    (keeps the lowest 8 bits)
i8(200)        // -56
int('A')       // 65
char(66)       // 'B'
bool(0)        // false (any non-zero number is true)
ptr(text)      // a str as an untyped pointer
str(p)         // a pointer as a str
```

**Fixed-size numbers wrap around**, like real hardware: a `u8` holding 255 plus 1 becomes 0.
Calculations happen in the type of the values involved, so `u8(250) + 10` is `4`, not `260`.
Convert first if you need more room: `int(x) + 10`.

---

## 7. Operators

### By priority (higher binds tighter)

| Priority | Operators | Meaning |
|---|---|---|
| 5 | `*` `/` `%` `<<` `>>` `&` | multiply, divide, remainder, shift left/right, bitwise AND |
| 4 | `+` `-` `\|` `^` | add, subtract, bitwise OR, bitwise XOR |
| 3 | `==` `!=` `<` `<=` `>` `>=` | comparisons |
| 2 | `&&` | logical AND |
| 1 | `\|\|` | logical OR |

Unary (in front of a value): `-x` negate, `!x` logical NOT, `~x` flip all bits.

> **Different from C, on purpose:** bit operators bind tighter than comparisons, so
> `flags & 4 != 0` means `(flags & 4) != 0`. In C it silently means something else.

Notes:

- `/` on whole numbers rounds toward zero: `7 / 2` is `3`, `-7 / 2` is `-3`. `%` takes the sign of the left side.
- Dividing by zero **stops the program** with `runtime error: division by zero` and the line number.
  If the zero is written in the code, it is a compile error instead.
- `&&` and `||` only evaluate the right side if needed.
- Text (`str`) can be compared with `==` and `!=` (it compares the contents). `<` and `>` do not work on text.
- Pointers can be compared with `==` and `!=`, including against `null`.
- Bit operators need whole numbers. For `bool` use `&&` and `||`.
- `>>` on signed numbers keeps the sign (`-16 >> 2` is `-4`). On unsigned numbers it fills with zeros.

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
```

- A condition must be a `bool`. `if x {` with a number is an error; write `if x != 0 {`.
- In `for i in a..b`, both ends are whole numbers evaluated **once** before the loop starts.
  `i` is an `int` and cannot be changed inside the loop.
- `while true { ... }` loops until a `break` or `return`.
- Code after `return`, `break` or `continue` in the same block is an error, because it can never run.

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

- Parameters are copies. Changing a parameter does not change the caller's variable.
  Use a pointer for that (see below).
- A function with `-> type` must return a value on every path. Finch checks this.
- A function cannot be defined inside another function.
- Built-in names you cannot reuse: `print`, `addr`, and all type names.

### Built-in functions

| Function | What it does |
|---|---|
| `print(a, b, ...)` | Prints all values separated by spaces, then a new line. Works with every type. |
| `addr(x)` | Gives a pointer to variable `x`. |
| `int(x)`, `u8(x)`, `float(x)`, … | Converts types (see [Conversions](#conversions)). |

---

## 10. Pointers

A pointer holds the **address** of a value: where it lives in memory. Finch uses words
instead of C's `*` and `&`:

| C | Finch | Meaning |
|---|---|---|
| `int *p` | `ptr[int] p` | p points to an int |
| `&x` | `addr(x)` | the address of x |
| `*p` | `p.value` | the value p points to |
| `NULL` | `null` | points nowhere |
| `void *` | `ptr` | points to something of unknown type |

```c
fn double(ptr[int] p) {
    p.value = p.value * 2
}

fn main() {
    x := 21
    double(addr(x))
    print(x)               // 42

    ptr[int] nothing = null
    if nothing == null {
        print("empty")
    }
}
```

Pointers can point to pointers: `ptr[ptr[int]]`, then `pp.value.value`.

Using `.value` on a `null` pointer **stops the program** with
`runtime error: used .value on a null pointer` instead of crashing randomly.

⚠️ A pointer to a variable must not be used after the function that owns the variable
has returned. The variable no longer exists then. Finch does not check this yet.

---

## 11. Using C libraries

Almost every library on Linux has a C interface: graphics, sound, networking, databases.
Finch can use them directly.

### Step 1: import the header

```c
import "stdio.h"
import "math.h"
import "GLFW/glfw3.h"     // a path inside the system include folders
import "mylib.h"          // a header next to your .fn file
```

Finch reads the header with **libclang** (clang's C parser) and makes these usable:

- **functions**, including ones with a variable number of arguments, like `printf`,
- **`enum` values**,
- **simple `#define` constants** that are a single number, like `M_PI`, `EOF`, `GL_COLOR_BUFFER_BIT`,
- **global variables**, like `stdout` and `stderr`.

### Step 2: link the library

A header only *describes* functions. Their actual code is in a library file (`libNAME.so`).
Tell Finch which library to use, at the top of the file:

```c
link "glfw"              // uses libglfw.so
link "GL"                // uses libGL.so
```

Write just the name: for `libglfw.so` that is `glfw`. If `pkg-config` knows the name, Finch uses
its settings (paths and extra libraries). Otherwise it passes `-lNAME` to the linker.
The C standard library and the math library (`libm`) are always linked.

On the command line, `-l NAME` does the same as `link "NAME"`.

If you forget the `link` line, Finch tells you exactly what is missing:

```
error: glfwInit, glfwCreateWindow, glfwMakeContextCurrent and 5 more come from "GLFW/glfw3.h", but its library isn't linked
  the header only says the functions exist; their code lives in a library.
  add this at the top of your file (with the library's real name):

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
| `char *`, `const char *` | `str` |
| `int *`, `double *`, … | `ptr[i32]`, `ptr[f64]`, … |
| `void *`, `struct X *`, `FILE *`, function pointers | `ptr` |

So C's `int` is Finch's `i32`. Literals adapt automatically (`abs(-5)` works),
but a Finch `int` variable must be converted: `abs(i32(x))`.

### Example

```c
import "stdio.h"
import "math.h"
import "stdlib.h"

fn main() {
    printf("%d + %d = %d\n", 2, 3, 5)
    print(sqrt(2), M_PI, RAND_MAX)

    ptr[i32] n = malloc(4)       // ask C for 4 bytes of memory
    n.value = 7
    print(n.value)
    free(n)                      // and give them back

    fprintf(stderr, "this goes to the error output\n")
}
```

A complete graphics example is in `examples/window.fn`: a GLFW + OpenGL window that changes color.
`examples/llvm.fn` shows that Finch can even drive LLVM itself through its C API.

### What does not work yet

- C functions that take or return a **`struct` by value**, such as `div()`. Finch says so
  clearly if you try. Functions taking a *pointer* to a struct are fine.
- **Function-like macros** (`#define MAX(a,b) ...`) and macros that are not a single number.
- `long double`, 128-bit integers.
- Reading the **fields** of a C struct.

---

## 12. Errors

### Compile errors

Finch stops at the first problem and shows where it is:

```
game.fn:12:9: error: 'score' must be int, but this is str
   12 |     score = "high"
      |             ^
```

Format: `file:line:column: error: explanation`, then the line with a `^` under the spot.

### Runtime errors

Some problems can only be seen while the program runs. Instead of undefined behavior
(as in C), Finch stops with a clear message on the error output and exit code 1:

| Message | Cause |
|---|---|
| `runtime error: division by zero` | `/` or `%` by a variable that was 0 |
| `runtime error: division overflows …` | the smallest signed number divided by −1 |
| `runtime error: used .value on a null pointer` | reading or writing `p.value` while `p` is `null` |

---

## 13. Troubleshooting

| Problem | Solution |
|---|---|
| `can't find the C header 'x.h'` | The library's development files are missing. Install them (on Debian: `libx-dev`; on Nix: add to `shell.nix`). For your own header, put it next to the `.fn` file. |
| `… come from "x.h", but its library isn't linked` | Add `link "name"` at the top of the file. |
| `the library 'x' wasn't found` | The library is not installed, or the name is wrong. On Nix, add it to `shell.nix` and run inside `nix-shell`. |
| `the C function 'f' can't be used from Finch yet` | It uses a struct by value, or another unsupported type. Look for a variant that takes pointers. |
| `no C compiler found to link with` | Install gcc or clang, or set `CC`. |
| The program runs, but a C library is not found at start | It was linked from a path the system does not search. Run it inside the same `nix-shell`, or install the library system-wide. |
| CMake: `libclang not found` | Install the libclang dev package, or run `cmake` inside `nix-shell`. |
| Build fails with LLVM API errors | You have a different LLVM version than 21. |
| A program never ends | A loop condition never becomes false. Press Ctrl+C. |

---

## 14. Project layout and tests

```
Finch/
├── src/            the compiler (C++)
├── examples/       example programs (hello, tour, types, c_import, window, llvm)
├── tests/
│   ├── run/        programs + the exact output they must print (.out)
│   ├── fail/       programs that must fail, with the expected error in line 1
│   └── run.sh      the test runner
├── docs/           this documentation (en, pl)
├── shell.nix       the Nix development environment
└── CMakeLists.txt
```

Run the tests after building:

```sh
tests/run.sh             # → 34 passed, 0 failed
```

---

## 15. Current limits

Finch 1.0 is a solid core, not a finished language. Not there yet:

- `struct`, arrays, reading keyboard input (`input()`), string building (`"a" + "b"`).
  For now, use C functions for these (`scanf`, `snprintf`, `malloc`…).
- Automatic memory management (planned: memory freed at the end of the block, plus `defer`).
- Finch modules (one program = one `.fn` file for now).
- Platforms other than Linux x86-64.

See the [roadmap in the README](../../README.md#roadmap).
