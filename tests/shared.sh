#!/bin/sh
# finch build --shared: a library a C program loads, calls by name, and that hands runtime
# errors to the host instead of exiting.
set -e
cd "$(dirname "$0")/shared"
FINCH=${FINCH:-../../build/finch}
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
"$FINCH" build mod.fch --shared -o "$tmp/libmod.so"
${CC:-cc} host.c -rdynamic -o "$tmp/host" -ldl
out=$("$tmp/host" "$tmp/libmod.so")
expected='add 42
even 1 0
host_print: hello Rakya
caught: mod.fch:17: runtime error: index 7 is out of range (the length is 3)
still alive'
if [ "$out" = "$expected" ]; then echo "shared library: passed"; else printf 'shared library: FAILED\n%s\n' "$out"; exit 1; fi
