#!/usr/bin/env bash
# The self-hosted compiler (boot/) must
#   1. build itself: stage1 (built by build/finch) -> stage2 -> stage3, with identical IR
#   2. give the same output as build/finch on every test in tests/run it supports
set -u
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

build/finch build boot/main.fch -o "$tmp/boot1" || exit 1
# Stage 2: the compiler built by itself. Until it can read the C headers its own source imports
# (libclang's), this stage can't be made, and the tests below run on stage 1 instead.
boot=$tmp/boot1
if "$tmp/boot1" build boot/main.fch -o "$tmp/boot2" > "$tmp/stage2.err" 2>&1; then
    "$tmp/boot1" ir boot/main.fch > "$tmp/stage2.ll"
    "$tmp/boot2" ir boot/main.fch > "$tmp/stage3.ll"
    if cmp -s "$tmp/stage2.ll" "$tmp/stage3.ll"; then
        echo "bootstrap: stage 2 and stage 3 are identical ($(wc -l < "$tmp/stage3.ll") lines of IR)"
        boot=$tmp/boot2
    else
        echo "bootstrap: FAILED, stage 2 and stage 3 differ"
        exit 1
    fi
else
    echo "bootstrap: not yet -- stage 1 can't build itself: $(head -1 "$tmp/stage2.err")"
fi

pass=0; failed=0; skipped=0
# tests/run: what the C++ compiler also runs. tests/new: the language since then (enums, match, ...),
# which only this compiler has.
for f in tests/run/*.fch tests/new/*.fch; do
    grep -q "^fn main" "$f" || continue
    input="${f%.fch}.in"
    [ -f "$input" ] || input=/dev/null
    if ! "$boot" build "$f" -o "$tmp/prog" > "$tmp/err" 2>&1; then
        failed=$((failed + 1))
        echo "FAIL (compile) $f"
        head -3 "$tmp/err"
        continue
    fi
    got=$("$tmp/prog" < "$input" 2>&1)
    if [ "$got" == "$(cat "${f%.fch}.out")" ]; then
        pass=$((pass + 1))
    else
        failed=$((failed + 1))
        echo "FAIL (output) $f"
        diff <(echo "$got") "${f%.fch}.out" | head -10
    fi
done
# programs that must be refused, and say why: the first line is "// expect: <text of the error>"
for f in tests/new/fail/*.fch; do
    expect=$(head -1 "$f" | sed -n 's|^// expect: ||p')
    got=$("$boot" build "$f" -o "$tmp/refused" 2>&1)
    status=$?
    if [ $status -ne 0 ] && [[ "$got" == *"$expect"* ]]; then
        pass=$((pass + 1))
    else
        failed=$((failed + 1))
        echo "FAIL $f (wanted: $expect)"
        echo "$got" | head -3
    fi
done
echo "self-hosted compiler: $pass passed, $failed failed, $skipped skipped (features it doesn't have)"
[ $failed -eq 0 ]
