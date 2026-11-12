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

# a test named x.x86_64.fch or x.arm64.fch is for that processor only (inline asm)
case "$(uname -m)" in aarch64|arm64) arch=arm64 ;; *) arch=x86_64 ;; esac
other_arch() {  # file -> true if it is for another processor
    case "$1" in *.x86_64.fch) [ $arch != x86_64 ] ;; *.arm64.fch) [ $arch != arm64 ] ;; *) false ;; esac
}

for f in run/*.fch new/*.fch; do
    grep -q "^fn main" "$f" || continue
    other_arch "$f" && continue
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

# every "// expect: ..." line at the top of the file must be in what the compiler said (the error, its help: line, ...)
expects_met() {  # file, output -> true if all its expect lines are there; $expect is the first one missing
    local line
    while IFS= read -r line; do
        case "$line" in "// expect: "*) ;; *) break ;; esac
        expect=${line#// expect: }
        [[ "$2" == *"$expect"* ]] || return 1
    done < "$1"
    return 0
}

for f in fail/*.fch new/fail/*.fch; do
    got=$("$FINCH" run "$f" < /dev/null 2>&1)
    status=$?
    got=$(echo "$got" | tr -d '\r')
    if [ $status -ne 0 ] && expects_met "$f" "$got"; then
        pass=$((pass + 1))
    else
        failed=$((failed + 1))
        echo "FAIL $f (wanted: $expect)"
        echo "$got" | head -5
    fi
done

# the exercises' solutions must keep passing (and the exercises themselves must keep compiling)
for f in ../exercises/solutions/[0-9]*.fch ../exercises/[0-9]*.fch; do
    other_arch "$f" && continue
    got=$("$FINCH" test "$f" 2>&1 | tr -d '\r')
    if [[ "$f" == */solutions/* ]]; then
        ok=0; [[ "$got" == *" 0 failed"* ]] && ok=1
    else
        ok=1; echo "$got" | grep -qE '^[^ ]+:[0-9]+:[0-9]+: error:' && ok=0
    fi
    if [ $ok = 1 ]; then
        pass=$((pass + 1))
    else
        failed=$((failed + 1))
        echo "FAIL exercise $f"
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
    if "$FINCH" build "$f" -g -O0 -o "$tmp/progg" > "$tmp/err" 2>&1 && [ "$("$tmp/progg" < "$input" 2>&1 | tr -d '\r')" == "$(tr -d '\r' < "${f%.fch}.out")" ]; then
        pass=$((pass + 1))
    else
        failed=$((failed + 1))
        echo "FAIL (-g) $f"
        head -3 "$tmp/err"
    fi
done

# a library a C program loads (finch build --shared); the test's host uses dlopen and -rdynamic, so not on Windows
# (there a DLL can't call functions of the program that loads it without an import library)
case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) skip_shared=1 ;; *) skip_shared=0 ;; esac
if [ $skip_shared = 1 ]; then
    :
elif FINCH="$FINCH" ./shared.sh > "$tmp/shared.out" 2>&1; then
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
