#!/bin/sh
# Installs the compiler you built (bootstrap/build.sh) under a prefix:
#   bootstrap/install.sh               /usr/local   (needs sudo)
#   bootstrap/install.sh ~/.local
# finch goes to <prefix>/bin; the runtime and the standard library go to <prefix>/share/finch, where finch looks for them.
set -eu
cd "$(dirname "$0")/.."
prefix=${1:-/usr/local}
[ -x build/finch ] || { echo "install: run bootstrap/build.sh first" >&2; exit 1; }
mkdir -p "$prefix/bin" "$prefix/share/finch"
cp build/finch "$prefix/bin/finch"
rm -rf "$prefix/share/finch/runtime" "$prefix/share/finch/std"
cp -r runtime std "$prefix/share/finch/"
echo "installed: $prefix/bin/finch ($("$prefix/bin/finch" version))"
