{
  description = "KLEE Symbolic Execution Engine";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixpkgs-unstable";
    nixpkgs-legacy.url = "github:nixos/nixpkgs/25.05";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = {
    self,
    nixpkgs,
    nixpkgs-legacy,
    flake-utils,
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
          klee = final.klee;
          klee-libcxx = final.klee-libcxx;
        };
      };
    }
    // flake-utils.lib.eachDefaultSystem (
      system: let
        pkgs = import nixpkgs {
          inherit system;
          overlays = [self.overlays.default];
        };
      in {
        packages = {
          default = pkgs.klee;
          klee = pkgs.klee;
          klee-libcxx = pkgs.klee-libcxx;
          klee-wrappers = pkgs.klee-wrappers;
        };

        # Development shell with all dependencies
        devShells.default = pkgs.mkShell {
          inputsFrom = [pkgs.klee];

          KLEE_LIBCXX_PATH = "${pkgs.klee-libcxx}";

          packages = with pkgs; [
            # KLEE itself
            pkgs.klee

            # KLEE C++ convenience wrappers
            pkgs.klee-wrappers

            # Build tools
            cmake
            ninja
            gllvm

            # LLVM/Clang 16 (same version KLEE is built with)
            pkgs.llvmPackages_klee.clang
            pkgs.llvmPackages_klee.llvm
            pkgs.llvmPackages_klee.clang-tools

            # Debugging and development tools
            gdb
            lldb

            # Testing tools
            lit
          ];

          shellHook = ''
            echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
            echo "KLEE Development Environment"
            echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
            echo ""
            echo "LLVM/Clang version: $(clang --version | head -1)"
            echo ""
            echo "Test a C program:"
            echo "  clang -emit-llvm -c -g -O0 -Xclang -disable-O0-optnone program.c -o program.bc"
            echo "  klee program.bc"
            echo ""
            echo "Test a C++ program:"
            echo "  clang++ -emit-llvm -c -g -O0 -Xclang -disable-O0-optnone program.cpp -o program.bc"
            echo "  klee --libc=uclibc --libcxx program.bc"
            echo ""
            echo "Note: clang++ is pre-configured with libc++ include paths for KLEE"
            echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
          '';
        };

        # Checks (runs on nix flake check)
        checks = {
          klee-build = pkgs.klee;
        };
      }
    );
}
