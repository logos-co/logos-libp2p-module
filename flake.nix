{
  description = "Logos Libp2p Module";

  inputs = {
    logos-module-builder.url = "github:/logos-co/logos-module-builder";
    libp2p.url = "github:vacp2p/nim-libp2p";
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

      testsApps = builtins.listToAttrs (map (system:
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
        in {
          name = system;
          value = { tests = { type = "app"; program = toString runner; }; };
        }
      ) systems);

      existingApps = module.apps or {};
      mergedApps = builtins.listToAttrs (map (system: {
        name = system;
        value = (existingApps.${system} or {}) // (testsApps.${system} or {});
      }) systems);

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
