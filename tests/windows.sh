#!/usr/bin/env bash
# Cross-compiles every test for Windows (x86_64-w64-mingw32) and runs it under Wine.
# Needs x86_64-w64-mingw32-gcc and wine, e.g.:
#   nix-shell -p pkgsCross.mingwW64.buildPackages.gcc wine64 --run tests/windows.sh
# (On a real Windows machine, use tests/run.sh with finch.exe instead.)
set -u
cd "$(dirname "$0")"
FINCH=${FINCH:-../build/finch}
export WINEDEBUG=-all
pass=0; failed=0
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

run_exe() {  # exe, stdin -> output without Windows' \r
    wine "$1" < "$2" 2>&1 | tr -d '\r'
}

for f in run/*.fch; do
    grep -q "^fn main" "$f" || continue
    input="${f%.fch}.in"
    [ -f "$input" ] || input=/dev/null
    exe="$tmp/$(basename "${f%.fch}").exe"
    if ! "$FINCH" build "$f" --target windows -o "$exe" > "$tmp/err" 2>&1; then
        failed=$((failed + 1)); echo "FAIL (build) $f"; head -5 "$tmp/err"; continue
    fi
    got=$(run_exe "$exe" "$input")
    if [ "$got" == "$(cat "${f%.fch}.out")" ]; then
        pass=$((pass + 1))
    else
        failed=$((failed + 1)); echo "FAIL $f"; diff <(echo "$got") "${f%.fch}.out" | head -10
    fi
done

for f in fail/*.fch; do
    expect=$(head -1 "$f" | sed -n 's|^// expect: ||p')
    exe="$tmp/$(basename "${f%.fch}").exe"
    if got=$("$FINCH" build "$f" --target windows -o "$exe" 2>&1); then
        got=$(run_exe "$exe" /dev/null); status=$?
        status=$( [ -n "$got" ] && echo 1 || echo 0 )
    else
        status=1
    fi
    if [[ "$got" == *"$expect"* ]]; then
        pass=$((pass + 1))
    else
        failed=$((failed + 1)); echo "FAIL $f (wanted: $expect)"; echo "$got" | head -5
    fi
done
echo "windows (wine): $pass passed, $failed failed"
[ $failed -eq 0 ]
