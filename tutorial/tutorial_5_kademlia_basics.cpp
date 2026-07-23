/// # Tutorial 5: Kademlia DHT Basics
///
/// Kademlia is a Distributed Hash Table (DHT) that lets peers store and
/// retrieve values without a central server. The `logos-libp2p-module`
/// exposes this through the `kadPutValue()` and `kadGetValue()` functions.
///
/// In this tutorial we'll:
///   - Start two nodes that bootstrap to each other
///   - Store a value from one node
///   - Retrieve it from the other node
///
/// ## How Kademlia works in libp2p
///
/// Each node in the DHT maintains a routing table of peers closest to
/// certain "keys" (hashes). When you call `kadPutValue(key, value)`:
///   1. The key is hashed and the node finds the closest peers in its
///      routing table
///   2. The value is sent to those peers for storage
///
/// When you call `kadGetValue(key, quorum)`:
///   1. The key is hashed and closest peers are looked up
///   2. The value is requested from them
///   3. The quorum parameter controls how many peers must return the same
///      value for the key before the lookup succeeds
///
/// ## Bootstrap nodes
///
/// New nodes need at least one bootstrap peer to join the DHT. In this
/// tutorial, Node A acts as the bootstrap for Node B.
#include <cstdio>
#include <string>
#include <vector>
#include "plugin.h"

int main()
{
    printf("=== Tutorial 5: Kademlia DHT Basics ===\n\n");

/// ## Step 1: Create two nodes
///
/// We need at least two nodes to demonstrate DHT operations. In a
/// real network, one node would be a well-known bootstrap peer.
///
/// > **Important**: Kademlia is mounted by default (`mountKad: true`).
/// > We explicitly enable it in our options.
    Libp2pModuleOptions optsA;
    optsA.addrs = {"/ip4/127.0.0.1/tcp/9390"};
    optsA.mountKad = true;
    // Node A is the bootstrap — no special config needed, just its address.

    Libp2pModuleImpl nodeA(optsA);
    printf("Starting Node A (bootstrap)...\n");
    if (!nodeA.start().success) {
        fprintf(stderr, "Node A failed to start\n");
        return 1;
    }

    // Get Node A's peer info for bootstrapping Node B
    auto infoARes = nodeA.peerInfo();
    if (!infoARes.success) {
        fprintf(stderr, "Failed to get Node A info: %s\n",
                infoARes.error.c_str());
        return 1;
    }
    auto infoA = infoARes.value;
    std::string peerIdA = infoA["peerId"].get<std::string>();
    std::vector<std::string> addrsA;
    for (const auto& a : infoA["addrs"])
        addrsA.push_back(a.get<std::string>());
    printf("Node A peer ID: %s\n", peerIdA.c_str());

    // Now create Node B with Node A as its bootstrap
    Libp2pModuleOptions optsB;
    optsB.addrs = {"/ip4/127.0.0.1/tcp/9391"};
    optsB.mountKad = true;
    optsB.bootstrapNodes = {{peerIdA, addrsA}};

    Libp2pModuleImpl nodeB(optsB);
    printf("Starting Node B (with Node A as bootstrap)...\n");
    if (!nodeB.start().success) {
        fprintf(stderr, "Node B failed to start\n");
        return 1;
    }

    // Connect Node B to Node A so they can participate in the DHT
    printf("Connecting Node B to Node A...\n");
    if (!nodeB.connectPeer(peerIdA, addrsA, 5000).success) {
        fprintf(stderr, "Failed to connect\n");
        return 1;
    }
    printf("Nodes connected\n");

/// ## Step 2: Store a value in the DHT
///
/// Node A puts a key-value pair into the DHT. The key is a string,
/// and the value is also a string.
    std::string key = "greeting";
    std::string value = "Hello from the DHT!";

    printf("\nNode A putting value into DHT...\n");
    printf("  Key: \"%s\"\n", key.c_str());
    printf("  Value: \"%s\"\n", value.c_str());

    if (!nodeA.kadPutValue(key, value).success) {
        fprintf(stderr, "PutValue failed\n");
        return 1;
    }
    printf("Value stored!\n");

/// ## Step 3: Retrieve the value from Node B
///
/// Node B fetches the value from the DHT. The `quorum` parameter
/// controls the consistency level:
///   - `0` = default (usually 1 matching response is enough)
///   - `1` = wait for at least 1 peer to return the value for the key
///   - Higher values = require at least that many peers to return the same
///     value for the key, which improves consistency but can be slower
    printf("\nNode B fetching value from DHT...\n");
    auto getRes = nodeB.kadGetValue(key, 1);
    if (!getRes.success) {
        fprintf(stderr, "GetValue failed: %s\n",
                getRes.error.c_str());
        return 1;
    }

    if (!getRes.value.is_string()) {
        fprintf(stderr, "GetValue returned a non-string value\n");
        return 1;
    }
    std::string received = base64Decode(getRes.value.get<std::string>());

    if (received.empty()) {
        fprintf(stderr, "Node B did not find the value\n");
        return 1;
    }

    printf("Node B received: \"%s\"\n", received.c_str());
    if (received != value) {
        fprintf(stderr, "Value mismatch (expected: \"%s\", got: \"%s\")\n",
                value.c_str(), received.c_str());
        return 1;
    }
    printf("Value matches!\n");

/// ## Step 4: Clean up
    nodeA.stop();
    nodeB.stop();

    printf("\n=== Tutorial 5 Complete ===\n");

    return 0;
}

/// ## Key Takeaways
///
///   - Kademlia DHT is enabled by default (`mountKad: true`)
///   - Bootstrap nodes help new peers join the DHT
///   - `kadPutValue(key, value)` stores a value
///   - `kadGetValue(key, quorum)` retrieves it
///   - Values may take a moment to propagate after storing

/// ## Run tutorial
///
/// ```bash
/// ./build/tutorial/tutorial_5_kademlia_basics
/// ```
