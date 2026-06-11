{
  description = "Logos Libp2p Module";

  inputs = {
    logos-module-builder.url = "github:logos-co/logos-module-builder/fix/codegen-explicit-ctor";
    libp2p.url = "github:vacp2p/nim-libp2p";

    openmetrics-module = {
      url = "github:logos-co/openmetrics-module";
      inputs.logos-module-builder.follows = "logos-module-builder";
    };
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

      # Pre-resolved store paths for the two .lgx bundles the e2e installs.
      e2eEnv = system: {
        LIBP2P_LGX_DIR      = "${module.packages.${system}.lgx}";
        OPENMETRICS_LGX_DIR = "${inputs.openmetrics-module.packages.${system}.lgx}";
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

          # Exposed as `nix run .#openmetrics-e2e`, driven by a dedicated CI
          # step. It spins up a live logoscore daemon (TCP + IPC socket), which
          # can't run inside the hermetic `nix flake check` sandbox, so it is
          # deliberately not a flake check.
          #
          # The .lgx bundles are pinned via flake inputs; logoscore and lgpm
          # are supplied by the caller (LOGOSCORE_BIN / LGPM_BIN), falling back
          # to $PATH, since they are not vendored into this flake.lock.
          openmetricsE2eApp = pkgs.writeShellScript "openmetrics-e2e" ''
            export PATH=${pkgs.lib.makeBinPath e2eRuntime}:$PATH
            export LIBP2P_LGX_DIR=${env.LIBP2P_LGX_DIR}
            export OPENMETRICS_LGX_DIR=${env.OPENMETRICS_LGX_DIR}
            export LOGOSCORE_BIN="''${LOGOSCORE_BIN:-logoscore}"
            export LGPM_BIN="''${LGPM_BIN:-lgpm}"
            exec ${e2eScript} "$@"
          '';
        in {
          apps = {
            tests = { type = "app"; program = toString runner; };
            openmetrics-e2e = { type = "app"; program = toString openmetricsE2eApp; };
          };
        }
      );

      existingApps = module.apps or {};

      mergedApps = forEachSystem (system:
        (existingApps.${system} or {}) // (perSystem.${system}.apps or {})
      );

    in module // {
      apps = mergedApps;
      checks = module.checks or {};
    };
}
