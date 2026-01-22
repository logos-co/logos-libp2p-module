# Generates headers from the libp2p module plugin using logos-cpp-generator
{ pkgs, common, src, lib, logosSdk }:

pkgs.stdenv.mkDerivation {
  pname = "${common.pname}-headers";
  version = common.version;

  inherit src;
  inherit (common) meta;

  # We need the generator and the built plugin
  nativeBuildInputs = [ logosSdk ];

  # No configure phase needed
  dontConfigure = true;

   buildPhase = ''
    set -euo pipefail

    mkdir -p generated_headers

    PLUGIN_DIR="${lib}/lib/logos/modules"

    echo "Looking for plugin in $PLUGIN_DIR"
    ls -la "$PLUGIN_DIR" || true

    if [ -f "$PLUGIN_DIR/libp2p_module_plugin.dylib" ]; then
      PLUGIN_FILE="$PLUGIN_DIR/libp2p_module_plugin.dylib"
    elif [ -f "$PLUGIN_DIR/libp2p_module_plugin.so" ]; then
      PLUGIN_FILE="$PLUGIN_DIR/libp2p_module_plugin.so"
    elif [ -f "$PLUGIN_DIR/libp2p_module_plugin.dll" ]; then
      PLUGIN_FILE="$PLUGIN_DIR/libp2p_module_plugin.dll"
    else
      echo "Error: libp2p_module_plugin not found in $PLUGIN_DIR"
      exit 1
    fi

    # Ensure dependent libraries can be found
    export LD_LIBRARY_PATH="${lib}/lib:''${LD_LIBRARY_PATH:-}"
    export DYLD_LIBRARY_PATH="${lib}/lib:''${DYLD_LIBRARY_PATH:-}"

    echo "Running logos-cpp-generator on $PLUGIN_FILE"
    logos-cpp-generator "$PLUGIN_FILE" \
      --output-dir generated_headers \
      --module-only || {
        echo "Warning: logos-cpp-generator failed; creating marker"
        touch generated_headers/.no-api
      }
  '';

  installPhase = ''
    set -euo pipefail

    # Install generated headers
    mkdir -p $out/include

    # Copy all generated files to include/ if they exist
    if [ -d ./generated_headers ] && [ "$(ls -A ./generated_headers 2>/dev/null)" ]; then
      echo "Copying generated headers..."
	  cp -r ./generated_headers/. $out/include/
    else
      echo "Warning: No generated headers found, creating empty include directory"
      # Create a placeholder file to indicate headers should be generated from metadata
      echo "# Generated headers from metadata.json" > $out/include/.generated
    fi
  '';
}

