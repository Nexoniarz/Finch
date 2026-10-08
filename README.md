# Finch 🐦

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
- **Errors that talk like a person:**
  ```
  game.fn:3:13: error: 'y' must be i32, but this is int (use i32(...) to convert)
      3 |     i32 y = x
        |             ^
  ```
- **Any C library, directly.** `import "raylib.h"` + `link "raylib"`, and you can call it, structs by value
  and callbacks included. Headers are read by libclang, so there is nothing to declare by hand.
- **Written in itself, too.** `boot/` is a Finch compiler written in Finch that compiles itself to a
  byte-identical result.

## Quick start

Linux x86-64, LLVM 21.

```sh
git clone https://github.com/Nexoniarz/Finch.git
cd Finch
nix-shell                           # or install LLVM 21 + libclang + cmake + ninja yourself
cmake -S . -B build -G Ninja
ninja -C build

./build/finch run examples/hello.fn
./build/finch build examples/tour.fn -o tour && ./tour
tests/run.sh                        # 58 passed, 0 failed
tests/boot.sh                       # the compiler written in Finch builds itself
```

## Documentation

Pick the guide that fits you:

| Guide | For | English | Polski |
|---|---|---|---|
| **Beginners** | Never programmed: students, seniors, anyone curious | [for-beginners.md](docs/en/for-beginners.md) | [dla-poczatkujacych.md](docs/pl/dla-poczatkujacych.md) |
| **Technicians** | Know computers, new to programming: install, whole language, C libraries, troubleshooting | [for-technicians.md](docs/en/for-technicians.md) | [dla-technikow.md](docs/pl/dla-technikow.md) |
| **Engineers** | Everything: lexer, grammar, AST, type rules, IR lowering, libclang import, ABI, internals | [for-engineers.md](docs/en/for-engineers.md) | [dla-inzynierow.md](docs/pl/dla-inzynierow.md) |

## The language at a glance

| | |
|---|---|
| Variables | `x := 5` (inferred) or `int x = 5` |
| Types | `int` `float` `bool` `char` `str`, sized `i8`…`i64` `u8`…`u64` `f32` `f64`, arrays `[]T`, pointers `ptr[T]` |
| Functions | `fn add(int a, int b) -> int { return a + b }` |
| Structs | `struct Point { … }` with one field per line (`int x`, `int y = 0`); create with `Point(1, 2)` or `Point(x: 1)` |
| Arrays | `nums := [1, 2, 3]`, `nums.push(4)`, `nums[0]`, `nums.len`, `for n in nums { }` |
| Text | `"a" + "b"`, `str(42)`, `int("42")`, `s.split(",")`, `s.upper()`, `s[0]` |
| Control | `if` / `else if` / `else`, `while`, `for i in 0..10`, `break`, `continue`, `defer` |
| Memory | automatic for arrays, text and structs; `new(...)` / `free(...)` for your own heap structures |
| Modules | `import shapes` → `shapes.area(b)` |
| C interop | `import "stdio.h"`, `link "glfw"`, `link "mine.c"`, `addr(fn)` for callbacks |
| I/O | `print(...)`, `input("? ")`, `read_file`, `write_file`, `shell` |
| Tools | `finch run/build/ir`, `-g` for gdb, `-O0` |

## Examples

| File | Shows |
|---|---|
| [`examples/hello.fn`](examples/hello.fn) | the smallest program |
| [`examples/tour.fn`](examples/tour.fn) | variables, functions, conditions, loops |
| [`examples/guess.fn`](examples/guess.fn) | a guessing game: input, loops, C's `rand` |
| [`examples/todo.fn`](examples/todo.fn) | structs, arrays, strings and files |
| [`examples/structs.fn`](examples/structs.fn) | structs, arrays of structs, a linked list with `new`/`free` |
| [`examples/types.fn`](examples/types.fn) | sized numbers and pointers |
| [`examples/c_import.fn`](examples/c_import.fn) | `printf`, `math.h`, `malloc`/`free`, `stderr` |
| [`examples/window.fn`](examples/window.fn) | a GLFW + OpenGL window |
| [`examples/raylib.fn`](examples/raylib.fn) | raylib with C structs by value |
| [`examples/llvm.fn`](examples/llvm.fn) | Finch building LLVM IR through the LLVM-C API |
| [`boot/`](boot/) | the Finch compiler, written in Finch |

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
- [ ] Methods on structs, maps, `match`
- [ ] Error values instead of stopping the program
- [ ] More platforms (ARM64, macOS)

## Layout

```
src/        compiler: lexer, parser, AST, codegen, C import + ABI, driver (C++17, ~4.5k lines)
runtime/    the small C runtime linked into every program
boot/       the Finch compiler written in Finch (~3k lines)
examples/   example programs
tests/      run/ (golden output), fail/ (expected errors), run.sh, boot.sh
docs/       en/ and pl/ guides for three audiences
```

## License

[Apache License 2.0](LICENSE). Copyright 2026 Nexoniarz.
