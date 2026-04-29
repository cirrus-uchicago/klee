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
  };

  outputs = {
    self,
    nixpkgs,
    nixpkgs-legacy,
    flake-utils,
    treefmt-nix,
  }:
    {
      overlays.default = final: prev: {
        llvmPackages_klee = nixpkgs-legacy.legacyPackages.${prev.system}.llvmPackages_16;
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
      in {
        packages = {
          default = pkgs.klee;
          inherit (pkgs) klee klee-libcxx klee-wrappers;
        };

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
        checks = {
          klee-build = pkgs.klee;
          formatting = treefmtEval.config.build.check self;
        };
      }
    );
}
