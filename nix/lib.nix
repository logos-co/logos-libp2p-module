# Builds the logos-libp2p-module library
{ pkgs, common, src }:

pkgs.stdenv.mkDerivation {
  pname = "${common.pname}-lib";
  version = common.version;

  inherit src;
  inherit (common) nativeBuildInputs buildInputs cmakeFlags meta env;

  # Determine platform-specific library extension
  libp2pLib = if pkgs.stdenv.hostPlatform.isWindows then "dll" else if pkgs.stdenv.hostPlatform.isDarwin then "dylib" else "so";

  postInstall = ''
    # Logos already installed the plugin correctly:
    # $out/lib/logos/modules/libp2p_module_plugin.{so,dylib}

    # macOS: fix install names if needed
    ${pkgs.lib.optionalString pkgs.stdenv.hostPlatform.isDarwin ''
      PLUGIN="$out/lib/logos/modules/libp2p_module_plugin.dylib"
      if [ -f "$PLUGIN" ]; then
        ${pkgs.darwin.cctools}/bin/install_name_tool \
          -id "@rpath/libp2p_module_plugin.dylib" \
          "$PLUGIN"
      fi
    ''}
  '';
}

