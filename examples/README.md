# Examples

This directory contains small programs demonstrating common libp2p
use cases using the `logos-libp2p-module`.

## Available examples

- [Kademlia](kademlia.cpp) — Demonstrates basic [Kademlia DHT](https://github.com/libp2p/specs/blob/master/kad-dht/README.md) operations such as:
  - `PutValue`
  - `GetValue`
  - `GetProviders`
  - `StartProviding`

## Building
Enter the development shell and configure cmake

```bash
nix develop
cmake -B build -S .
```

Build the module and all examples:
```bash
cmake --build build -j
```

## Running an example
After building, run the executable from the build directory:

```bash
./build/examples/example_kademlia
```

