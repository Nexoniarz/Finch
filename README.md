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
- **Fast.** It compiles through LLVM's `-O2` pipeline to native code, with no runtime or garbage collector.
  Recursive `fib(40)` runs in about 0.19 s (`clang -O2` on the same C: about 0.23 s).
- **Safer than C, where it is cheap.** No silent narrowing and no signed/unsigned mixing. Division by zero
  and null `.value` stop with a clear runtime error instead of undefined behavior. Operator precedence
  that does not bite (`x & 1 == 0` means what it says).
- **Errors that talk like a person:**
  ```
  game.fn:3:13: error: 'y' must be i32, but this is int (use i32(...) to convert)
      3 |     i32 y = x
        |             ^
  ```
- **Any C library, directly.** `import "GLFW/glfw3.h"` + `link "glfw"`, and you can call it.
  Headers are read by libclang, so there is nothing to declare by hand.

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
tests/run.sh                        # 34 passed, 0 failed
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
| Types | `int` `float` `bool` `char` `str`, sized `i8`…`i64` `u8`…`u64` `f32` `f64`, pointers `ptr[T]` / `ptr` |
| Functions | `fn add(int a, int b) -> int { return a + b }` |
| Control | `if` / `else if` / `else`, `while`, `for i in 0..10`, `break`, `continue` |
| Pointers | `p := addr(x)`, `p.value = 5`, `null` |
| Conversions | `int(3.9)`, `u8(x)`, `float(n)` (implicit only when nothing is lost) |
| Operators | `+ - * / %`, `& \| ^ << >> ~`, `== != < <= > >=`, `&& \|\| !` |
| C interop | `import "stdio.h"`, `link "glfw"` |
| Output | `print(a, b, c)` prints anything, separated by spaces |

## Examples

| File | Shows |
|---|---|
| [`examples/hello.fn`](examples/hello.fn) | the smallest program |
| [`examples/tour.fn`](examples/tour.fn) | variables, functions, conditions, loops |
| [`examples/types.fn`](examples/types.fn) | sized numbers and pointers |
| [`examples/c_import.fn`](examples/c_import.fn) | `printf`, `math.h`, `malloc`/`free`, `stderr` |
| [`examples/window.fn`](examples/window.fn) | a GLFW + OpenGL window |
| [`examples/llvm.fn`](examples/llvm.fn) | Finch building LLVM IR through the LLVM-C API |

## Roadmap

- [x] Lexer, parser, LLVM codegen, O2 optimization
- [x] Types incl. sized integers, pointers, conversions; functions; `if`/`while`/`for`
- [x] Readable compile errors; runtime checks (division, null)
- [x] Bitwise operators with sane precedence
- [x] C headers through libclang; `link` with pkg-config; link-error advice
- [ ] `struct` (and C structs by value)
- [ ] Arrays/slices with `.len` and bounds checks
- [ ] Memory: automatic free at the end of the owning block, `defer`, manual `free` when wanted
- [ ] Strings: `+`, `len`, `str(x)`; `input()`
- [ ] Finch modules (`import math`)
- [ ] Debug info (DWARF)
- [ ] Bootstrap: the Finch compiler written in Finch

## Layout

```
src/        compiler: lexer, parser, AST, codegen, C import, driver (C++17, ~2.4k lines)
examples/   example programs
tests/      run/ (golden output), fail/ (expected errors), run.sh
docs/       en/ and pl/ guides for three audiences
```

## License

[Apache License 2.0](LICENSE). Copyright 2026 Nexoniarz.
