# Tests

This directory contains Qt-based tests for the `logos-libp2p-module`.

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