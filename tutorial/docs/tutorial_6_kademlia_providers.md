# Tutorial 6: Kademlia Provider Records

In the [previous tutorial](tutorial_5_kademlia_basics.md), we stored and
retrieved raw key-value pairs in the DHT. But what if you want to
advertise that your node **has** a piece of content, rather than
storing the content itself?

This is where **provider records** come in. Instead of putting a value
directly, you announce that your node provides a given Content ID (CID),
and other peers can discover who provides that content.

## Provider Records vs Key-Value Store

| Feature | Key-Value Store | Provider Records |
|---------|----------------|------------------|
| What's stored | The value itself | The list of provider peer IDs |
| Use case | Small configs, peer info | File sharing, content discovery |
| Data size | Unlimited (but impractical for large data) | Metadata only |
| Finding | `kadGetValue(key)` | `kadGetProviders(cid)` |

## Content IDs (CIDs)

CIDs are self-describing content hashes used in IPFS and IPLD. The
`toCid()` function converts a string key into a CID that can be used
with the provider API.

-----------

```cpp
#include <cstdio>
#include <string>
#include <vector>
#include "plugin.h"

int main()
{
    printf("=== Tutorial 6: Kademlia Provider Records ===\n\n");
    
    setLogLevel(LogLevel::Fatal);

```

## Step 1: Create two peers
```cpp
    Libp2pModuleOptions optsA, optsB;
    optsA.addrs = {"/ip4/127.0.0.1/tcp/9490"};
    optsA.mountKad = true;

    Libp2pModuleImpl nodeA(optsA);

    StdLogosResult startARes = nodeA.start();
    if (!startARes.success) {
        fprintf(stderr, "Node A failed: %s\n", startARes.error.c_str());
        return 1;
    }
    
    StdLogosResult infoARes = nodeA.peerInfo();
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
    
    // Bootstrap and connect Node B
    optsB.addrs = {"/ip4/127.0.0.1/tcp/9491"};
    optsB.mountKad = true;
    optsB.bootstrapNodes = {{peerIdA, addrsA}};
    Libp2pModuleImpl nodeB(optsB);
    StdLogosResult startBRes = nodeB.start();
    if (!startBRes.success) {
        fprintf(stderr, "Node B failed: %s\n", startBRes.error.c_str());
        return 1;
    }

    StdLogosResult connectRes = nodeB.connectPeer(peerIdA, addrsA, 5000);
    if (!connectRes.success) {
        fprintf(stderr, "Failed to connect: %s\n",
                connectRes.error.c_str());
        return 1;
    }
    printf("Nodes connected\n");

```

## Step 2: Convert a content key to a CID

The `toCid()` function takes a string and produces a CID (Content ID)
that can be used with Kademlia's provider API.
```cpp
    std::string contentKey = "my-awesome-file.txt";
    printf("Converting \"%s\" to CID...\n", contentKey.c_str());
    StdLogosResult cidRes = nodeA.toCid(contentKey);
    if (!cidRes.success) {
        fprintf(stderr, "Failed to create CID: %s\n",
                cidRes.error.c_str());
        return 1;
    }
    std::string cid = cidRes.value.get<std::string>();
    printf("CID: %s\n", cid.c_str());

```

## Step 3: Node A starts providing the CID

This advertises to the DHT that Node A has this content.
```cpp
    printf("\nNode A starting to provide CID...\n");
    StdLogosResult startProvidingRes = nodeA.kadStartProviding(cid);
    if (!startProvidingRes.success) {
        fprintf(stderr, "kadStartProviding failed: %s\n",
                startProvidingRes.error.c_str());
        return 1;
    }
    printf("Node A is now a provider for: %s\n", cid.c_str());

```

## Step 4: Node B discovers providers

Node B queries the DHT to find who provides this CID.
```cpp
    printf("\nNode B looking up providers...\n");
    StdLogosResult provRes = nodeB.kadGetProviders(cid);
    if (!provRes.success) {
        fprintf(stderr, "kadGetProviders failed: %s\n",
                provRes.error.c_str());
        return 1;
    }

    auto providers = provRes.value;

    printf("Node B found %zu provider(s):\n", providers.size());
    bool foundNodeA = false;
    for (const auto& p : providers) {
        std::string providerId = p["peerId"].get<std::string>();
        if (providerId == peerIdA) {
            foundNodeA = true;
        }
        printf("  Peer: %s\n", providerId.c_str());
        for (const auto& addr : p["addrs"]) {
            printf("    Address: %s\n",
                   addr.get<std::string>().c_str());
        }
    }
    if (!foundNodeA) {
        fprintf(stderr, "Node A was not found as a provider for %s\n",
                cid.c_str());
        return 1;
    }

```

## Step 5: Find a specific node in the DHT

We can also use `kadFindNode()` to locate a specific peer in the
DHT routing table.
```cpp
    printf("\nNode B finding Node A in the DHT...\n");
    StdLogosResult findRes = nodeB.kadFindNode(peerIdA);
    if (!findRes.success) {
        fprintf(stderr, "kadFindNode failed: %s\n",
                findRes.error.c_str());
        return 1;
    }
    printf("Closest peers to Node A:\n");
    for (const auto& p : findRes.value) {
        printf("  %s\n", p.get<std::string>().c_str());
    }

```

## Step 6: Stop providing

When Node A no longer has the content, it can stop advertising.
```cpp
    printf("\nNode A stopping providing...\n");
    if (!nodeA.kadStopProviding(cid).success) {
        fprintf(stderr, "kadStopProviding failed\n");
        return 1;
    }
    printf("Node A stopped providing %s\n", cid.c_str());

```

## Step 7: Clean up
```cpp
    nodeA.stop();
    nodeB.stop();

    printf("\n=== Tutorial 6 Complete ===\n");

    return 0;
}


```

## Key Takeaways

  - Provider records advertise **content availability**, not content itself
  - `toCid()` converts a key string to a CID for use with provider API
  - `kadStartProviding()` / `kadStopProviding()` manage provider announcements
  - `kadGetProviders()` discovers who has content
  - `kadFindNode()` finds peers in the DHT routing table

## Run tutorial

```bash
./build/tutorial/tutorial_6_kademlia_providers
```
---

<p align="center"><a href="tutorial_5_kademlia_basics.md">&larr; Kademlia DHT Basics</a> &nbsp;|&nbsp; <a href="tutorial_7_gossipsub_polling.md">GossipSub - Polling for Messages &rarr;</a></p>
