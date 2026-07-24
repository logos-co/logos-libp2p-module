/// # Tutorial 10: Peer Store Management
///
/// The **peer store** is libp2p's database of known peers. It maps peer IDs
/// to their known addresses, protocols, public keys, and metadata.
///
/// A well-maintained peer store helps:
///   - Reconnect to known peers without re-discovery
///   - Track protocol capabilities of known peers
///   - Maintain a persistent view of the network
///
/// In this tutorial we'll:
///   - List known peers
///   - Get detailed peer info
///   - Manually add peers to the store
///   - Update addresses and protocols
///   - Delete peers from the store
///
/// -----------

#include <cstdio>
#include <string>
#include <vector>
#include "plugin.h"

int main()
{
    printf("=== Tutorial 10: Peer Store Management ===\n\n");

/// ## Step 1: Create two nodes
///
/// The peer store is automatically available on any started node.
/// We use one main node and one remote peer so the main node's peerstore
/// contains real peer data after a connection is established.
    Libp2pModuleOptions opts;
    opts.addrs = {"/ip4/127.0.0.1/tcp/9790"};

    Libp2pModuleOptions remoteOpts;
    remoteOpts.addrs = {"/ip4/127.0.0.1/tcp/9791"};

    Libp2pModuleImpl node(opts);
    Libp2pModuleImpl remote(remoteOpts);

    if (!node.start().success) {
        fprintf(stderr, "Node failed to start\n");
        return 1;
    }
    if (!remote.start().success) {
        fprintf(stderr, "Remote node failed to start\n");
        return 1;
    }

    auto infoRes = node.peerInfo();
    if (!infoRes.success) {
        fprintf(stderr, "Failed to get node info: %s\n",
                infoRes.error.c_str());
        return 1;
    }
    auto info = infoRes.value;
    std::string nodePeerId = info["peerId"].get<std::string>();
    printf("Node started, peer ID: %s\n", nodePeerId.c_str());

    auto remoteInfoRes = remote.peerInfo();
    if (!remoteInfoRes.success) {
        fprintf(stderr, "Failed to get remote node info: %s\n",
                remoteInfoRes.error.c_str());
        return 1;
    }
    auto remoteInfo = remoteInfoRes.value;
    std::string remotePeerId = remoteInfo["peerId"].get<std::string>();
    std::vector<std::string> remoteAddrs;
    for (const auto& a : remoteInfo["addrs"]) {
        remoteAddrs.push_back(a.get<std::string>());
    }
    printf("Remote node started, peer ID: %s\n", remotePeerId.c_str());

/// ## Step 2: List known peers
///
/// Initially, the peer store may be empty because no remote peers have
/// been discovered or added yet.
/// `peerstoreGetPeers()` returns a JSON array of peer IDs.
    printf("\nListing known peers...\n");
    auto peersRes = node.peerstoreGetPeers();
    if (!peersRes.success) {
        fprintf(stderr, "Failed to list peers: %s\n",
                peersRes.error.c_str());
        return 1;
    }
    printf("Found %zu known peer(s):\n", peersRes.value.size());
    for (const auto& p : peersRes.value) {
        printf("  %s\n", p.get<std::string>().c_str());
    }

/// ## Step 3: Connect a remote peer and get detailed peer info
///
/// `peerstoreGetPeerInfo()` returns a rich JSON object with addresses,
/// protocols, and the public key.
    printf("\nConnecting to remote peer...\n");
    if (!node.connectPeer(remotePeerId, remoteAddrs, 5000).success) {
        fprintf(stderr, "Failed to connect to remote peer\n");
        return 1;
    }
    printf("Connected to remote peer\n");

    printf("\nGetting remote peer info from the store:\n");
    auto remoteStoreInfo = node.peerstoreGetPeerInfo(remotePeerId);
    if (!remoteStoreInfo.success) {
        fprintf(stderr, "Failed to get remote peer info: %s\n",
                remoteStoreInfo.error.c_str());
        return 1;
    }

    printf("  Peer ID: %s\n",
           remoteStoreInfo.value["peerId"].get<std::string>().c_str());
    printf("  Public key: %s\n",
           remoteStoreInfo.value["publicKey"].get<std::string>().c_str());

    printf("  Addresses:\n");
    for (const auto& a : remoteStoreInfo.value["addrs"]) {
        printf("    %s\n", a.get<std::string>().c_str());
    }

    printf("  Protocols:\n");
    for (const auto& p : remoteStoreInfo.value["protocols"]) {
        printf("    %s\n", p.get<std::string>().c_str());
    }
    if (!ownInfo.value.is_object()) {
        fprintf(stderr, "peerstoreGetPeerInfo returned a non-object value\n");
        return 1;
    }
    auto& j = ownInfo.value;
    printf("  Peer ID: %s\n",
           j["peerId"].get<std::string>().c_str());

    printf("  Addresses:\n");
    if (j.contains("addrs") && j["addrs"].is_array()) {
        for (const auto& a : j["addrs"]) {
            printf("    %s\n", a.get<std::string>().c_str());
        }
    }

    printf("  Protocols:\n");
    if (j.contains("protocols") && j["protocols"].is_array()) {
        for (const auto& p : j["protocols"]) {
            printf("    %s\n", p.get<std::string>().c_str());
        }
    }

    printf("  Public key: %s\n",
           j["publicKey"].get<std::string>().c_str());

/// ## Step 4: Manually add a peer to the store
///
/// You can manually register peers in the store. This is useful for
/// peers discovered through out-of-band methods (e.g. a config file,
/// QR code, or DNS).
    printf("\nManually adding a peer to the store...\n");

    std::string manualPeerId =
        "16Uiu2HAkzM1nzU99VxRqzF9CjZAqjF3MZHQ2oRENL7SNgP1d8pRy";
    std::vector<std::string> manualAddrs = {
        "/ip4/192.168.1.100/tcp/9000",
        "/ip4/10.0.0.50/tcp/9001",
    };
    std::vector<std::string> manualProtos = {
        "/ipfs/ping/1.0.0",
        "/examples/echo/1.0.0",
    };

    if (!node.peerstoreAddPeer(manualPeerId, manualAddrs, manualProtos)
             .success) {
        fprintf(stderr, "Failed to add peer\n");
        return 1;
    }
    printf("Peer added: %s\n", manualPeerId.c_str());

/// Verify it was added:
    printf("\nVerifying: listing peers again...\n");
    auto peersAfter = node.peerstoreGetPeers();
    if (!peersAfter.success) {
        fprintf(stderr, "Failed to list peers after add: %s\n",
                peersAfter.error.c_str());
        return 1;
    }
    printf("Now have %zu known peer(s)\n", peersAfter.value.size());
    if (peersAfter.value.size() != 2) {
        fprintf(stderr, "Expected to have 2 peers in the peer store after manually adding one");
        return 1;
    }
    if (!peersAfter.value.is_array()) {
        fprintf(stderr, "peerstoreGetPeers returned a non-array value\n");
        return 1;
    }
    printf("Now have %zu known peer(s)\n", peersAfter.value.size());

/// ## Step 5: Update peer info
///
/// `peerstoreSetPeerAddresses()` to update a peer's stored data.
    printf("\nUpdating peer addresses...\n");
    std::vector<std::string> updatedAddrs = {
        "/ip4/192.168.1.100/tcp/9002",
        "/dns4/peer.example.com/tcp/9000",
    };
    if (!node.peerstoreSetPeerAddresses(manualPeerId, updatedAddrs)
             .success) {
        fprintf(stderr, "Failed to update addresses\n");
        return 1;
    }
    printf("Addresses updated\n");

    // Check the updated info:
    auto updatedInfo = node.peerstoreGetPeerInfo(manualPeerId);
    if (!updatedInfo.success) {
        fprintf(stderr, "Failed to get updated peer info: %s\n",
                updatedInfo.error.c_str());
        return 1;
    }
    printf("Updated addresses:\n");
    for (const auto& a : updatedInfo.value["addrs"]) {
        printf("  %s\n", a.get<std::string>().c_str());
    }

/// ## Step 6: Delete a peer from the store
///
/// `peerstoreDeletePeer()` removes a peer and all its metadata.
    printf("\nDeleting peer from store...\n");
    if (!node.peerstoreDeletePeer(manualPeerId).success) {
        fprintf(stderr, "Failed to delete peer\n");
        return 1;
    }
    printf("Peer deleted\n");

/// Verify deletion:
    auto peersFinal = node.peerstoreGetPeers();
    if (!peersFinal.success) {
        fprintf(stderr, "Failed to list peers after delete: %s\n",
                peersFinal.error.c_str());
        return 1;
    }
    printf("Now have %zu known peer(s)\n",
           peersFinal.value.size());
    for (const auto& p : peersFinal.value) {
        printf("  %s\n", p.get<std::string>().c_str());
    }

/// ## Step 7: Clean up
   node.stop();
   remote.stop();

    printf("\n=== Tutorial 10 Complete ===\n");

    return 0;
}

/// ## Key Takeaways
///
///   - The peer store is a built-in database of known peers
///   - `peerstoreGetPeers()` lists all known peer IDs
///   - `peerstoreGetPeerInfo()` returns detailed peer metadata
///   - `peerstoreAddPeer()` adds peers manually (with addresses + protocols)
///   - `peerstoreSetPeerAddresses()` update existing entries
///   - `peerstoreDeletePeer()` removes peers
///   - A populated peer store speeds up reconnection and reduces
///     network overhead

/// ## Run tutorial
///
/// ```bash
/// ./build/tutorial/tutorial_10_peerstore
/// ```
