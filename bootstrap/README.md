# The seed

Finch's compiler is written in Finch. To build it you need something that already runs Finch: the **seed**, which is
the compiler's own LLVM IR, one file per system:

| File | System | Checked |
|---|---|---|
| `linux-x86_64.ll` | Linux x86-64 | built and tested here and in CI |
| `linux-aarch64.ll` | Linux ARM64 | made with the ARM64 cross compiler; CI builds and tests it on an ARM64 runner |
| `windows-x86_64.ll` | Windows x86-64 (MinGW: llvm-mingw's clang + libclang.dll) | built and run under Wine: it compiles itself, and every test program runs |
| `macos-arm64.ll`, `macos-x86_64.ll` | macOS | made on Linux with the macOS 11.3 SDK; checked as IR only (nobody has run them yet) |

`bootstrap/build.sh` compiles the seed with `clang` (and the runtime, and libclang) into `build/finch0`; `finch0` builds
`src/` into `finch1`; `finch1` builds `src/` again into `build/finch`; the two must make the same IR, or the script stops.

## Why one per system

The seed includes how this system's C compiler passes structs by value, because the compiler calls libclang with
`CXCursor`, `CXType` and `CXString` (the calling convention is different on x86-64, on ARM64 and on Windows). So a seed
made for one system doesn't run on another, and a seed needs the target system's C headers to be made.

## When to make a new one

When the compiler's own source starts to use a feature that the committed seed doesn't have, `build.sh` fails at the first
step. Make the seed from a working compiler:

```sh
bootstrap/build.sh seed            # writes bootstrap/<this system>.ll
```

For another system, from a machine that has that system's headers and a cross compiler (what `tests/cross.sh` uses):

```sh
FINCH_CC=aarch64-linux-gnu-gcc build/finch ir src/main.fch --target arm64 > bootstrap/linux-aarch64.ll
```

## Windows and macOS

Windows: `windows-x86_64.ll` is made by `bootstrap/winseed.sh` on Linux (llvm-mingw's MinGW headers and the headers of an
official LLVM for Windows); `bootstrap/winbuild.sh` turns it into `finch.exe` for trying under Wine. On a Windows machine
`bootstrap/build.sh` does the same from Git Bash: put llvm-mingw's `bin` in PATH and set `FINCH_LIBCLANG` (the path of
`libclang.dll`), `FINCH_LLVM_INCLUDE` (its `include` folder) and `FINCH_LDFLAGS=-L<folder of libclang.dll>`.

macOS: the seeds were made with the macOS 11.3 SDK; `brew install llvm@21` gives clang and libclang; the CI job sets
`FINCH_CLANG`, `FINCH_LIBCLANG` and `FINCH_LLVM_INCLUDE`. If the macOS build fails, that job is where it shows first.
