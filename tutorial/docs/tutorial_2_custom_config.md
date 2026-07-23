# Tutorial 2: Custom Node Configuration

In the [previous tutorial](tutorial_1_node_lifecycle.md), we created a
node with default settings that binds to a random port. In this tutorial,
we'll learn how to configure the node explicitly — setting a fixed port,
changing transports, and passing options via both C++ structs and JSON.

## Understanding Libp2pModuleOptions

The `Libp2pModuleImpl` constructor accepts a `Libp2pModuleOptions` struct
that lets you control every aspect of the node. Here are the most common
configuration fields:

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `addrs` | `vector<string>` | `["/ip4/127.0.0.1/tcp/0"]` | Listen addresses |
| `transport` | `int` | `LIBP2P_TRANSPORT_TCP` | Transport protocol |
| `maxConnections` | `int` | `50` | Max total connections |
| `maxConnsPerPeer` | `int` | `1` | Max connections per peer |
| `mountGossipsub` | `bool` | `true` | Enable GossipSub |
| `mountKad` | `bool` | `true` | Enable Kademlia DHT |
| `mountServiceDiscovery` | `bool` | `true` | Enable service discovery |

## Binding to a fixed port

By default nodes bind to port 0 (random). To listen on a specific port,
provide a custom address in the `addrs` field.
```cpp
#include <cstdio>
#include <string>
#include "plugin.h"

int main()
{
```

## Method 1: Configure via C++ struct

The most explicit way to configure a node is by constructing
`Libp2pModuleOptions` directly and passing it to the constructor.
```cpp
    Libp2pModuleOptions options;

    // Listen on TCP port 9090 on all interfaces
    options.addrs = {"/ip4/0.0.0.0/tcp/9090"};

    // Limit to 10 total connections
    options.maxConnections = 10;

    // Allow up to 2 connections from the same peer
    options.maxConnsPerPeer = 2;

    // We don't need GossipSub or Kademlia for this basic example
    options.mountGossipsub = false;
    options.mountKad = false;
    options.mountServiceDiscovery = false;

    printf("Creating node with custom configuration...\n");
    printf("  Listen address: %s\n", options.addrs[0].c_str());
    printf("  Max connections: %d\n", options.maxConnections);
    printf("  Max per peer: %d\n", options.maxConnsPerPeer);

    Libp2pModuleImpl node(options);

    if (!node.start().success) {
        fprintf(stderr, "Failed to start node\n");
        return 1;
    }

```

Verify that the node is actually listening on the configured port:
```cpp
    auto info = node.peerInfo();
    if (!info.success) {
        fprintf(stderr, "Failed to get node info: %s\n",
                info.error.c_str());
        return 1;
    }

    printf("\nNode is live!\n");
    printf("  Peer ID: %s\n",
           info.value["peerId"].get<std::string>().c_str());
    for (const auto& addr : info.value["addrs"]) {
        printf("  Listening on: %s\n",
               addr.get<std::string>().c_str());
    }

    node.stop();

```

## Method 2: Configure via JSON

When integrating with larger systems (e.g. `logoscore`), you'll
often receive configuration as JSON. The same options can be
supplied via `Libp2pModuleOptions::fromJson()`.
```cpp
    printf("\n--- Method 2: JSON configuration ---\n");

    std::string jsonConfig = R"({
        "addrs": ["/ip4/127.0.0.1/tcp/9091"],
        "maxConnections": 20,
        "mountGossipsub": false,
        "mountKad": false,
        "mountServiceDiscovery": false
    })";

    bool parseOk;
    std::string parseErr;
    auto jsonOpts = Libp2pModuleOptions::fromJson(jsonConfig, parseOk, &parseErr);

    if (!parseOk) {
        fprintf(stderr, "Failed to parse JSON config: %s\n",
                parseErr.c_str());
        return 1;
    }

    printf("Parsed JSON config:\n");
    printf("  Listen address: %s\n", jsonOpts.addrs[0].c_str());
    printf("  Max connections: %d\n", jsonOpts.maxConnections);

    Libp2pModuleImpl nodeFromJson(jsonOpts);

    if (!nodeFromJson.start().success) {
        fprintf(stderr, "Failed to start JSON-configured node\n");
        return 1;
    }

    auto info2 = nodeFromJson.peerInfo();
    if (!info2.success) {
        fprintf(stderr, "Failed to get JSON-configured node info: %s\n",
                info2.error.c_str());
        return 1;
    }

    printf("\nJSON-configured node is live!\n");
    printf("  Peer ID: %s\n",
           info2.value["peerId"].get<std::string>().c_str());
    for (const auto& addr : info2.value["addrs"]) {
        printf("  Listening on: %s\n",
               addr.get<std::string>().c_str());
    }

    nodeFromJson.stop();

    return 0;
}

```

## Exercise: Try different configurations

Experiment with these variations:

```cpp
// QUIC transport instead of TCP
options.transport = LIBP2P_TRANSPORT_QUIC;
options.addrs = {"/ip4/127.0.0.1/udp/9092/quic-v1"};

// Listen on multiple addresses
options.addrs = {
    "/ip4/0.0.0.0/tcp/9093",
    "/ip4/0.0.0.0/tcp/9094"
};
```

## Summary

In this tutorial you learned:
  - How to configure a node with a fixed port
  - How to control connection limits
  - How to pass options via C++ struct or JSON
  - How to verify the node's actual bound addresses

## Run tutorial

```bash
./build/tutorial/tutorial_2_custom_config
```
---

<p align="center"><a href="tutorial_1_node_lifecycle.md">&larr; Creating and Starting a libp2p Node</a> &nbsp;|&nbsp; <a href="tutorial_3_connecting_peers.md">Connecting Peers and Exchanging Data &rarr;</a></p>
