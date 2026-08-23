{
  description = "KLEE Symbolic Execution Engine";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixpkgs-unstable";
    nixpkgs-legacy.url = "github:nixos/nixpkgs/25.05";
    flake-utils.url = "github:numtide/flake-utils";
    treefmt-nix = {
      url = "github:numtide/treefmt-nix";
      inputs.nixpkgs.follows = "nixpkgs";
    };
    nix2container = {
      url = "github:nlewo/nix2container";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs = {
    self,
    nixpkgs,
    nixpkgs-legacy,
    flake-utils,
    treefmt-nix,
    nix2container,
  }:
    {
      overlays.default = final: prev: {
        llvmPackages_klee = nixpkgs-legacy.legacyPackages.${prev.stdenv.hostPlatform.system}.llvmPackages_16;
        klee-libcxx = final.callPackage ./nix/klee-libcxx.nix {
          llvmPackages = final.llvmPackages_klee;
        };
        klee = final.callPackage ./nix/klee.nix {
          llvmPackages = final.llvmPackages_klee;
          src = self;
          kleeLibcxx = final.klee-libcxx;
        };
        klee-wrappers = final.callPackage ./nix/klee-wrappers.nix {
          llvmPackages = final.llvmPackages_klee;
          inherit (final) klee klee-libcxx;
        };
        klee-containers = final.callPackage ./nix/container.nix {
          inherit (nix2container.packages.${prev.stdenv.hostPlatform.system}) nix2container;
          llvmPackages = final.llvmPackages_klee;
          inherit (final) klee klee-wrappers;
        };
      };
    }
    // flake-utils.lib.eachDefaultSystem (
      system: let
        pkgs = import nixpkgs {
          inherit system;
          overlays = [self.overlays.default];
        };
        treefmtEval = treefmt-nix.lib.evalModule pkgs {
          projectRootFile = "flake.nix";
          programs.alejandra.enable = true;
          programs.statix.enable = true;
        };

        # nix2container only supports Linux.
        inherit (pkgs.stdenv.hostPlatform) isLinux;

        kleeImage = pkgs.klee-containers.mkKleeImage {name = "klee";};
        kleeImageExample = pkgs.klee-containers.mkKleeImage {
          name = "klee-example";
          packages = with pkgs; [gnumake pkg-config openssl];
        };
      in {
        packages =
          {
            default = pkgs.klee;
            inherit (pkgs) klee klee-libcxx klee-wrappers;
          }
          // pkgs.lib.optionalAttrs isLinux {
            klee-deps-layer = pkgs.klee-containers.depsLayer;
            klee-layer = pkgs.klee-containers.kleeLayer;
            klee-image = kleeImage;
            klee-image-example = kleeImageExample;
          };

        lib = pkgs.lib.optionalAttrs isLinux {inherit (pkgs.klee-containers) mkKleeImage;};

        # Development shell with all dependencies
        devShells.default = pkgs.mkShell {
          inputsFrom = [pkgs.klee];

          KLEE_LIBCXX_PATH = pkgs.klee-libcxx;

          # cmake, clang, and llvm come from `inputsFrom = [pkgs.klee]`.
          packages = with pkgs; [
            klee
            klee-wrappers
            ninja
            gllvm
            llvmPackages_klee.clang-tools
            gdb
            lldb
            lit
          ];

          shellHook = ''
            echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
            echo "KLEE Development Environment"
            echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
            echo ""
            echo "LLVM/Clang version: $(clang --version | head -1)"
            echo ""
            echo "Basic Usage:"
            echo "  C program:"
            echo "    clang -emit-llvm -c -g -O0 -Xclang -disable-O0-optnone program.c -o program.bc"
            echo "    klee program.bc"
            echo ""
            echo "  C++ program:"
            echo "    clang++ -emit-llvm -c -g -O0 -Xclang -disable-O0-optnone program.cpp -o program.bc"
            echo "    klee --libc=uclibc --libcxx program.bc"
            echo ""
            echo "Symbolic Environment (requires --posix-runtime):"
            echo "  --sym-stdin <N>              Make stdin symbolic (N bytes)"
            echo "    klee --posix-runtime --libc=uclibc program.bc --sym-stdin 10"
            echo ""
            echo "  --sym-arg <N>                Single symbolic argument (max N bytes)"
            echo "    klee --posix-runtime --libc=uclibc program.bc --sym-arg 5"
            echo ""
            echo "  --sym-args <MIN> <MAX> <N>   Symbolic arguments (MIN to MAX args, each max N bytes)"
            echo "    klee --posix-runtime --libc=uclibc program.bc --sym-args 0 2 4"
            echo ""
            echo "  --sym-files <NUM> <N>        NUM symbolic files, each N bytes"
            echo "    klee --posix-runtime --libc=uclibc program.bc --sym-files 2 100"
            echo ""
            echo "  --sym-stdout                 Make stdout symbolic"
            echo "    klee --posix-runtime --libc=uclibc program.bc --sym-stdout"
            echo ""
            echo "  Combined example:"
            echo "    klee --posix-runtime --libc=uclibc program.bc --sym-stdin 10 --sym-arg 5"
            echo ""
            echo "  For C++ programs, add --libcxx (required):"
            echo "    klee --posix-runtime --libc=uclibc --libcxx program.bc --sym-stdin 10"
            echo ""
            echo "Note: clang++ is pre-configured with libc++ include paths for KLEE"
            echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
          '';
        };

        # Formatter — run with `nix fmt`
        formatter = treefmtEval.config.build.wrapper;

        # Checks (runs on nix flake check)
        checks =
          {
            klee-build = pkgs.klee;
            formatting = treefmtEval.config.build.check self;
          }
          // pkgs.lib.optionalAttrs isLinux {
            container-layer-sharing =
              pkgs.runCommand "container-layer-sharing" {
                nativeBuildInputs = [pkgs.jq];
              } ''
                for image in ${kleeImage} ${kleeImageExample}; do
                  jq -r '.layers[].paths[].path' "$image" >paths
                  sort -u paths >unique
                  if [ "$(wc -l <paths)" != "$(wc -l <unique)" ]; then
                    echo "$image emits the same store path in more than one layer:"
                    sort paths | uniq -d
                    exit 1
                  fi
                done

                jq -r '.layers[].digest' ${kleeImage} | sort >base
                jq -r '.layers[].digest' ${kleeImageExample} | sort >derived
                comm -23 base derived >missing
                if [ -s missing ]; then
                  echo "klee-image-example does not reuse every klee-image layer:"
                  cat missing
                  exit 1
                fi

                touch $out
              '';
          };
      }
    );
}
