# Nix Package Definitions

This directory contains Nix package definitions for building KLEE and its dependencies.

## Files

- `klee.nix`: Main KLEE package definition
- `klee-uclibc.nix`: Modified uClibc for KLEE's runtime
- `klee-libcxx.nix`: libc++ built to bitcode for symbolic execution of C++
- `klee-wrappers.nix`: `clang++` wrapper preconfigured with KLEE's libc++ paths
- `container.nix`: OCI images sharing a single KLEE base layer

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

## Container Images

`nix/container.nix` builds OCI images with [nix2container](https://github.com/nlewo/nix2container). It exists so that a large set of benchmark images — each needing its own build dependencies — can share KLEE's closure instead of duplicating it per image.

### Prebuilt images

```bash
# KLEE plus a shell, the bitcode toolchain, and the analysis tools
nix run .#klee-image.copyToPodman
nix run .#klee-image.copyToDockerDaemon

# The same layers plus gnumake, pkg-config, and openssl
nix run .#klee-image-example.copyToPodman
```

### Per-benchmark images

`lib.${system}.mkKleeImage` builds an image on the shared layers. It takes `packages`, an optional `config` merged over the defaults, and forwards every other argument to `nix2container.buildImage` — so `name`, `tag`, `copyToRoot`, `meta` and the rest work as documented there. Passing `layers` is rejected, since the shared layers are what makes the sharing work.

```nix
{
  inputs.klee.url = "github:cirrus-uchicago/klee/feat/symbolon";

  outputs = {self, nixpkgs, klee}: let
    system = "x86_64-linux";
    pkgs = nixpkgs.legacyPackages.${system};
  in {
    packages.${system} = {
      bench-libxml2 = klee.lib.${system}.mkKleeImage {
        name = "klee-bench-libxml2";
        packages = with pkgs; [gnumake libxml2.dev zlib.dev];
      };

      bench-sqlite = klee.lib.${system}.mkKleeImage {
        name = "klee-bench-sqlite";
        packages = with pkgs; [gnumake sqlite.dev];
      };
    };
  };
}
```

A benchmark packaged as its own derivation can be passed in `packages`, which puts it on `PATH`; one that should be present without being on `PATH` goes in `copyToRoot` instead.

Consumers who take `overlays.default` rather than the flake outputs reach the same builders as `pkgs.klee-containers`, alongside `pkgs.klee` and `pkgs.klee-wrappers`.

Note that `config` replaces keys rather than merging them, so supplying `Env` drops the default `PATH` entirely.

### Layer structure

Images are built from three layers, ordered so the expensive parts change least often:

| Output | Contents | Changes when |
| --- | --- | --- |
| `packages.klee-deps-layer` | LLVM 16, STP, Z3, the PyTorch environment, clang, `gllvm`, bash, coreutils, and the root filesystem skeleton — about 4 GiB across 25 layers | `flake.lock` moves |
| `packages.klee-layer` | KLEE and `klee-wrappers` only — about 13 MiB | KLEE's source changes |
| per-image layer | Whatever `packages` adds, one store path per layer | That benchmark's dependencies change |

The deps layer derives from `klee.buildInputs`, which does not depend on KLEE's source, so iterating on KLEE re-copies only the small layer. Both are exposed as packages so a cache can be warmed before any image is built.

### How the sharing works

Every image passes the same layer derivations in nix2container's `layers` argument. nix2container skips any store path already belonging to a listed layer, so those layers are emitted once and every image references identical blob digests. Adding a benchmark costs only the store paths it introduces.

Some details are worth knowing:

- Ancestor layers must be listed **explicitly and flat**, as `mkKleeImage` does. nix2container tracks nesting for the image's own layer list but forwards only the top-level list to the internal customization layer. Because `config.Env` embeds store paths in `PATH`, that customization layer inherits the entire closure as references, and relying on nesting alone makes it re-emit everything the lower layers already carry.
- The per-image layer sets `maxLayers`, giving each new store path its own blob. Left at the default of 1, two benchmarks that share a dependency would each get a private copy of it inside an otherwise-unique blob.
- Deduplication happens in the container runtime's blob store, so it applies to Podman, Docker, and OCI registries. Apptainer and Singularity flatten an image into a single SIF file and therefore do not benefit.
- `maxLayers` alone does not guarantee sharing. It splits a closure by store path popularity, computed per image, so the same package can land in differently-digested layers across images. The explicit layer pinning is what makes the digests match.

Both properties are guarded by the `container-layer-sharing` check, which fails if an image emits a store path in more than one layer, or if the example image stops reusing every layer of the plain one.

### Image contents

The root filesystem comes from `dockerTools.binSh` and `dockerTools.fakeNss`, plus a `1777` `/tmp`. `klee-stats`, `ktest-tool`, and `klee-replay` come from the KLEE output; their shebangs already point at the Nix Python environment, so no interpreter needs to be on `PATH`.

The toolchain comes from `llvmPackages_klee`, the same LLVM 16 that KLEE is built against. This matters: bitcode emitted by a newer clang will not load into this KLEE. Base packages precede `packages` on `PATH` so a benchmark cannot accidentally shadow the matching compiler.

Only `coreutils` is present for shell utilities — `grep`, `sed`, and `awk` are not — and build systems are deliberately excluded. Benchmarks needing those, or `make`, `cmake`, or autotools, pass them through `packages`, which keeps them out of the shared layers. `llvm-config` is in the image but not on `PATH`, since it lives in LLVM's `dev` output; add `llvmPackages_klee.llvm.dev` to `packages` to expose it.

Mount benchmark sources at the default working directory:

```bash
podman run --rm -it -v "$PWD:/work" klee-bench-libxml2
```
