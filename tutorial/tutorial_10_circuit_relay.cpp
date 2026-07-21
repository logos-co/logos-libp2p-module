/// # Tutorial 10: Circuit Relay – Connecting Through Firewalls
///
/// Not all peers are directly reachable. Some are behind NAT, firewalls,
/// or have no public IP. Circuit Relay solves this by having a
/// **relay node** forward traffic between peers.
///
/// ## The Three-Node Pattern
///
/// Circuit relay uses three roles:
///
/// ```
///   Client  <-->  Relay  <-->  Destination
///   (B)            (R)          (A)
/// ```
///
/// - **Destination** (A) — The peer behind NAT that wants to be reachable.
///   It connects to the relay and reserves a slot.
/// - **Relay** (R) — A publicly reachable node that forwards traffic.
/// - **Client** (B) — A peer that wants to connect to A through R.
///
/// ## How it works
///
/// 1. The Destination connects to the Relay and calls
///    `circuitRelayReserve()` to request a relay slot.
/// 2. The Relay confirms the reservation and returns relay addresses.
/// 3. The Client dials the Destination via the Relay using
///    `dialCircuitRelay()`.
/// 4. The Relay transparently forwards stream data between them.
///
/// > **Note**: Circuit relay adds latency and bandwidth overhead on the
/// > relay node. Use it only when direct connections are impossible.
#include <cstdio>
#include <chrono>
#include <cstdint>
#include <thread>
#include <string>
#include <vector>
#include "plugin.h"

int main()
{
    printf("=== Tutorial 10: Circuit Relay ===\n\n");

/// ## Step 1: Create three nodes
///
/// - **Relay** (port 9890): publicly reachable, `circuitRelay: true`
/// - **Destination** (port 9891): behind NAT
/// - **Client** (port 9892): wants to connect to Destination
///
/// The Relay and Destination must be connected before the reservation.
/// The Client connects to the Relay before dialing the Destination.

    // Relay node — must have circuitRelay enabled
    Libp2pModuleOptions optsRelay;
    optsRelay.addrs = {"/ip4/127.0.0.1/tcp/9890"};
    optsRelay.circuitRelay = true;

    // Destination node (behind NAT)
    Libp2pModuleOptions optsDest;
    optsDest.addrs = {"/ip4/127.0.0.1/tcp/9891"};
    optsDest.circuitRelayClient = true;

    // Client node
    Libp2pModuleOptions optsClient;
    optsClient.addrs = {"/ip4/127.0.0.1/tcp/9892"};
    optsClient.circuitRelayClient = true;

    Libp2pModuleImpl relay(optsRelay);
    Libp2pModuleImpl dest(optsDest);
    Libp2pModuleImpl client(optsClient);

    printf("Starting nodes...\n");

    if (!relay.start().success) {
        fprintf(stderr, "Relay failed\n");
        return 1;
    }

    if (!dest.start().success) {
        fprintf(stderr, "Destination failed\n");
        return 1;
    }

    if (!client.start().success) {
        fprintf(stderr, "Client failed\n");
        return 1;
    }
    printf("All three nodes started\n");

/// ## Step 2: Get node addresses
    auto infoRelay = relay.peerInfo().value;
    std::string relayPeerId = infoRelay["peerId"].get<std::string>();
    std::vector<std::string> relayAddrs;
    for (const auto& a : infoRelay["addrs"])
        relayAddrs.push_back(a.get<std::string>());

    auto infoDest = dest.peerInfo().value;
    std::string destPeerId = infoDest["peerId"].get<std::string>();

    printf("Relay peer ID: %s\n", relayPeerId.c_str());
    printf("Destination peer ID: %s\n", destPeerId.c_str());

/// ## Step 3: Destination connects to the Relay and reserves a slot
///
/// The Destination must connect to the Relay first, then call
/// `circuitRelayReserve()` to request a reservation. The relay
/// returns the addresses the Destination can be reached at (via relay).
    printf("\nDestination connecting to relay...\n");
    if (!dest.connectPeer(relayPeerId, relayAddrs, 5000).success) {
        fprintf(stderr, "Destination failed to connect to relay\n");
        return 1;
    }
    printf("Destination connected to relay\n");

    printf("Destination requesting relay reservation...\n");
    auto reserveRes = dest.circuitRelayReserve(relayPeerId, relayAddrs);
    if (!reserveRes.success) {
        fprintf(stderr, "Relay reservation failed: %s\n",
                reserveRes.error.c_str());
        return 1;
    }

    printf("Relay reservation successful!\n");
    if (reserveRes.value.is_array()) {
        printf("Relay addresses:\n");
        for (const auto& addr : reserveRes.value) {
            printf("  %s\n", addr.get<std::string>().c_str());
        }
    }

/// ## Step 4: Client connects to the Relay, then dials Destination
///
/// The Client connects to the Relay (same as any other peer), then
/// uses `dialCircuitRelay()` to reach the Destination through the Relay.
    printf("\nClient connecting to relay...\n");
    if (!client.connectPeer(relayPeerId, relayAddrs, 5000).success) {
        fprintf(stderr, "Client failed to connect to relay\n");
        return 1;
    }
    printf("Client connected to relay\n");

/// Now the Client dials the Destination's peer ID through the relay.
/// The multiaddr comes from the reservation response, with `/p2p-circuit`
/// appended so libp2p routes the stream through the Relay.
    printf("Client dialing destination through relay...\n");

    std::string relayDialAddr;
    if (reserveRes.value.is_array() && !reserveRes.value.empty()) {
        relayDialAddr = reserveRes.value[0].get<std::string>() + "/p2p-circuit";
    } else {
        fprintf(stderr, "Reservation did not return relay addresses\n");
        return 1;
    }

/// For circuit relay, we use a well-known protocol to test connectivity.
/// The ping protocol works well for this.
    auto dialRes = client.dialCircuitRelay(
        destPeerId,
        relayDialAddr,
        "/ipfs/ping/1.0.0");

    if (!dialRes.success) {
        fprintf(stderr, "Circuit relay dial failed: %s\n",
                dialRes.error.c_str());
        printf("\nNote: Circuit relay may need configuration tuning.\n");
        printf("Ensure the relay node has circuitRelay=true and the\n");
        printf("destination has connected and reserved a slot.\n");
    } else {
        uint64_t streamId = dialRes.value.get<uint64_t>();
        printf("Circuit relay stream opened! id: %llu\n",
               (unsigned long long)streamId);

        // Send a ping through the relay:
        std::string payload(32, '\0');
        for (int i = 0; i < 32; ++i) payload[i] = static_cast<char>(i);

        if (!client.streamWrite(streamId, payload).success) {
            fprintf(stderr, "Write failed\n");
        } else {
            printf("Sent %zu bytes through relay\n", payload.size());
        }

        client.streamClose(streamId);
        client.streamRelease(streamId);
    }

/// ## Step 5: Clean up
    relay.stop();
    dest.stop();
    client.stop();

    printf("\n=== Tutorial 10 Complete ===\n");

    return 0;
}

/// ## Key Takeaways
///
///   - Circuit Relay allows connecting peers behind NAT/firewalls
///   - Three roles: Destination (relayed), Relay (forwarder), Client
///   - `circuitRelay: true` enables relay mode on a node
///   - Destination calls `circuitRelayReserve()` to request a slot
///   - Client calls `dialCircuitRelay()` with dest peer ID + address
///   - Relay overhead should be considered; prefer direct connections
///     when possible
///   - The relay address returned by reserve may be needed by the
///     client for some configurations
///
/// This concludes the tutorial series! You now have a solid
/// foundation for building peer-to-peer applications with
/// `logos-libp2p-module`.

/// ## Run tutorial
///
/// Build the module (one time):
/// ```bash
/// nix develop
/// ./tutorial/build_tutorials.sh
/// ```
///
/// Run this tutorial:
/// ```bash
/// ./build/tutorial_10_circuit_relay
/// ```
