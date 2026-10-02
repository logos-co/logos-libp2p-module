# Tests

This directory contains Qt-based tests for the `logos-libp2p-module`.

Tests come in two layers:

- **Fast unit layer** (`libp2p_module_unit_tests`) — `unit_config.cpp`,
  `unit_metrics.cpp`, `unit_sync.cpp`. Exercise only header-inline logic
  (config parsing, `Metric` JSON, the await/parse primitives), construct no
  `Libp2pModuleImpl`, and link without the libp2p library, so they always build
  and run.
- **Integration layer** (`libp2p_module_tests`), everything that drives a real
  node. Built only when the libp2p shared library is staged in `../lib`, under
  either name the packaging produces (`liblibp2p.so`, `liblibp2p.dylib`) or the
  hand-staged one (`libp2p.so`, `libp2p.dylib`). A bare checkout without it
  skips this layer. Configure with `-DLIBP2P_TESTS_REQUIRE_LIB=ON` to turn the
  skip into a configure error; CI sets it.

Note: All commands should be executed from the project root.

## Building

Enter the development shell and configure CMake:

```bash
nix develop
cmake -B build -S .
```

Build the module and all tests:

```bash
cmake --build build -j
```

## Running tests

Run all tests:

```bash
ctest --test-dir build
```

Run with full output (useful for debugging failures):

```bash
ctest --test-dir build -V
```

Run one layer:

```bash
ctest --test-dir build -R libp2p_module_unit_tests
ctest --test-dir build -R libp2p_module_tests
```

Or run the test executables directly:

```bash
./build/libp2p_module_unit_tests
./build/libp2p_module_tests
```

## Standalone (logoscore)

`integration_e2e/standalone_e2e.sh` runs this module on its own under a live
`logoscore` daemon and asserts the standalone commands and their outputs:
`createNode` binds the requested port, `start` succeeds, and `getNodeInfo`
returns the expected `Version` / `MyBoundPorts` / `PeerId` / `Multiaddrs`. It
also covers the negative paths — an unknown `getNodeInfo` field and a malformed
`createNode` config are rejected, with the failure reason landing in the daemon
log. Like the openmetrics e2e it starts a live daemon, so it runs outside the
`nix flake check` sandbox.

Both e2e scripts take `logoscore` and `lgpm` from `LOGOSCORE_BIN` / `LGPM_BIN`,
else from `PATH`. Neither is a flake input, so build them first. This builds
their latest commit; CI pins the tested revs in
[`.github/workflows/ci.yml`](../.github/workflows/ci.yml).

```bash
export LOGOSCORE_BIN=$(nix build --no-link --print-out-paths \
  github:logos-co/logos-logoscore-cli)/bin/logoscore
export LGPM_BIN=$(nix build --no-link --print-out-paths \
  github:logos-co/logos-package-manager#cli)/bin/lgpm
nix run .#standalone-e2e
```

The C++ integration layer covers the same API in-process
(`integration_create_node_then_node_info`,
`integration_create_node_invalid_config_fails`); the e2e additionally validates
the real `logoscore` call path.

## OpenMetrics

`metrics.cpp` covers the JSON schema returned by `collectMetrics()` (field
shape, counter `_total` suffix, expected `libp2p_*` series). Real consumer
behavior is covered by the e2e below, which runs this module under `logoscore`
with the real `openmetrics` module, scrapes `/metrics`, and asserts our series.
It starts a live `logoscore` daemon, so it runs outside the `nix flake check`
sandbox — as its own CI step and locally via:

```bash
nix run .#openmetrics-e2e
```

It needs `LOGOSCORE_BIN` and `LGPM_BIN` set, as the standalone e2e above does.
