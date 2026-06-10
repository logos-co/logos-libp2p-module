{
  description = "Logos Libp2p Module";

  inputs = {
    logos-module-builder.url = "github:/logos-co/logos-module-builder";
    libp2p.url = "github:vacp2p/nim-libp2p";
    openmetrics-module.url = "github:logos-co/openmetrics-module";
    logoscore-cli.url = "github:logos-co/logos-logoscore-cli";
    lgpm.url = "github:logos-co/logos-package-manager";
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
        tests = {
          dir = ./tests;
        };
      };

      nixpkgs = logos-module-builder.inputs.nixpkgs;
      systems = [ "aarch64-darwin" "x86_64-darwin" "aarch64-linux" "x86_64-linux" ];

      forEachSystem = f: builtins.listToAttrs (map (system: {
        name = system;
        value = f system;
      }) systems);

      # Pre-resolved store paths so the e2e is hermetic under `nix flake check`.
      e2eEnv = system: {
        LIBP2P_LGX_DIR     = "${module.packages.${system}.lgx}";
        OPENMETRICS_LGX_DIR = "${inputs.openmetrics-module.packages.${system}.lgx}";
        LOGOSCORE_BIN      = "${inputs.logoscore-cli.packages.${system}.default}/bin/logoscore";
        LGPM_BIN           = "${inputs.lgpm.packages.${system}.cli}/bin/lgpm";
      };

      perSystem = forEachSystem (system:
        let
          pkgs = import nixpkgs { inherit system; };
          unitTests = module.packages.${system}.unit-tests;

          runner = pkgs.writeShellScript "run-tests" ''
            filter="''${1:-}"
            ran=0
            for bin in ${unitTests}/bin/*; do
              name="$(basename "$bin")"
              if [ -n "$filter" ] && ! echo "$name" | grep -q "$filter"; then
                continue
              fi
              echo "=== $name ==="
              "$bin"
              ran=$((ran + 1))
            done
            if [ "$ran" -eq 0 ] && [ -n "$filter" ]; then
              echo "No test binary matched filter: $filter" >&2
              exit 1
            fi
          '';

          env = e2eEnv system;
          e2eRuntime = [ pkgs.curl pkgs.coreutils pkgs.gnugrep pkgs.bash ];
          e2eScript = ./tests/integration_e2e/openmetrics_e2e.sh;

          openmetricsE2eApp = pkgs.writeShellScript "openmetrics-e2e" ''
            export PATH=${pkgs.lib.makeBinPath e2eRuntime}:$PATH
            export LIBP2P_LGX_DIR=${env.LIBP2P_LGX_DIR}
            export OPENMETRICS_LGX_DIR=${env.OPENMETRICS_LGX_DIR}
            export LOGOSCORE_BIN=${env.LOGOSCORE_BIN}
            export LGPM_BIN=${env.LGPM_BIN}
            exec ${e2eScript} "$@"
          '';

          openmetricsE2eCheck = pkgs.runCommand "openmetrics-e2e" ({
            nativeBuildInputs = e2eRuntime;
          } // env) ''
            bash ${e2eScript}
            touch $out
          '';
        in {
          apps = {
            tests = { type = "app"; program = toString runner; };
            openmetrics-e2e = { type = "app"; program = toString openmetricsE2eApp; };
          };
          checks = { openmetrics-e2e = openmetricsE2eCheck; };
        }
      );

      existingApps = module.apps or {};
      existingChecks = module.checks or {};

      mergedApps = forEachSystem (system:
        (existingApps.${system} or {}) // (perSystem.${system}.apps or {})
      );
      mergedChecks = forEachSystem (system:
        (existingChecks.${system} or {}) // (perSystem.${system}.checks or {})
      );

    in module // {
      apps = mergedApps;
      checks = mergedChecks;
    };
}
