#!/usr/bin/env bash
# Wrapper for clang++ that automatically configures KLEE's libc++
# Usage: clang++ [clang++ options] file.cpp -o file.bc

set -euo pipefail

# KLEE_LIBCXX_PATH is set by the Nix derivation
if [ -z "${KLEE_LIBCXX_PATH:-}" ]; then
    echo "Error: KLEE_LIBCXX_PATH not set. This script must be run from the KLEE Nix environment." >&2
    exit 1
fi

# Automatically add libc++ include paths
exec clang++ \
    -nostdinc++ \
    -I"$KLEE_LIBCXX_PATH/include/c++/v1" \
    -I"$KLEE_LIBCXX_PATH/include/x86_64-unknown-linux-gnu/c++/v1" \
    "$@"
