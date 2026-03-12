# Nix Package Definitions

This directory contains Nix package definitions for building KLEE and its dependencies.

## Files

- `klee.nix`: Main KLEE package definition
- `klee-uclibc.nix`: Modified uClibc for KLEE's runtime

## Usage

From the repository root:

```bash
# Build KLEE
nix build

# Enter development shell with all dependencies
nix develop

# Run KLEE directly
nix run . -- --help
```

## Building from Source

The flake uses the repository source directly (not fetching from GitHub), so any local changes are immediately reflected in the build.

## Options

You can customize the build with package options:

```bash
# Build with debug enabled
nix build --impure --expr '(builtins.getFlake (toString ./.)).packages.${builtins.currentSystem}.klee.override { debug = true; }'

# Build with assertions
nix build --impure --expr '(builtins.getFlake (toString ./.)).packages.${builtins.currentSystem}.klee.override { asserts = true; }'
```
