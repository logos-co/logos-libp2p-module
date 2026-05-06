{
  description = "Logos Libp2p Module";

  inputs = {
    logos-module-builder.url = "github:/logos-co/logos-module-builder";
    libp2p.url = "github:vacp2p/nim-libp2p";
  };

  outputs = inputs@{ logos-module-builder, ... }:
    let
      module = logos-module-builder.lib.mkLogosModule {
        src = ./.;
        configFile = ./metadata.json;
        flakeInputs = inputs;
        externalLibInputs = {
          libp2p = {
            input = inputs.libp2p;
            packages.default = "cbind";
          };
        };
      };

      nixpkgs = logos-module-builder.inputs.nixpkgs;
      systems = [ "aarch64-darwin" "x86_64-darwin" "aarch64-linux" "x86_64-linux" ];

      checks = builtins.listToAttrs (map (system:
        let
          pkgs = import nixpkgs { inherit system; };
          pkg = module.packages.${system}.lib;
        in {
          name = system;
          value = {
            module-tests = pkg.overrideAttrs (old: {
              doCheck = true;
              checkPhase = ''
                export QT_QPA_PLATFORM=offscreen
                export QT_PLUGIN_PATH=${pkgs.qt6.qtbase}/${pkgs.qt6.qtbase.qtPluginPrefix}
                ctest --output-on-failure
              '';
            });
          };
        }
      ) systems);

    in module // { inherit checks; };
}
