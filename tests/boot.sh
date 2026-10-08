#!/usr/bin/env bash
# The self-hosted compiler (boot/) must
#   1. build itself: stage1 (built by build/finch) -> stage2 -> stage3, with identical IR
#   2. give the same output as build/finch on every test in tests/run it supports
set -u
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

build/finch build boot/main.fch -o "$tmp/boot1" || exit 1
"$tmp/boot1" build boot/main.fch -o "$tmp/boot2" || exit 1
"$tmp/boot1" ir boot/main.fch > "$tmp/stage2.ll"
"$tmp/boot2" ir boot/main.fch > "$tmp/stage3.ll"
if cmp -s "$tmp/stage2.ll" "$tmp/stage3.ll"; then
    echo "bootstrap: stage 2 and stage 3 are identical ($(wc -l < "$tmp/stage3.ll") lines of IR)"
else
    echo "bootstrap: FAILED, stage 2 and stage 3 differ"
    exit 1
fi

pass=0; failed=0; skipped=0
for f in tests/run/*.fch; do
    grep -q "^fn main" "$f" || continue
    # the bootstrap compiler doesn't do C imports, sized numbers or defer
    # (checked in the test and in the modules it imports)
    files="$f"
    for m in $(sed -n 's/^import \([a-z_]*\)$/\1/p' "$f"); do files="$files tests/run/$m.fch"; done
    if grep -qE '^import "|^link |\b(u8|u16|u32|u64|i8|i16|i32|f32|f64)\b|defer |0x' $files; then
        skipped=$((skipped + 1))
        continue
    fi
    input="${f%.fch}.in"
    [ -f "$input" ] || input=/dev/null
    if ! "$tmp/boot2" build "$f" -o "$tmp/prog" > "$tmp/err" 2>&1; then
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
echo "self-hosted compiler: $pass passed, $failed failed, $skipped skipped (features it doesn't have)"
[ $failed -eq 0 ]
