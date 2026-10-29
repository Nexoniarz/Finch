#!/usr/bin/env bash
# Finch test suite.
#   tests/run/*.fch   must compile and print exactly what is in the matching .out file
#                    (files without `fn main` are modules or helpers and are skipped;
#                     a matching .in file is fed to the program as its input)
#   tests/fail/*.fch and tests/new/fail/*.fch   must fail; the first line is "// expect: <text that must appear in the error>"
#   the same programs built with -g -O0, and a library loaded by a C program (tests/shared.sh)
#   MEMCHECK=1 tests/run.sh   also runs every program under valgrind: no leaks, no bad memory access
set -u
cd "$(dirname "$0")"
FINCH=${FINCH:-../build/finch}
FINCH="$(cd "$(dirname "$FINCH")" && pwd)/$(basename "$FINCH")"       # tests/shared.sh runs from another folder
FINCH_ROOT="$(cd .. && pwd)"
export FINCH_RT=${FINCH_RT:-$PWD/../runtime/finch_rt.c}
pass=0; failed=0
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

for f in run/*.fch new/*.fch; do
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

for f in fail/*.fch new/fail/*.fch; do
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

# the commands around the compiler: finch test, the chain of calls, finch fmt, a finch.toml project
check_tool() {   # name, what it printed, the file with what it should print
    if [ "$2" == "$(tr -d '\r' < "$3")" ]; then
        pass=$((pass + 1))
    else
        failed=$((failed + 1))
        echo "FAIL $1"
        diff <(echo "$2") "$3" | head -20
    fi
}
check_tool "finch test" "$(cd tool && "$FINCH" test test_blocks.fch 2>&1 | tr -d '\r')" tool/test_blocks.out
check_tool "the chain of calls" "$(cd tool && "$FINCH" run trace.fch 2>&1 | tr -d '\r')" tool/trace.out
cp tool/fmt_in.fch "$tmp/fmt.fch"
"$FINCH" fmt "$tmp/fmt.fch"
check_tool "finch fmt" "$(cat "$tmp/fmt.fch")" tool/fmt_out.fch
check_tool "finch fmt (again)" "$("$FINCH" fmt --check tool/fmt_out.fch 2>&1)" /dev/null
check_tool "finch.toml" "$(cd tool/project && "$FINCH" run 2>&1 | tr -d '\r')" tool/project/expected.out

# the same programs with debug info (-g -O0): the metadata has to be valid for every kind of thing a program has
for f in new/enums.fch new/generics.fch new/std_json.fch run/basics.fch run/methods_basic.fch run/maps.fch run/errors.fch \
         run/defer.fch run/c_structs.fch run/memory.fch; do
    input="${f%.fch}.in"
    [ -f "$input" ] || input=/dev/null
    if "$FINCH" build "$f" -g -O0 -o "$tmp/progg" > "$tmp/err" 2>&1 && [ "$("$tmp/progg" < "$input" 2>&1)" == "$(cat "${f%.fch}.out")" ]; then
        pass=$((pass + 1))
    else
        failed=$((failed + 1))
        echo "FAIL (-g) $f"
        head -3 "$tmp/err"
    fi
done

# a library a C program loads (finch build --shared)
if FINCH="$FINCH" ./shared.sh > "$tmp/shared.out" 2>&1; then
    pass=$((pass + 1))
else
    failed=$((failed + 1))
    echo "FAIL shared.sh"
    head -5 "$tmp/shared.out"
fi

# the runtime and std carried inside the compiler are the files as they are now
"$FINCH_ROOT/bootstrap/pack.sh" "$FINCH" "$tmp/embedded.fch" 2>/dev/null
if cmp -s "$tmp/embedded.fch" "$FINCH_ROOT/src/embedded.fch"; then
    pass=$((pass + 1))
else
    failed=$((failed + 1))
    echo "FAIL src/embedded.fch is out of date (run bootstrap/build.sh)"
fi

# the language server, talked to the way an editor does
if command -v python3 > /dev/null 2>&1; then
    if FINCH="$FINCH" python3 lsp_test.py > "$tmp/lsp.out" 2>&1; then
        pass=$((pass + 1))
    else
        failed=$((failed + 1))
        echo "FAIL lsp_test.py"
        grep FAIL "$tmp/lsp.out" | head -5
    fi
fi

echo "$pass passed, $failed failed"
[ $failed -eq 0 ]
