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
            mkdir -p include
            cp -r "${libp2pCbind system}/include"/* include/
          '' + lib.optionalString (lib.hasSuffix "darwin" system) ''
            for f in lib/*.dylib; do
              [ -f "$f" ] || continue
              chmod +w "$f"
              install_name_tool -id "@rpath/$(basename "$f")" "$f"
            done
          '';
          postInstall = ''
            mkdir -p $out/lib
            cp lib/*.dylib $out/lib/ 2>/dev/null || true
            cp lib/*.so $out/lib/ 2>/dev/null || true
          '';
        };

    in {

      packages = forAllSystems (system:
        (buildModule system).packages.${system}
      );

      devShells = forAllSystems (system:
        let
          builderShell = (buildModule system).devShells.${system}.default;
          cbind = libp2pCbind system;
        in {
          default = builderShell.overrideAttrs (old: {
            shellHook = (old.shellHook or "") + ''
              export CMAKE_MODULE_PATH=${logos-module-builder}/cmake
              export LOGOS_MODULE_BUILDER_ROOT=${logos-module-builder}

              # replicate preConfigure from build
              mkdir -p lib
              cp -r ${cbind}/lib/* lib/ 2>/dev/null || true

              mkdir -p include
              cp -r ${cbind}/include/* include/ 2>/dev/null || true
            '';
          });
        }
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
