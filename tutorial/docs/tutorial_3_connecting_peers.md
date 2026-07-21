# Tutorial 3: Connecting Peers and Exchanging Data

Now that we can create and configure nodes, let's make them talk to
each other! In this tutorial we'll:

  - Create two nodes
  - Connect one to the other
  - Dial a protocol and open a stream
  - Exchange data over the stream

## How libp2p Connections Work

A libp2p connection is established in two steps:

1. **Connect** — Establish a transport-level connection to a remote peer,
   identified by its Peer ID and a multiaddress.

2. **Dial** — Negotiate a protocol on top of the connection and open a
   bidirectional stream. The stream is what you actually read from and
   write to.

Streams in `logos-libp2p-module` are identified by a numeric `streamId`
that you get back from `dial()` and pass to read/write functions.

## Stream Lifecycle

A stream must follow this lifecycle:
  1. `dial()` — Open a stream (returns a `streamId`)
  2. `streamWrite()` / `streamWriteLp()` — Send data
  3. `streamReadLp()` / `streamReadExactly()` — Receive data
  4. `streamClose()` or `streamCloseWithEOF()` — Close gracefully
  5. `streamRelease()` — Free server-side resources
```cpp
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>
#include "plugin.h"

int main()
{
    printf("=== Tutorial 3: Connecting Peers ===\n\n");

```

## Step 1: Create and start two nodes

We create two nodes. Node A listens on port 9190, Node B on port 9191.
For simplicity, both mount the built-in `/ipfs/ping/1.0.0` protocol
(enabled by default — no extra config needed).
```cpp
    Libp2pModuleOptions optsA;
    optsA.addrs = {"/ip4/127.0.0.1/tcp/9190"};

    Libp2pModuleOptions optsB;
    optsB.addrs = {"/ip4/127.0.0.1/tcp/9191"};
    // Node B needs to know about node A to connect, but we'll pass
    // that info after starting both nodes.

    Libp2pModuleImpl nodeA(optsA);
    Libp2pModuleImpl nodeB(optsB);

    if (!nodeA.start().success) {
        fprintf(stderr, "Node A failed to start\n");
        return 1;
    }
    printf("Node A started\n");

    if (!nodeB.start().success) {
        fprintf(stderr, "Node B failed to start\n");
        return 1;
    }
    printf("Node B started\n");

```

## Step 2: Get Node A's address info

Node B needs to know where to find Node A. We get Node A's
peer ID and listening addresses from `peerInfo()`.
```cpp
    auto infoA = nodeA.peerInfo();
    if (!infoA.success) {
        fprintf(stderr, "Failed to get node A info\n");
        return 1;
    }

    std::string peerIdA = infoA.value["peerId"].get<std::string>();
    std::vector<std::string> addrsA;
    for (const auto& a : infoA.value["addrs"]) {
        addrsA.push_back(a.get<std::string>());
    }

    printf("Node A peer ID: %s\n", peerIdA.c_str());
    printf("Node A addresses:\n");
    for (const auto& a : addrsA) {
        printf("  %s\n", a.c_str());
    }

```

## Step 3: Connect Node B to Node A

`connectPeer()` establishes the transport connection. The timeout
parameter is in milliseconds.
```cpp
    printf("\nConnecting Node B to Node A...\n");
    if (!nodeB.connectPeer(peerIdA, addrsA, 5000).success) {
        fprintf(stderr, "Failed to connect\n");
        return 1;
    }
    printf("Connected!\n");

```

## Step 4: List connected peers

We can verify the connection by listing connected peers on each node.
The direction parameter `0` means "all connected peers".
```cpp
    auto peersA = nodeA.connectedPeers(0);
    if (peersA.success) {
        printf("\nNode A's connected peers:\n");
        for (const auto& p : peersA.value) {
            printf("  %s\n", p.get<std::string>().c_str());
        }
    }

    auto peersB = nodeB.connectedPeers(0);
    if (peersB.success) {
        printf("Node B's connected peers:\n");
        for (const auto& p : peersB.value) {
            printf("  %s\n", p.get<std::string>().c_str());
        }
    }

```

## Step 5: Dial the ping protocol and exchange data

Now let's use the Ping protocol — a simple built-in protocol where
the client sends a payload and the server echoes it back.

`dial()` opens a stream on the remote peer for a specific protocol.
It returns the `streamId` we use for subsequent operations.
```cpp
    printf("\nDialing /ipfs/ping/1.0.0 on Node A...\n");
    auto dialRes = nodeB.dial(peerIdA, "/ipfs/ping/1.0.0");
    if (!dialRes.success) {
        fprintf(stderr, "Dial failed: %s\n", dialRes.error.c_str());
        return 1;
    }

    uint64_t streamId = dialRes.value.get<uint64_t>();
    printf("Stream opened, id: %llu\n", (unsigned long long)streamId);

```

Write a 32-byte ping payload:
```cpp
    std::string payload(32, '\0');
    for (int i = 0; i < 32; ++i) {
        payload[i] = static_cast<char>(i);
    }

    printf("Sending %zu bytes...\n", payload.size());
    if (!nodeB.streamWrite(streamId, payload).success) {
        fprintf(stderr, "Write failed\n");
        return 1;
    }

```

Read the echo (32 bytes back):
```cpp
    auto readRes = nodeB.streamReadExactly(streamId, 32);
    if (!readRes.success) {
        fprintf(stderr, "Read failed: %s\n", readRes.error.c_str());
        return 1;
    }

    std::string reply = base64Decode(readRes.value.get<std::string>());

```

Verify the echo matches:
```cpp
    if (reply == payload) {
        printf("Ping successful — received matching echo back!\n");
    } else {
        fprintf(stderr, "Ping payload mismatch\n");
        return 1;
    }

```

## Step 6: Clean up the stream and nodes

Always close and release streams when done:
```cpp
    nodeB.streamClose(streamId);
    nodeB.streamRelease(streamId);

    nodeA.stop();
    nodeB.stop();

    printf("\n=== Tutorial 3 Complete ===\n");

    return 0;
}

```

## Run tutorial

```bash
./build/tutorial/tutorial_3_connecting_peers
```
---

<table width="100%">
  <tr>
<td width="50%" align="left"><a href="tutorial_2_custom_config.md">&larr; Custom Node Configuration</a></td>
<td width="50%" align="right"><a href="tutorial_4_custom_protocol.md">Custom Protocol Handlers &rarr;</a></td>
  </tr>
</table>
