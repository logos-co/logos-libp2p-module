# Tests

This directory contains Qt-based tests for the `logos-libp2p-module`.

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

The `logoscore` and `lgpm` binaries are not vendored into this flake (their
upstream locks would bloat ours). The e2e picks them up from `$PATH`, or from
`LOGOSCORE_BIN` / `LGPM_BIN` if set:

```bash
LOGOSCORE_BIN=$(nix build --no-link --print-out-paths \
  github:logos-co/logos-logoscore-cli)/bin/logoscore \
LGPM_BIN=$(nix build --no-link --print-out-paths \
  'github:logos-co/logos-package-manager#cli')/bin/lgpm \
  nix run .#openmetrics-e2e
```
