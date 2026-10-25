# The seed

Finch's compiler is written in Finch. To build it you need something that already runs Finch: the **seed**, which is
the compiler's own LLVM IR, one file per system:

| File | System | Checked |
|---|---|---|
| `linux-x86_64.ll` | Linux x86-64 | built and tested here and in CI |
| `linux-aarch64.ll` | Linux ARM64 | made with the ARM64 cross compiler; CI builds and tests it on an ARM64 runner |

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

## macOS and Windows

There is no seed for them yet: it needs Apple's or Microsoft's headers, which can't be had on Linux. Until there is one,
programs for macOS and Windows are built from Linux with `--target macos|windows`, and the compiler's older C++ version
(branch `before-bootstrap`) is the way to get a compiler that runs there.
