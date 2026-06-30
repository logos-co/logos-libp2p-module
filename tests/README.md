# Tests

This directory contains Qt-based tests for the `logos-libp2p-module`.

Tests come in two layers:

- **Fast unit layer** (`libp2p_module_unit_tests`) — `unit_config.cpp`,
  `unit_metrics.cpp`, `unit_sync.cpp`. Exercise only header-inline logic
  (config parsing, `Metric` JSON, the await/parse primitives), construct no
  `Libp2pModuleImpl`, and link without `libp2p.so`, so they always build and run.
- **Integration layer** (`libp2p_module_tests`) — everything that drives a real
  node. Built only when `libp2p.so` is found in `../lib`.

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

Run a specific test by name:

```bash
ctest --test-dir build -R async
ctest --test-dir build -R sync
ctest --test-dir build -R integration
```

Or run the test executables directly:

```bash
./build/async
./build/sync
./build/integration
```

## Standalone (logoscore)

`integration_e2e/standalone_e2e.sh` runs this module on its own under a live
`logoscore` daemon and asserts the standalone commands and their outputs:
`createNode` binds the requested port, `start` succeeds, and `getNodeInfo`
returns the expected `Version` / `MyBoundPorts` / `PeerId` / `Multiaddrs`. It
also covers the negative paths — an unknown `getNodeInfo` field and a malformed
`createNode` config are rejected, with the failure reason landing in the daemon
log. Like the openmetrics e2e it starts a live daemon, so it runs outside the
`nix flake check` sandbox:

```bash
nix run .#standalone-e2e
# or against your own builds:
LOGOSCORE_BIN=/path/to/logoscore \
LGPM_BIN=/path/to/lgpm \
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

The `logoscore` and `lgpm` binaries are pinned via flake inputs, so this needs
no prebuilt binaries. To run against your own builds, set `LOGOSCORE_BIN` /
`LGPM_BIN`:

```bash
LOGOSCORE_BIN=/path/to/logoscore \
LGPM_BIN=/path/to/lgpm \
  nix run .#openmetrics-e2e
```
