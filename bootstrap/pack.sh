#!/bin/sh
# Writes src/embedded.fch: the runtime and the standard library as text, so the compiler can carry them inside itself.
#   bootstrap/pack.sh <a Finch compiler> [out.fch]
set -eu
cd "$(dirname "$0")/.."
FINCH_RT=$PWD/runtime/finch_rt.c "$1" run tools/pack.fch "${2:-src/embedded.fch}" runtime/finch_rt.c std/*.fch std/*.h
