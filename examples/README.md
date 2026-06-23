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

- [Gossipsub](gossipsub.cpp) — Demonstrates usage of the [Gossipsub](https://github.com/libp2p/specs/blob/master/pubsub/gossipsub/README.md) protocol for messaging:
  - Connecting to a peer
  - Subscribing to a topic
  - Publishing to that topic
  - Receiving the message on the subscribed topic

- [Service Discovery](service_discovery.cpp) — Demonstrates the Service Discovery API over a multi-node DHT with a bootstrap node:
  - Anchoring a DHT with a bootstrap node
  - Advertising a named service (`discoStartAdvertising`)
  - Registering interest and looking it up from another node (`discoRegisterInterest` / `discoLookup`)
  - Resolving the advertiser's peer record (peerId, seqNo, addrs)
  - Building a signed Extended Peer Record for the node's own services (`createXpr`)

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
./build/examples/example_gossipsub
./build/examples/example_service_discovery
```

