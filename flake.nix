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
        klee = final.callPackage ./nix/klee.nix {
          llvmPackages = final.llvmPackages_klee;
          # Use the flake's own source (the klee repo itself)
          src = self;
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
        };

        # Development shell with all dependencies
        devShells.default = pkgs.mkShell {
          inputsFrom = [pkgs.klee];
          packages = with pkgs; [
            # Build tools
            cmake
            ninja

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
            echo "Test a program:"
            echo "  clang -emit-llvm -c program.c -o program.bc"
            echo "  ./result/bin/klee program.bc"
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
