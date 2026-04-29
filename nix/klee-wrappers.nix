{
  lib,
  stdenv,
  makeWrapper,
  klee,
  klee-libcxx,
  llvmPackages,
}:
stdenv.mkDerivation {
  pname = "klee-wrappers";
  inherit (klee) version;

  src = ./scripts;

  nativeBuildInputs = [makeWrapper];

  dontBuild = true;
  dontConfigure = true;

  installPhase = ''
    mkdir -p $out/bin

    # Install clang++ wrapper with KLEE_LIBCXX_PATH set
    install -m755 clang++.sh $out/bin/clang++
    wrapProgram $out/bin/clang++ \
      --prefix PATH : ${llvmPackages.clang}/bin \
      --set KLEE_LIBCXX_PATH ${klee-libcxx}
  '';

  meta = {
    description = "C++ compiler wrapper for KLEE symbolic execution";
    mainProgram = "clang++";
    platforms = ["x86_64-linux"];
  };
}
