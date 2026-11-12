#!/usr/bin/env bash
# Builds every test for another system and runs it there through an emulator:
#   tests/cross.sh windows   x86_64-w64-mingw32, run with Wine
#   tests/cross.sh arm64     aarch64 Linux, run with qemu-aarch64
# e.g. nix-shell -p pkgsCross.mingwW64.buildPackages.gcc wine64 --run 'tests/cross.sh windows'
#      nix-shell -p pkgsCross.aarch64-multiplatform.buildPackages.gcc qemu \
#                --run 'FINCH_CC=aarch64-unknown-linux-gnu-gcc QEMU_LD_PREFIX=<the aarch64 glibc> tests/cross.sh arm64'
# (On a real Windows/ARM64/macOS machine, use tests/run.sh with that system's finch.)
set -u
cd "$(dirname "$0")"
target=${1:?usage: tests/cross.sh windows|arm64}
FINCH=${FINCH:-../build/finch}
export WINEDEBUG=-all
case $target in
    windows) runner=wine; ext=.exe
             wine cmd /c exit > /dev/null 2>&1 || true ;;      # the first run sets Wine up and says so: not in a test's output
    arm64)   runner=qemu-aarch64; ext= ;;
    *) echo "unknown target $target"; exit 2 ;;
esac
pass=0; failed=0
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

run_exe() {  # program, stdin -> output without Windows' \r
    $runner "$1" < "$2" 2>&1 | tr -d '\r'
}

for f in run/*.fch new/*.fch; do
    grep -q "^fn main" "$f" || continue
    case "$target-$f" in arm64-*.x86_64.fch|windows-*.arm64.fch) continue ;; esac     # inline asm for another processor
    input="${f%.fch}.in"
    [ -f "$input" ] || input=/dev/null
    exe="$tmp/$(basename "${f%.fch}")$ext"
    if ! "$FINCH" build "$f" --target "$target" -o "$exe" > "$tmp/err" 2>&1; then
        failed=$((failed + 1)); echo "FAIL (build) $f"; head -5 "$tmp/err"; continue
    fi
    got=$(run_exe "$exe" "$input")
    # (warnings come from the build, which is not part of what the program prints)
    want=$(tr -d '\r' < "${f%.fch}.out" | awk '/: warning: /{skip=1; next} skip && /^ +([0-9]+ +)?[|=]/{next} {skip=0; print}')
    if [ "$got" == "$want" ]; then
        pass=$((pass + 1))
    else
        failed=$((failed + 1)); echo "FAIL $f"; diff <(echo "$got") "${f%.fch}.out" | head -10
    fi
done

expects_met() {  # file, output -> true if all its "// expect:" lines are there
    local line
    while IFS= read -r line; do
        case "$line" in "// expect: "*) ;; *) break ;; esac
        expect=${line#// expect: }
        [[ "$2" == *"$expect"* ]] || return 1
    done < "$1"
    return 0
}

for f in fail/*.fch new/fail/*.fch; do
    exe="$tmp/$(basename "${f%.fch}")$ext"
    if "$FINCH" build "$f" --target "$target" -o "$exe" > "$tmp/out" 2>&1; then
        got=$(run_exe "$exe" /dev/null)
    else
        got=$(cat "$tmp/out")
    fi
    if expects_met "$f" "$got"; then
        pass=$((pass + 1))
    else
        failed=$((failed + 1)); echo "FAIL $f (wanted: $expect)"; echo "$got" | head -5
    fi
done
echo "$target ($runner): $pass passed, $failed failed"
[ $failed -eq 0 ]
