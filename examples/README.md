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
  - Verifying and decoding a signed Extended Peer Record back into its fields (`decodeXpr`)

  `discoLookup` (and `discoRandomLookup`) return a JSON array of peer records. Each record describes one advertiser:

  ```json
  [
    {
      "peerId": "16Uiu2HAm...",
      "seqNo": 1718000000,
      "addrs": ["/ip4/127.0.0.1/tcp/40001"],
      "services": [
        { "id": "demo-service", "data": "version=1" }
      ]
    }
  ]
  ```

  | Field      | Type             | Description                                                        |
  | ---------- | ---------------- | ------------------------------------------------------------------ |
  | `peerId`   | string           | The advertiser's peer ID.                                          |
  | `seqNo`    | unsigned integer | Monotonic sequence number of the peer record; higher is newer.     |
  | `addrs`    | array of strings | The advertiser's multiaddrs.                                       |
  | `services` | array of objects | Services the peer advertises, each `{ "id", "data" }`.             |

  `services[].data` is the opaque payload passed to `discoStartAdvertising`, returned verbatim as a string (empty when advertised without data).

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

For service discovery, a successful run prints the records returned by `discoLookup`:

```text
Starting nodes...
Advertiser: advertising demo-service
Discoverer: registering interest in demo-service
Discoverer: looking up demo-service
Discoverer found 1 peer(s) advertising demo-service
  peer: 16Uiu2HAm... seq: 1718000000
    addr: /ip4/127.0.0.1/tcp/40001
    service: demo-service data: version=1
Discoverer matched the advertiser: 16Uiu2HAm...
Advertiser: random lookup
Random lookup returned 1 peer(s)
Discoverer: unregistering interest in demo-service
Advertiser: stopping advertising demo-service
Done
```
