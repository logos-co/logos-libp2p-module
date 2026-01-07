# Builds the logos-libp2p-module library
{ pkgs, common, src }:

pkgs.stdenv.mkDerivation {
  pname = "${common.pname}-lib";
  version = common.version;
  
  inherit src;
  inherit (common) nativeBuildInputs buildInputs cmakeFlags meta env;
  
  # Determine platform-specific library extension
  liblibp2pLib = if pkgs.stdenv.hostPlatform.isDarwin then "liblibp2p.dylib" else "liblibp2p.so";
  
  postInstall = ''
    mkdir -p $out/lib
    
    # Copy liblibp2p library from source
    srcLib="$src/lib/''${liblibp2pLib}"
    if [ ! -f "$srcLib" ]; then
      echo "Expected ''${liblibp2pLib} in $src/lib/" >&2
      exit 1
    fi
    cp "$srcLib" "$out/lib/"
    
    # Fix the install name of liblibp2p on macOS
    ${pkgs.lib.optionalString pkgs.stdenv.hostPlatform.isDarwin ''
      ${pkgs.darwin.cctools}/bin/install_name_tool -id "@rpath/''${liblibp2pLib}" "$out/lib/''${liblibp2pLib}"
    ''}
    
    # Copy the libp2p module plugin from the installed location
    if [ -f "$out/lib/logos/modules/libp2p_module_plugin.dylib" ]; then
      cp "$out/lib/logos/modules/libp2p_module_plugin.dylib" "$out/lib/"
      
      # Fix the plugin's reference to liblibp2p on macOS
      ${pkgs.lib.optionalString pkgs.stdenv.hostPlatform.isDarwin ''
        # Find what liblibp2p path the plugin is referencing and change it to @rpath
        for dep in $(${pkgs.darwin.cctools}/bin/otool -L "$out/lib/libp2p_module_plugin.dylib" | grep liblibp2p | awk '{print $1}'); do
          ${pkgs.darwin.cctools}/bin/install_name_tool -change "$dep" "@rpath/''${liblibp2pLib}" "$out/lib/libp2p_module_plugin.dylib"
        done
      ''}
    elif [ -f "$out/lib/logos/modules/libp2p_module_plugin.so" ]; then
      cp "$out/lib/logos/modules/libp2p_module_plugin.so" "$out/lib/"
    else
      echo "Error: No libp2p_module_plugin library file found"
      exit 1
    fi
    
    # Remove the nested structure we don't want
    rm -rf "$out/lib/logos" 2>/dev/null || true
    rm -rf "$out/share" 2>/dev/null || true
  '';
}

