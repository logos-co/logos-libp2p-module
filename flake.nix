{
  description = "Logos Libp2p Module";

  inputs = {
    logos-module-builder.url = "github:logos-co/logos-module-builder/fix/codegen-explicit-ctor";
    libp2p.url = "github:vacp2p/nim-libp2p";
    openmetrics-module.url = "github:logos-co/openmetrics-module";

    # logoscore-cli and lgpm pull the same logos-nix / nixpkgs tree as the
    # module builder; without these follows each drags in a full duplicate
    # (the flake.lock balloons by tens of thousands of lines).
    logoscore-cli = {
      url = "github:logos-co/logos-logoscore-cli";
      inputs.nixpkgs.follows = "logos-module-builder/nixpkgs";
      inputs.logos-nix.follows = "logos-module-builder/logos-nix";
      inputs.logos-cpp-sdk.follows = "logos-module-builder/logos-cpp-sdk";
      inputs.nix-bundle-logos-module-install.follows = "logos-module-builder/nix-bundle-logos-module-install";
    };

    lgpm = {
      url = "github:logos-co/logos-package-manager";
      inputs.nixpkgs.follows = "logos-module-builder/nixpkgs";
      inputs.logos-nix.follows = "logos-module-builder/logos-nix";
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

      # Pre-resolved store paths so the e2e runs against pinned module/CLI builds.
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

          # Exposed as `nix run .#openmetrics-e2e`, driven by a dedicated CI
          # step. It spins up a live logoscore daemon (TCP + IPC socket), which
          # can't run inside the hermetic `nix flake check` sandbox, so it is
          # deliberately not a flake check.
          openmetricsE2eApp = pkgs.writeShellScript "openmetrics-e2e" ''
            export PATH=${pkgs.lib.makeBinPath e2eRuntime}:$PATH
            export LIBP2P_LGX_DIR=${env.LIBP2P_LGX_DIR}
            export OPENMETRICS_LGX_DIR=${env.OPENMETRICS_LGX_DIR}
            export LOGOSCORE_BIN=${env.LOGOSCORE_BIN}
            export LGPM_BIN=${env.LGPM_BIN}
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
