#!/bin/sh
# Builds the Finch compiler without a compiler for Finch: from the seed (the compiler's own LLVM IR),
# then from itself, and checks that the two come out the same.
#
#   bootstrap/build.sh            build/finch
#   bootstrap/build.sh seed       also write bootstrap/<system>.ll anew (do this when the compiler got a feature
#                                 its own source uses, or the seed is old: the next build starts from it)
#
# Needs clang and libclang (the C headers are read through it). The compiler is then in build/finch. It carries the
# runtime and the standard library inside itself (src/embedded.fch), so it runs from anywhere.
set -eu
cd "$(dirname "$0")/.."
mkdir -p build

case "$(uname -s)-$(uname -m)" in
    Linux-x86_64)                 seed=linux-x86_64 ;;
    Linux-aarch64|Linux-arm64)    seed=linux-aarch64 ;;
    Darwin-arm64)                 seed=macos-arm64 ;;
    Darwin-x86_64)                seed=macos-x86_64 ;;
    MINGW*|MSYS*|CYGWIN*)         seed=windows-x86_64 ;;
    *) echo "bootstrap: no seed for $(uname -s) $(uname -m) (see bootstrap/README.md)" >&2; exit 1 ;;
esac
[ -f "bootstrap/$seed.ll" ] || { echo "bootstrap: bootstrap/$seed.ll is missing" >&2; exit 1; }

CLANG=${FINCH_CLANG:-clang}
LIBS=${FINCH_LIBCLANG:--lclang}
export FINCH_RT=$PWD/runtime/finch_rt.c

echo "bootstrap: seed ($seed) -> build/finch0"
$CLANG -O1 -w "bootstrap/$seed.ll" runtime/finch_rt.c -o build/finch0 -lm $LIBS
echo "bootstrap: build/finch0 packs the runtime and std into src/embedded.fch"
bootstrap/pack.sh build/finch0
echo "bootstrap: build/finch0 builds the compiler -> build/finch1"
build/finch0 build src/main.fch -o build/finch1
echo "bootstrap: build/finch1 builds the compiler -> build/finch"
build/finch1 build src/main.fch -o build/finch

# a compiler that builds itself must come out the same however many times it does
build/finch1 ir src/main.fch > build/stage1.ll
build/finch ir src/main.fch > build/stage2.ll
if cmp -s build/stage1.ll build/stage2.ll; then
    echo "bootstrap: build/finch1 and build/finch make the same IR ($(wc -l < build/stage2.ll) lines): done"
else
    echo "bootstrap: the compiler does not reproduce itself (build/stage1.ll and build/stage2.ll differ)" >&2
    exit 1
fi

if [ "${1:-}" = seed ]; then
    cp build/stage2.ll "bootstrap/$seed.ll"
    echo "bootstrap: wrote bootstrap/$seed.ll"
fi
