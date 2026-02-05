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

      libp2pCbind = system: libp2p.packages.${system}.cbind;

      buildModule = system:
        logos-module-builder.lib.mkLogosModule {
          src = ./.;
          configFile = ./module.yaml;

          preConfigure = ''
            mkdir -p lib
            cp -r "${libp2pCbind system}/lib"/* lib/
            cp -r "${libp2pCbind system}/include"/* lib/
          '';

          postInstall = ''
            mkdir -p $out/lib
            cp "${libp2pCbind system}/lib"/*.dylib $out/lib/ 2>/dev/null || true
            cp "${libp2pCbind system}/lib"/*.so $out/lib/ 2>/dev/null || true
          '';
        };

    in {

      packages = forAllSystems (system:
        (buildModule system).packages.${system}
      );

      devShells = forAllSystems (system:
        (buildModule system).devShells.${system}
      );

      checks = forAllSystems (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
          module = buildModule system;
          pkg = module.packages.${system}.lib;
        in {
          module-tests = pkg.overrideAttrs (old: {
            doCheck = true;
            checkPhase = ''
              echo "Running Qt tests..."

              export QT_QPA_PLATFORM=offscreen
              export QT_PLUGIN_PATH=${pkgs.qt6.qtbase}/${pkgs.qt6.qtbase.qtPluginPrefix}

              ctest --output-on-failure
            '';
          });
        }
      );
    };
}

