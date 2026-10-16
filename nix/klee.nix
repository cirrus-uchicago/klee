{
  lib,
  stdenv,
  llvmPackages,
  callPackage,
  cmake,
  python3,
  z3,
  stp,
  cryptominisat,
  gperftools,
  sqlite,
  gtest,
  lit,
  # Source of KLEE — typically passed from flake as `self`.
  src,
  debug ? false,
  includeDebugInfo ? true,
  asserts ? false,
  # Debug runtime improves stack traces of the software under test.
  debugRuntime ? true,
  runtimeAsserts ? false,
  kleeuClibc ? null,
  # Only applied when kleeuClibc is null (i.e. the bundled build).
  extraKleeuClibcConfig ? {},
  kleeLibcxx ? null,
  enableCxx ? (kleeLibcxx != null),
  # Requires enableCxx.
  enableEhCxx ? false,
}: let
  # The chosen version of klee-uclibc.
  chosenKleeuClibc =
    if kleeuClibc == null
    then
      callPackage ./klee-uclibc.nix {
        inherit llvmPackages extraKleeuClibcConfig debugRuntime runtimeAsserts;
      }
    else kleeuClibc;

  # Python used for KLEE tests and learch ML searcher runtime.
  kleePython = python3.withPackages (
    ps:
      with ps; [
        tabulate
        torch
        numpy
        scikit-learn
      ]
  );
in
  # KLEE's own C++ is built by GCC: nixpkgs pairs every clang with the default
  # GCC's libstdc++, which can outgrow what clang 18 parses (GCC 16 already
  # does). The runtime bitcode still comes from clang 18 through LLVMCC.
  stdenv.mkDerivation {
    pname = "klee";
    version = "3.3";

    inherit src;

    nativeBuildInputs = [cmake];

    buildInputs =
      [
        llvmPackages.llvm
        cryptominisat
        gperftools
        sqlite
        stp
        z3
        kleePython
      ]
      ++ lib.optionals enableCxx [
        kleeLibcxx
      ];

    nativeCheckInputs = [
      gtest

      # Should appear BEFORE lit, since lit passes through python rather
      # than the python environment we make.
      kleePython
      (lit.override {python = kleePython;})
    ];

    cmakeBuildType =
      if debug
      then "Debug"
      else if includeDebugInfo
      then "RelWithDebInfo"
      else "MinSizeRel";

    cmakeFlags = let
      onOff = val:
        if val
        then "ON"
        else "OFF";
    in
      [
        "-DKLEE_RUNTIME_BUILD_TYPE=${
          if debugRuntime
          then "Debug"
          else "Release"
        }"
        "-DLLVMCC=${llvmPackages.clang}/bin/clang"
        "-DLLVMCXX=${llvmPackages.clang}/bin/clang++"
        "-DKLEE_ENABLE_TIMESTAMP=${onOff false}"
        "-DKLEE_UCLIBC_PATH=${chosenKleeuClibc}"
        "-DENABLE_KLEE_ASSERTS=${onOff asserts}"
        "-DENABLE_POSIX_RUNTIME=${onOff true}"
        "-DENABLE_UNIT_TESTS=${onOff false}"
        "-DENABLE_SYSTEM_TESTS=${onOff false}"
        "-DLIT_ARGS=--verbose"
        "-DGTEST_SRC_DIR=${gtest.src}"
        "-DGTEST_INCLUDE_DIR=${gtest.src}/googletest/include"
        "-Wno-dev"
      ]
      ++ lib.optionals enableCxx [
        "-DENABLE_KLEE_LIBCXX=ON"
        "-DKLEE_LIBCXX_DIR=${kleeLibcxx}"
        "-DKLEE_LIBCXX_INCLUDE_DIR=${kleeLibcxx}/include"
      ]
      ++ lib.optionals enableEhCxx [
        "-DENABLE_KLEE_EH_CXX=ON"
        "-DKLEE_LIBCXXABI_SRC_DIR=${kleeLibcxx.passthru.src}/libcxxabi"
      ];

    # Silence various warnings during the compilation of fortified bitcode.
    env.NIX_CFLAGS_COMPILE = toString ["-Wno-macro-redefined"];

    env.FILECHECK_OPTS = "--dump-input-filter=all";

    prePatch = ''
      patchShebangs --build .
    '';

    # https://github.com/klee/klee/issues/1690
    hardeningDisable = ["fortify"];

    enableParallelBuilding = true;
    doCheck = true;

    passthru =
      {
        # Let the user access the chosen uClibc outside the derivation.
        uclibc = chosenKleeuClibc;
      }
      // lib.optionalAttrs enableCxx {
        libcxx = kleeLibcxx;
      };

    __structuredAttrs = true;

    meta = {
      mainProgram = "klee";
      description = "Symbolic virtual machine built on top of LLVM";
      longDescription = ''
        KLEE is a symbolic virtual machine built on top of the LLVM compiler
        infrastructure. Currently, there are two primary components:

        1. The core symbolic virtual machine engine; this is responsible for
           executing LLVM bitcode modules with support for symbolic values. This
           is comprised of the code in lib/.

        2. A POSIX/Linux emulation layer oriented towards supporting uClibc, with
           additional support for making parts of the operating system environment
           symbolic.

        Additionally, there is a simple library for replaying computed inputs on
        native code (for closed programs). There is also a more complicated
        infrastructure for replaying the inputs generated for the POSIX/Linux
        emulation layer, which handles running native programs in an environment
        that matches a computed test input, including setting up files, pipes,
        environment variables, and passing command line arguments.
      '';
      homepage = "https://klee.github.io";
      license = lib.licenses.ncsa;
      platforms = ["x86_64-linux"];
      maintainers = with lib.maintainers; [jiezhuzzz];
    };
  }
