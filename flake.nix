{
  description = "Logos Libp2p Module";

  inputs = {
    logos-module-builder.url = "github:/Logos-co/logos-module-builder";
    nixpkgs.follows = "logos-module-builder/nixpkgs";
    libp2p.url = "github:vacp2p/nim-libp2p";
  };

  outputs = { self, logos-module-builder, nixpkgs, libp2p }:
    let
      lib = nixpkgs.lib;
      systems = [ "aarch64-darwin" "x86_64-darwin" "aarch64-linux" "x86_64-linux" ];
      forAllSystems = f: lib.genAttrs systems (system: f system);
      
      # Get pre-built libp2p cbind package for each system
      libp2pCbind = system: libp2p.packages.${system}.cbind;
      
      # Build module using mkLogosModule with library setup
      buildModule = system: 
        logos-module-builder.lib.mkLogosModule {
          src = ./.;
          configFile = ./module.yaml;
          
          # Copy library to lib/ before cmake (for linking)
          preConfigure = ''
            mkdir -p lib
            cp -r "${libp2pCbind system}/lib"/* lib/
            cp -r "${libp2pCbind system}/include"/* lib/
          '';
          
          # Copy library to output after install (for runtime)
          postInstall = ''
            echo "Copying libp2p library to output..."
            cp "${libp2pCbind system}/lib"/*.dylib $out/lib/ 2>/dev/null || true
            cp "${libp2pCbind system}/lib"/*.so $out/lib/ 2>/dev/null || true
          '';
        };
    in {
      packages = forAllSystems (system: (buildModule system).packages.${system});
      devShells = forAllSystems (system: (buildModule system).devShells.${system});
    };
}
