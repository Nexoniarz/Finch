#!/usr/bin/env bash
# Finch test suite.
#   tests/run/*.fch   must compile and print exactly what is in the matching .out file
#                    (files without `fn main` are modules or helpers and are skipped;
#                     a matching .in file is fed to the program as its input)
#   tests/fail/*.fch  must fail; the first line is "// expect: <text that must appear in the error>"
#   MEMCHECK=1 tests/run.sh   also runs every program under valgrind: no leaks, no bad memory access
set -u
cd "$(dirname "$0")"
FINCH=${FINCH:-../build/finch}
pass=0; failed=0
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

for f in run/*.fch; do
    grep -q "^fn main" "$f" || continue
    want="${f%.fch}.out"
    input="${f%.fch}.in"
    [ -f "$input" ] || input=/dev/null
    got=$("$FINCH" run "$f" < "$input" 2>&1 | tr -d '\r')   # Windows ends lines with \r\n
    if [ "$got" != "$(tr -d '\r' < "$want")" ]; then
        failed=$((failed + 1))
        echo "FAIL $f"
        diff <(echo "$got") "$want" | head -20
        continue
    fi
    if [ "${MEMCHECK:-0}" = 1 ]; then
        exe="$tmp/$(basename "${f%.fch}")"
        "$FINCH" build "$f" -o "$exe" >/dev/null 2>&1
        valgrind -q --leak-check=full --errors-for-leak-kinds=all --error-exitcode=99 "$exe" < "$input" >/dev/null 2>"$tmp/vg"
        if [ $? -eq 99 ]; then  # (other exit codes are the program's own)
            failed=$((failed + 1))
            echo "MEMCHECK FAIL $f"
            head -20 "$tmp/vg"
            continue
        fi
    fi
    pass=$((pass + 1))
done

for f in fail/*.fch; do
    expect=$(head -1 "$f" | sed -n 's|^// expect: ||p')
    got=$("$FINCH" run "$f" < /dev/null 2>&1)
    status=$?
    got=$(echo "$got" | tr -d '\r')
    if [ $status -ne 0 ] && [[ "$got" == *"$expect"* ]]; then
        pass=$((pass + 1))
    else
        failed=$((failed + 1))
        echo "FAIL $f (wanted: $expect)"
        echo "$got" | head -5
    fi
done

echo "$pass passed, $failed failed"
[ $failed -eq 0 ]
