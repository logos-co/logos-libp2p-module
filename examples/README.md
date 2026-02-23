# Examples

This directory contains small programs demonstrating common libp2p
use cases using the `logos-libp2p-module`.

## Available examples
- [Ping](ping.cpp) — Minimal libp2p connectivity example.
  Demonstrates:
  - Starting two libp2p nodes
  - Connecting peers
  - Opening a stream using `/ipfs/ping/1.0.0`
  - Sending and receiving data over a stream

- [Kademlia](kademlia.cpp) — Demonstrates basic [Kademlia DHT](https://github.com/libp2p/specs/blob/master/kad-dht/README.md) operations such as:
  - `PutValue`
  - `GetValue`
  - `GetProviders`
  - `StartProviding`

- [Mix](mix.cpp) — Demonstrates usage of the [Mix](https://github.com/vacp2p/nim-libp2p/blob/master/docs/protocols_mix.md) protocol for private messaging:
  - Generating private and public keys
  - Setting node info
  - Populating mix pools
  - Dialing through mix

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
./build/examples/example_ping
./build/examples/example_kademlia
./build/examples/example_mix
```

