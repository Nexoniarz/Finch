#!/bin/sh
# Builds build/win/finch.exe from bootstrap/windows-x86_64.ll on Linux (to try the Windows compiler under Wine):
#   bootstrap/winbuild.sh <llvm-mingw dir> <LLVM-for-Windows dir>
set -eu
cd "$(dirname "$0")/.."
mkdir -p build/win
"$1/bin/x86_64-w64-mingw32-clang" -O1 -w bootstrap/windows-x86_64.ll runtime/finch_rt.c -o build/win/finch.exe "$2/bin/libclang.dll"
cp "$2/bin/libclang.dll" build/win/
