{
  lib,
  nix2container,
  dockerTools,
  runCommandLocal,
  bashInteractive,
  coreutils,
  gllvm,
  klee,
  klee-wrappers,
  llvmPackages,
}: let
  kleePackages = [klee klee-wrappers];

  toolchain = [
    llvmPackages.clang
    llvmPackages.llvm
    gllvm
    bashInteractive
    coreutils
  ];

  tmpdir = runCommandLocal "klee-container-tmp" {} "mkdir -p $out/tmp";

  # KLEE's runtime dependencies weigh about 4 GiB, dominated by LLVM, STP and
  # PyTorch, and move only when nixpkgs does. Layering them below KLEE itself
  # keeps a KLEE rebuild from invalidating them.
  depsLayer = nix2container.buildLayer {
    deps = klee.buildInputs ++ toolchain;
    copyToRoot = [dockerTools.binSh dockerTools.fakeNss tmpdir];
    maxLayers = 25;
    perms = [
      {
        path = tmpdir;
        regex = "/tmp";
        mode = "1777";
      }
    ];
  };

  kleeLayer = nix2container.buildLayer {
    deps = kleePackages;
    layers = [depsLayer];
  };

  mkKleeImage = {
    name,
    packages ? [],
    config ? {},
    ...
  } @ args:
    assert !(args ? layers);
      nix2container.buildImage (
        builtins.removeAttrs args ["packages" "config"]
        // {
          # Ancestors are listed flat rather than nested: buildImage forwards
          # only this list to its customization layer, which the store paths in
          # `config.Env` would otherwise make re-emit the whole closure.
          layers = [
            depsLayer
            kleeLayer
            (nix2container.buildLayer {
              deps = packages;
              layers = [depsLayer kleeLayer];
              # One store path per layer, so a package two benchmarks share
              # produces one blob rather than one per image.
              maxLayers = 64;
            })
          ];

          config =
            {
              Env = ["PATH=${lib.makeBinPath (kleePackages ++ toolchain ++ packages)}"];
              Cmd = ["/bin/sh"];
              WorkingDir = "/work";
            }
            // config;
        }
      );
in {
  inherit depsLayer kleeLayer mkKleeImage;
}
