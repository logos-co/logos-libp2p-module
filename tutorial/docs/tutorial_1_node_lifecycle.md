# Tutorial 1: Creating and Starting a libp2p Node

Welcome to the first `logos-libp2p-module` tutorial!

This tutorial will guide you through the basics of creating, configuring,
starting, and stopping a libp2p node using the Logos libp2p module.

## Before You Start

Make sure you can build the `logos-libp2p-module` project. See the
project's `README.md` for build instructions using Nix or CMake.

## What is libp2p?

[libp2p](https://libp2p.io/) is a modular networking stack for building
peer-to-peer applications. It provides transport-agnostic connectivity,
peer identity, stream multiplexing, secure channels, and content routing
— everything you need to build a decentralized network.

The `logos-libp2p-module` wraps nim-libp2p's C bindings into a C++ class
called `Libp2pModuleImpl` that you can embed directly into your application.

## Step 1: Include the module header and instantiate a node

Every program starts by including the module's single public header:
```cpp
#include <cstdio>
#include <string>
#include "plugin.h"

```

The main class we work with is `Libp2pModuleImpl`. Let's create one with
default options:
```cpp
int main()
{
    // Create a libp2p node with default configuration.
    // By default it listens on 127.0.0.1 with a random port (tcp/0).
    Libp2pModuleImpl node;

    printf("Node object created (not yet started)\n");

```

## Step 2: Start the node

Calling `start()` creates the libp2p context, binds the configured
address, and begins accepting connections.

```cpp
    if (!node.start().success) {
        fprintf(stderr, "Failed to start node\n");
        return 1;
    }

    printf("Node started successfully!\n");

```

## Step 3: Query node information

Once the node is running, we can inspect its identity and
network addresses using `peerInfo()`.

```cpp
    auto info = node.peerInfo();
    if (info.success) {
        // Extract the peer ID — a unique cryptographic identifier
        std::string peerId = info.value["peerId"].get<std::string>();
        printf("Peer ID: %s\n", peerId.c_str());

        // List all multiaddresses the node is listening on
        printf("Listening addresses:\n");
        for (const auto& addr : info.value["addrs"]) {
            printf("  %s\n", addr.get<std::string>().c_str());
        }
    }

```

We can also query specific fields using `getNodeInfo()`:

```cpp
    auto version = node.getNodeInfo("Version");
    if (version.success) {
        printf("Module version: %s\n",
               version.value.get<std::string>().c_str());
    }

    auto peerIdResult = node.getNodeInfo("PeerId");
    if (peerIdResult.success) {
        printf("Peer ID (via getNodeInfo): %s\n",
               peerIdResult.value.get<std::string>().c_str());
    }

```

## Step 4: Stop the node

Always clean up by stopping the node when you're done.

```cpp
    node.stop();
    printf("Node stopped\n");

    return 0;
}

```

## Summary

In this tutorial you learned how to:
  - Create a `Libp2pModuleImpl` instance
  - Start and stop a libp2p node
  - Query peer identity and listening addresses
  - Retrieve module metadata

## Run tutorial

```bash
./build/tutorial/tutorial_1_node_lifecycle
```
---

<table width="100%">
  <tr>
<td width="50%"></td>
<td width="50%" align="right"><a href="tutorial_2_custom_config.md">Custom Node Configuration &rarr;</a></td>
  </tr>
</table>
