# Builds the logos-libp2p-module library
{ pkgs, common, src }:

pkgs.stdenv.mkDerivation {
  pname = "${common.pname}-lib";
  version = common.version;

  inherit src;
  inherit (common) nativeBuildInputs buildInputs cmakeFlags meta env;

  # Determine platform-specific library extension
  libp2pLib = if pkgs.stdenv.hostPlatform.isWindows then "dll" else if pkgs.stdenv.hostPlatform.isDarwin then "dylib" else "so";

}

