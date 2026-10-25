#!/bin/sh
# Makes bootstrap/windows-x86_64.ll from Linux, with a Linux-hosted llvm-mingw (MinGW headers) and the headers of an
# official LLVM for Windows (clang-c/Index.h):
#   bootstrap/winseed.sh <finch> <llvm-mingw dir> <LLVM-for-Windows dir>
set -eu
cd "$(dirname "$0")/.."
finch=$1; mingw=$2; llvm=$3
PATH="$mingw/bin:$PATH" FINCH_CC="$mingw/bin/x86_64-w64-mingw32-clang" "$finch" ir src/main.fch --target windows -I "$llvm/include" > bootstrap/windows-x86_64.ll
echo "wrote bootstrap/windows-x86_64.ll"
