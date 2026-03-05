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
