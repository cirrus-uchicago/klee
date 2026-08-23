{
  lib,
  llvmPackages,
  gllvm,
  cmake,
  ninja,
  python3,
  fetchFromGitHub,
}: let
  # Fetch the LLVM 16 monorepo — same major version KLEE is built with.
  llvmSrc = fetchFromGitHub {
    owner = "llvm";
    repo = "llvm-project";
    rev = "release/16.x";
    hash = "sha256-fspqSReX+VD+Nl/Cfq+tDcdPtnQPV1IRopNDfd5VtUs=";
  };
in
  llvmPackages.stdenv.mkDerivation {
    pname = "klee-libcxx";
    version = "16";

    src = llvmSrc;

    sourceRoot = "${llvmSrc.name}/llvm";

    # sourceRoot is the llvm directory, and unpackPhase only makes that one
    # writable, so reach back up to the monorepo root for both steps.
    prePatch = ''
      chmod -R u+w ../libcxx
      patch -p1 -d .. <${./patches/libcxx-klee-uclibc-ctype.patch}
    '';

    nativeBuildInputs = [
      cmake
      ninja
      gllvm
      python3
      llvmPackages.clang
      llvmPackages.llvm
    ];

    cmakeBuildType = "Release";

    cmakeFlags = [
      "-DLLVM_ENABLE_RUNTIMES=libcxx;libcxxabi"
      "-DLLVM_ENABLE_PROJECTS="
      "-DLLVM_ENABLE_PROJECTS_USED=ON"
      "-DLLVM_ENABLE_THREADS=OFF"
      "-DLIBCXX_ENABLE_THREADS=OFF"
      "-DLIBCXXABI_ENABLE_THREADS=OFF"
      "-DLIBCXX_ENABLE_SHARED=ON"
      "-DLIBCXX_ENABLE_STATIC_ABI_LIBRARY=ON"
    ];

    env = {
      # gllvm environment — tells gclang/gclang++ to use our clang
      LLVM_COMPILER = "clang";
      LLVM_COMPILER_PATH = "${llvmPackages.clang}/bin";
      # Suppress warnings during build
      NIX_CFLAGS_COMPILE = toString ["-Wno-unused-command-line-argument"];
    };

    # Override CC/CXX to use gllvm wrappers so bitcode is embedded in objects
    preConfigure = ''
      export CC=gclang
      export CXX=gclang++
    '';

    # Build only the runtimes target (libc++ and libc++abi)
    buildFlags = ["runtimes"];

    # Install from the runtimes subdirectory, then extract bitcode
    installPhase = ''
      ninja install-cxx install-cxxabi

      # Extract LLVM bitcode from all shared and static libraries
      for lib in "$out"/lib/*.so "$out"/lib/*.a "$out"/lib/*/*.so "$out"/lib/*/*.a; do
        [ -f "$lib" ] || continue
        echo "Extracting bitcode from $lib"
        get-bc -a ${llvmPackages.llvm}/bin/llvm-ar "$lib" || true
      done

      # KLEE's CMake searches for x86_64-unknown-linux-gnu but Nix builds
      # produce x86_64-pc-linux-gnu. Add compatibility symlinks.
      if [ -d "$out/lib/x86_64-pc-linux-gnu" ]; then
        ln -s x86_64-pc-linux-gnu "$out/lib/x86_64-unknown-linux-gnu"
      fi
      if [ -d "$out/include/x86_64-pc-linux-gnu" ]; then
        ln -s x86_64-pc-linux-gnu "$out/include/x86_64-unknown-linux-gnu"
      fi
    '';

    # Expose the full LLVM monorepo source for KLEE C++ exception handling
    # (KLEE needs the libcxxabi source directory)
    passthru.src = llvmSrc;

    meta = {
      description = "libc++ compiled to LLVM bitcode for KLEE symbolic execution";
      homepage = "https://libcxx.llvm.org";
      license = with lib.licenses; [
        asl20
        mit
      ];
      platforms = ["x86_64-linux"];
    };
  }
