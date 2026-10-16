{
  lib,
  llvmPackages,
  gllvm,
  cmake,
  ninja,
  python3,
}: let
  llvmSrc = llvmPackages.llvm.monorepoSrc;
in
  llvmPackages.stdenv.mkDerivation {
    pname = "klee-libcxx";
    inherit (llvmPackages.llvm) version;

    src = llvmSrc;

    sourceRoot = "${llvmSrc.name}/runtimes";

    # sourceRoot is the runtimes directory, and unpackPhase only makes that one
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
      # libc++abi defaults to the LLVM unwinder since LLVM 17 and refuses to
      # configure unless libunwind is built alongside it.
      "-DLLVM_ENABLE_RUNTIMES=libcxx;libcxxabi;libunwind"
      # klee lists this package in buildInputs, putting include/ on the
      # compiler's search path, where libunwind's unwind.h would shadow the
      # clang one that runtime/klee-eh-cxx needs.
      "-DLIBUNWIND_INSTALL_HEADERS=OFF"
      # KLEE's CMake and the clang++ wrapper expect the per-target layout,
      # which only the llvm root enables by default.
      "-DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=ON"
      "-DLLVM_ENABLE_THREADS=OFF"
      "-DLIBCXX_ENABLE_THREADS=OFF"
      "-DLIBCXXABI_ENABLE_THREADS=OFF"
      "-DLIBCXX_ENABLE_SHARED=ON"
      "-DLIBCXX_ENABLE_STATIC_ABI_LIBRARY=ON"
      # Unoptimized, unlike upstream KLEE's Release build: with LLVM 18, -O2
      # if-converts libc++'s EOF checks into pointer selects, so reading
      # symbolic input through std::cin degrades into symbolic-offset reads
      # that never finish (test/CXX/symex/libc++/cin_sym.cpp).
      "-DCMAKE_C_FLAGS_RELEASE=-DNDEBUG"
      "-DCMAKE_CXX_FLAGS_RELEASE=-DNDEBUG"
    ];

    # The fortify flags the cc-wrapper injects include -O2.
    hardeningDisable = ["fortify"];

    env = {
      # gllvm environment — tells gclang/gclang++ to use our clang
      LLVM_COMPILER = "clang";
      LLVM_COMPILER_PATH = "${llvmPackages.clang}/bin";
      # Suppress warnings during build
      NIX_CFLAGS_COMPILE = toString [
        "-Wno-unused-command-line-argument"
        # Keep klee --optimize able to touch libc++ despite the -O0 build.
        "-Xclang"
        "-disable-O0-optnone"
      ];
    };

    # Override CC/CXX to use gllvm wrappers so bitcode is embedded in objects
    preConfigure = ''
      export CC=gclang
      export CXX=gclang++
    '';

    # Install from the runtimes subdirectory, then extract bitcode
    installPhase = ''
      ninja install-cxx install-cxxabi install-unwind

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
