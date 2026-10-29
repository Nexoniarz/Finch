<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="assets/logo-dark.svg">
    <img alt="Finch" src="assets/logo.svg" width="460">
  </picture>
</p>

**Small, quick, sharp.** A programming language on LLVM with C-like syntax, simple enough
to learn in an hour and as fast as C.

> 🇵🇱 [Polska wersja](README.pl.md)

```c
import "math.h"

fn hypot2(float a, float b) -> float {
    return sqrt(a * a + b * b)
}

fn main() {
    name := "Finch"
    print("Hello from", name, "!")

    for i in 1..4 {
        print(i, "→", hypot2(float(i), 1.0))
    }
}
```

## Why Finch

- **Simple.** No semicolons, no headers, no preprocessor. Each thing has one way to do it.
- **Fast.** It compiles through LLVM's `-O2` pipeline to native code, with no garbage collector.
  Recursive `fib(40)` runs in about 0.19 s (`clang -O2` on the same C: about 0.23 s); bounds-checked
  array code matches C.
- **Memory without the pain.** Lists, text and structs are values: assigning copies, and the end of a
  block frees them. There is no `free` to forget, no use-after-free, and every test is clean under valgrind.
- **Safer than C, where it is cheap.** No silent narrowing and no signed/unsigned mixing. Out-of-range
  indexes, division by zero and null pointers stop with a clear runtime error instead of undefined behavior.
  Operator precedence that does not bite (`x & 1 == 0` means what it says).
- **Practical.** Maps, methods, errors as values (`int(s) or 0`, `try read_file(p)`), modules, any C
  library, and a language server for your editor. Runs on Linux, macOS and Windows, on x86-64 and ARM64.
- **Errors that talk like a person:**
  ```
  game.fch:3:13: error: 'y' must be i32, but this is int (use i32(...) to convert)
      3 |     i32 y = x
        |             ^
  ```
- **Any C library, directly.** `import "raylib.h"` + `link "raylib"`, and you can call it, structs by value
  and callbacks included. Headers are read by libclang, so there is nothing to declare by hand.
- **Written in itself.** The compiler is Finch code, and nothing else: `src/*.fch`, about 8.6k lines. It is built from a
  seed (its own LLVM IR, `bootstrap/`) and then from itself, and the two come out identical. Its first version, in C++,
  is kept on the `before-bootstrap` branch.

## Quick start

**Building it** (Linux x86-64 or ARM64; needs `clang` and libclang, LLVM 21):

```sh
git clone https://github.com/Nexoniarz/Finch.git
cd Finch
nix-shell                           # or install clang + libclang (LLVM 21) + pkg-config yourself
bootstrap/build.sh                  # the compiler is build/finch: built from the seed, then from itself

./build/finch run examples/hello.fch
./build/finch build examples/tour.fch -o tour && ./tour
tests/run.sh                        # every program, every refusal, -g, a shared library, the language server
```

Ready builds of the compiler are in the [releases](https://github.com/Nexoniarz/Finch/releases) (Linux x64 and ARM64:
unpack, keep `runtime/` and `std/` next to `finch`). **Programs for Windows, ARM64 and macOS** are built from Linux with
`--target windows|arm64|macos` (see the [guide](docs/en/for-technicians.md#3-the-finch-command)); the compiler itself does not run on
macOS or Windows yet (it needs a seed made with that system's headers: see [bootstrap/README.md](bootstrap/README.md)).

Editors: **Kate** (`editors/kate/install.sh`), **VS Code** (the `finch-lang` `.vsix` from the release), or any
editor with LSP support (`finch lsp`).

## Documentation

Pick the guide that fits you:

| Guide | For | English | Polski |
|---|---|---|---|
| **Beginners** | Never programmed: students, seniors, anyone curious | [for-beginners.md](docs/en/for-beginners.md) | [dla-poczatkujacych.md](docs/pl/dla-poczatkujacych.md) |
| **Technicians** | Know computers, new to programming: install, whole language, C libraries, troubleshooting | [for-technicians.md](docs/en/for-technicians.md) | [dla-technikow.md](docs/pl/dla-technikow.md) |
| **Engineers** | Everything: lexer, grammar, AST, type rules, IR lowering, libclang import, ABI, internals | [for-engineers.md](docs/en/for-engineers.md) | [dla-inzynierow.md](docs/pl/dla-inzynierow.md) |
| **Cheat sheet** | Every function, method, operator and command on one page | [cheatsheet.md](docs/en/cheatsheet.md) | [sciaga.md](docs/pl/sciaga.md) |

## The language at a glance

| | |
|---|---|
| Variables | `x := 5` (inferred) or `int x = 5`; `const W = 640` |
| Types | `int` `float` `bool` `char` `str`, sized `i8`…`i64` `u8`…`u64` `f32` `f64`, arrays `[]T`, maps `map[K]V`, pointers `ptr[T]` |
| Functions | `fn add(int a, int b) -> int { return a + b }` |
| Structs | `struct Point { … }` with one field per line (`int x`, `int y = 0`); create with `Point(1, 2)` or `Point(x: 1)` |
| Methods | `fn Point.move(int dx) { self.x += dx }` → `p.move(3)` |
| Arrays | `nums := [1, 2, 3]`, `nums.push(4)`, `nums[0]`, `nums.len`, `for i, n in nums { }` |
| Maps | `ages := ["ann": 31]`, `ages["bob"] = 25`, `ages.has("cy")`, `counts[w] += 1`, `for k, v in ages { }` |
| Errors | `fn parse(str s) -> int! { … return error("bad") }` → `parse(s) or 0`, `parse(s) or { … }`, `try parse(s)` |
| Text | `"hp \(hp)"`, `"a" + "b"`, `str(42)`, `int("42")`, `s.split(",")`, `s.upper()`, `s[0]` |
| Control | `if` / `else if` / `else` (also as a value), `while`, `for i in 0..10`, `break`, `continue`, `defer` |
| Memory | automatic for arrays, text and structs; `new(...)` / `free(...)` for your own heap structures |
| Enums, match | `enum Color { Red, Green }`, `match c { Red -> ... else -> ... }` (also as a value) |
| Generics | `fn max[T](T a, T b) -> T`, `struct Stack[T]`, `Stack[int]()` |
| Modules | `import shapes` → `shapes.area(b)`, `import ui/button as btn`; the standard library: `json`, `path`, `time`, `proc`, `fs`, `thread` |
| Threads | `thread.spawn(addr(work), addr(data))`, `t.join()`, `thread.mutex()`, `atomic_add(addr(n), 1)` |
| Tests | `test "name" { assert(x == 3) }` next to the code, run with `finch test` |
| C interop | `import "stdio.h"`, `link "glfw"`, `link "mine.c"` / `"glue.cpp"`, unions, `#define`s (also `(Color){…}`), `addr(fn)` for callbacks |
| I/O | `print(...)`, `input("? ")`, `read_file`, `write_file`, `shell` |
| Tools | `finch run/build/test/fmt/ir/lsp`, `finch.toml` for a project, a build cache, `--target windows/arm64/macos/android`, `-g` for gdb/lldb, `-O0`, the chain of calls on runtime errors |

## Examples

| File | Shows |
|---|---|
| [`examples/hello.fch`](examples/hello.fch) | the smallest program |
| [`examples/tour.fch`](examples/tour.fch) | variables, functions, conditions, loops |
| [`examples/guess.fch`](examples/guess.fch) | a guessing game: input, loops, C's `rand` |
| [`examples/todo.fch`](examples/todo.fch) | structs, arrays, strings and files |
| [`examples/wordcount.fch`](examples/wordcount.fch) | maps, methods and errors as values: the most frequent words in a file |
| [`examples/structs.fch`](examples/structs.fch) | structs, arrays of structs, a linked list with `new`/`free` |
| [`examples/types.fch`](examples/types.fch) | sized numbers and pointers |
| [`examples/c_import.fch`](examples/c_import.fch) | `printf`, `math.h`, `malloc`/`free`, `stderr` |
| [`examples/window.fch`](examples/window.fch) | a GLFW + OpenGL window |
| [`examples/opengl.fch`](examples/opengl.fch) | modern OpenGL 3.3: shaders, buffers, a spinning triangle |
| [`examples/vulkan.fch`](examples/vulkan.fch) | Vulkan: list the GPUs, their memory and queues, create a device |
| [`examples/raylib.fch`](examples/raylib.fch) | raylib with C structs by value |
| [`examples/llvm.fch`](examples/llvm.fch) | Finch building LLVM IR through the LLVM-C API |
| [`src/`](src/) | the compiler, written in Finch (the seed to build it is in [`bootstrap/`](bootstrap/)) |

## Roadmap

- [x] Lexer, parser, LLVM codegen, O2 optimization
- [x] Types incl. sized integers, pointers, conversions; functions; `if`/`while`/`for`
- [x] Readable compile errors; runtime checks (division, null, bounds)
- [x] Bitwise operators with sane precedence
- [x] C headers through libclang; `link` with pkg-config; link-error advice
- [x] `struct`, including C structs by value (System V ABI) and callbacks
- [x] Arrays with `.len`, bounds checks and methods; `for x in list`
- [x] Memory: values freed at the end of their block, `defer`, `new`/`free`
- [x] Strings: `+`, `.len`, indexing, methods, `str(x)`, `int(s)`; `input()`; files
- [x] Finch modules (`import name`)
- [x] Debug info (`-g`)
- [x] Bootstrap: a Finch compiler written in Finch that builds itself
- [x] Windows: `finch.exe`, `.exe` programs, the Microsoft x64 ABI; cross-building from Linux (`--target windows`)
- [x] Language server (`finch lsp`) and a VS Code extension
- [x] macOS and ARM64 (Linux and Apple Silicon), with their C calling conventions; Kate support
- [x] Methods on structs, maps, errors as values (`!`, `or`, `try`)
- [x] Generics (`fn max[T]`, `struct Stack[T]`), `match`, enums
- [x] A standard library in Finch: `json`, `path`, `time`, `proc`, `fs`
- [x] `const`, `if` as a value, `"\(x)"` in text, `"""` text, C unions and `#define` expressions
- [x] `test` blocks and `finch test`, `finch fmt`, `finch.toml`, a build cache, the chain of calls on runtime errors
- [x] Threads (`import thread`) and atomics; C++ files; `--target android` (built, not yet tried on a device)

## Layout

```
src/        the compiler, in Finch: lexer, parser, AST, code generation (to LLVM IR as text), C import (libclang) and
            the C calling conventions, the language server, the driver
std/        the standard library, in Finch: json, path, time, proc, fs
runtime/    the small C runtime linked into every program
bootstrap/  build.sh and the seeds: the compiler's own LLVM IR, one per system
examples/   example programs
editors/    VS Code extension, Kate highlighting and LSP setup
tests/      run/ new/ (golden output), fail/ new/fail/ (expected errors), run.sh, cross.sh, shared.sh, lsp_test.py
docs/       en/ and pl/ guides for three audiences
```

## License

[Apache License 2.0](LICENSE). Copyright 2026 Nexoniarz.
