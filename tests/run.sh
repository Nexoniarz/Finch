#!/usr/bin/env bash
# Finch test suite.
#   tests/run/*.fn   must compile and print exactly what is in the matching .out file
#   tests/fail/*.fn  must fail; the first line is "// expect: <text that must appear in the error>"
set -u
cd "$(dirname "$0")"
FINCH=${FINCH:-../build/finch}
pass=0; failed=0

for f in run/*.fn; do
    want="${f%.fn}.out"
    got=$("$FINCH" run "$f" 2>&1)
    if [ "$got" == "$(cat "$want")" ]; then
        pass=$((pass + 1))
    else
        failed=$((failed + 1))
        echo "FAIL $f"
        diff <(echo "$got") "$want" | head -20
    fi
done

for f in fail/*.fn; do
    expect=$(head -1 "$f" | sed -n 's|^// expect: ||p')
    got=$("$FINCH" run "$f" 2>&1)
    status=$?
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
