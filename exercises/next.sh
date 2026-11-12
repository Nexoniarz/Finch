#!/usr/bin/env bash
# Finds the first exercise that isn't solved yet and shows what its tests say.
#   exercises/next.sh            the next one
#   exercises/next.sh all        how far you are
set -u
cd "$(dirname "$0")"
FINCH=${FINCH:-}
if [ -z "$FINCH" ]; then
    if [ -x ../build/finch ]; then FINCH=../build/finch; else FINCH=finch; fi
fi
case "$(uname -m)" in aarch64|arm64) arch=arm64 ;; *) arch=x86_64 ;; esac
done=0; total=0
for f in [0-9]*.fch; do
    case "$f" in *.x86_64.fch) [ $arch = x86_64 ] || continue ;; *.arm64.fch) [ $arch = arm64 ] || continue ;; esac
    total=$((total + 1))
    out=$("$FINCH" test "$f" 2>&1)
    if echo "$out" | grep -q " 0 failed"; then
        done=$((done + 1))
        continue
    fi
    if [ "${1:-}" = all ]; then
        continue
    fi
    echo "------------------------------------------------------------"
    echo " Next: exercises/$f    ($done solved so far)"
    echo "------------------------------------------------------------"
    echo "$out"
    echo
    echo "Open exercises/$f, do its TODOs, then run exercises/next.sh again."
    echo "Stuck? exercises/solutions/$f has one answer (try first!)."
    exit 1
done
if [ $done -eq $total ]; then
    echo "All $total exercises solved. The finch is flying."
else
    echo "$done of $total exercises solved."
fi
