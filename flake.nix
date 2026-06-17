{
  description = "Logos Libp2p Module";

  inputs = {
    logos-module-builder.url = "github:logos-co/logos-module-builder";
    libp2p.url = "github:vacp2p/nim-libp2p/chore/cbind/metrics";

    openmetrics-module = {
      url = "github:logos-co/openmetrics-module";
      inputs.logos-module-builder.follows = "logos-module-builder";
    };
  };

  outputs = inputs@{ logos-module-builder, ... }:
    let
      externalLibInputs = {
        libp2p = {
          input = inputs.libp2p;
          packages.default = "cbind";
        };
      };

      module = logos-module-builder.lib.mkLogosModule {
        src = ./.;
        configFile = ./metadata.json;
        flakeInputs = inputs;
        inherit externalLibInputs;
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

      # mkLogosModuleTests splats extraCmakeFlags into cmake unquoted, so
      # multi-token values get shell-split. Inject sanitizer flags via env vars.
      baseSanitizerTests = logos-module-builder.lib.mkLogosModuleTests {
        src = ./.;
        testDir = ./tests;
        configFile = ./metadata.json;
        flakeInputs = inputs;
        inherit externalLibInputs;
        extraCmakeFlags = [ "-DCMAKE_BUILD_TYPE=Debug" ];
      };

      mkSanitized = system: sanitizer: runtimeOpts:
        let
          pkgs = import nixpkgs { inherit system; };
          clang = pkgs.clang;
          llvm = pkgs.llvm;  # llvm-symbolizer; symbol-based suppressions need it
          sanFlags = "-fsanitize=${sanitizer} -fno-omit-frame-pointer -g -O1";
          tsanSuppressions = pkgs.writeText "tsan.supp" ''
            race:libp2p.so
          '';
          # Wrapper code (Libp2pModuleImpl, src/*.cpp) is NOT suppressed —
          # real wrapper leaks (e.g. src/plugin.cpp:121 privkey) still surface.
          lsanSuppressions = pkgs.writeText "lsan.supp" ''
            leak:libp2p.so
            leak:std::promise
            leak:std::__future_base
            leak:QCoreApplication
            leak:QMetaObject
            leak:QArrayData
            leak:QObjectPrivate
            leak:LogosTestRunner
            leak:LogosTestContext
          '';
          extraRuntime =
            if sanitizer == "address" then ''
              export ASAN_SYMBOLIZER_PATH=${llvm}/bin/llvm-symbolizer
              export LSAN_OPTIONS="suppressions=${lsanSuppressions}:print_suppressions=0"
            '' else ''
              export TSAN_OPTIONS="$TSAN_OPTIONS:suppressions=${tsanSuppressions}:external_symbolizer_path=${llvm}/bin/llvm-symbolizer"
            '';
        in baseSanitizerTests.${system}.unit-tests.overrideAttrs (old: {
          nativeBuildInputs = (old.nativeBuildInputs or []) ++ [ clang llvm ];
          hardeningDisable = (old.hardeningDisable or []) ++ [ "all" ];
          preBuild = (old.preBuild or "") + ''
            export CC=${clang}/bin/clang
            export CXX=${clang}/bin/clang++
            export CFLAGS="${sanFlags}"
            export CXXFLAGS="${sanFlags}"
            export LDFLAGS="-fsanitize=${sanitizer}"
            export ${runtimeOpts}
            ${extraRuntime}
          '';
        });

      sanitizerPackages = builtins.listToAttrs (map (system: {
        name = system;
        value = {
          unit-tests-asan = mkSanitized system "address"
            "ASAN_OPTIONS=detect_leaks=1:abort_on_error=1:print_stacktrace=1:strict_string_checks=1:check_initialization_order=1";
          unit-tests-tsan = mkSanitized system "thread"
            "TSAN_OPTIONS=halt_on_error=1:second_deadlock_stack=1:history_size=7";
        };
      }) systems);

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
          e2eRuntime = [ pkgs.curl pkgs.coreutils pkgs.gnugrep pkgs.bash pkgs.iproute2 ];
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

      mergedPackages = builtins.listToAttrs (map (system: {
        name = system;
        value = (module.packages.${system} or {}) // (sanitizerPackages.${system} or {});
      }) systems);

    in module // {
      apps = mergedApps;
      checks = module.checks or {};
      packages = mergedPackages;
    };
}
