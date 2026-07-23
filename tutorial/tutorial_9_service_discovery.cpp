/// # Tutorial 9: Service Discovery
///
/// In decentralized networks, peers join and leave dynamically. How do you
/// find a peer that offers a specific service? That's what **Service
/// Discovery** (Disco) is for.
///
/// Service Discovery lets peers:
///   - **Advertise** the services they offer
///   - **Discover** peers offering services they're interested in
///   - **Register interest** to be notified when new providers appear
///   - **Query randomly** to discover the network
///
/// ## Architecture
///
/// Service Discovery uses a **bootstrap node** as a rendezvous point.
/// All peers connect to the bootstrap and register their services there.
/// Looking up a service queries the bootstrap, which returns matching peers.
///
/// ```
///     Bootstrap
///     /       \
///   Advertiser  Discoverer
/// ```
#include <cstdio>
#include <chrono>
#include <thread>
#include <string>
#include <vector>
#include <utility>
#include "plugin.h"

/// Helper to extract peer info from a node:
static std::pair<std::string, std::vector<std::string>> getPeerInfo(
    Libp2pModuleImpl& node)
{
    auto info = node.peerInfo().value;
    std::string peerId = info["peerId"].get<std::string>();
    std::vector<std::string> addrs;
    for (const auto& a : info["addrs"])
        addrs.push_back(a.get<std::string>());
    return {peerId, addrs};
}

/// Helper to retry a lookup with backoff:
static nlohmann::json lookupWithRetry(
    Libp2pModuleImpl& node,
    const std::string& serviceId,
    const std::string& serviceData,
    int attempts,
    int delayMs)
{
    for (int i = 0; i < attempts; ++i) {
        auto res = node.discoLookup(serviceId, serviceData);
        if (res.success && res.value.is_array() && !res.value.empty()) {
            return res.value;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
    }
    return nlohmann::json::array();
}

int main()
{
    printf("=== Tutorial 9: Service Discovery ===\n\n");

/// ## Step 1: Create a bootstrap node
///
/// The bootstrap is a well-known node that all peers connect to.
/// It relays service advertisements and lookup queries.
    Libp2pModuleOptions optsBootstrap;
    optsBootstrap.addrs = {"/ip4/127.0.0.1/tcp/9690"};
    optsBootstrap.mountServiceDiscovery = true;

    Libp2pModuleImpl bootstrap(optsBootstrap);
    if (!bootstrap.start().success) {
        fprintf(stderr, "Bootstrap failed\n");
        return 1;
    }
    if (!bootstrap.discoStart().success) {
        fprintf(stderr, "Bootstrap discoStart failed\n");
        return 1;
    }

    auto [bootstrapId, bootstrapAddrs] = getPeerInfo(bootstrap);
    printf("Bootstrap node started: %s\n", bootstrapId.c_str());

/// ## Step 2: Create an advertiser node
///
/// This node will advertise a service that others can discover.
    Libp2pModuleOptions optsAdvertiser;
    optsAdvertiser.addrs = {"/ip4/127.0.0.1/tcp/9691"};
    optsAdvertiser.bootstrapNodes = {{bootstrapId, bootstrapAddrs}};
    optsAdvertiser.mountServiceDiscovery = true;

    Libp2pModuleImpl advertiser(optsAdvertiser);
    if (!advertiser.start().success) {
        fprintf(stderr, "Advertiser failed\n");
        return 1;
    }
    if (!advertiser.discoStart().success) {
        fprintf(stderr, "Advertiser discoStart failed\n");
        return 1;
    }

    // Connect to bootstrap
    if (!advertiser.connectPeer(bootstrapId, bootstrapAddrs, 5000).success) {
        fprintf(stderr, "Advertiser failed to connect to bootstrap\n");
        return 1;
    }
    printf("Advertiser connected to bootstrap\n");

/// ## Step 3: Create a discoverer node
    Libp2pModuleOptions optsDiscoverer;
    optsDiscoverer.addrs = {"/ip4/127.0.0.1/tcp/9692"};
    optsDiscoverer.bootstrapNodes = {{bootstrapId, bootstrapAddrs}};
    optsDiscoverer.mountServiceDiscovery = true;

    Libp2pModuleImpl discoverer(optsDiscoverer);
    if (!discoverer.start().success) {
        fprintf(stderr, "Discoverer failed\n");
        return 1;
    }
    if (!discoverer.discoStart().success) {
        fprintf(stderr, "Discoverer discoStart failed\n");
        return 1;
    }

    if (!discoverer.connectPeer(bootstrapId, bootstrapAddrs, 5000).success) {
        fprintf(stderr, "Discoverer failed to connect to bootstrap\n");
        return 1;
    }
    printf("Discoverer connected to bootstrap\n");

/// ## Step 4: The advertiser starts advertising a service
///
/// Services are identified by a service ID (string) and can carry
/// arbitrary data (also a string).
    std::string serviceId = "demo-chat-service";
    std::string serviceData = "version=1.0;capacity=100";

    printf("\nAdvertiser advertising: \"%s\"\n", serviceId.c_str());
    if (!advertiser.discoStartAdvertising(serviceId, serviceData).success) {
        fprintf(stderr, "discoStartAdvertising failed\n");
        return 1;
    }
    printf("Advertising started\n");

/// ## Step 5: The discoverer registers interest
///
/// Registering interest tells the service discovery system that
/// this peer wants to know about providers of this service.
    printf("Discoverer registering interest in \"%s\"\n", serviceId.c_str());
    if (!discoverer.discoRegisterInterest(serviceId).success) {
        fprintf(stderr, "discoRegisterInterest failed\n");
        return 1;
    }

/// ## Step 6: Discoverer looks up the service
///
/// The lookup queries the bootstrap for peers advertising this
/// service. It may take a moment for the advertisement to propagate.
    printf("Discoverer looking up \"%s\"...\n", serviceId.c_str());

    constexpr int kLookupAttempts = 10;
    constexpr int kLookupDelayMs = 500;
    auto records = lookupWithRetry(
        discoverer, serviceId, serviceData,
        kLookupAttempts, kLookupDelayMs);

    printf("Discoverer found %zu provider(s):\n", records.size());
    for (const auto& rec : records) {
        printf("  Peer: %s\n",
               rec["peerId"].get<std::string>().c_str());
        for (const auto& s : rec["services"]) {
            printf("    Service: %s (data: %s)\n",
                   s["id"].get<std::string>().c_str(),
                   s["data"].get<std::string>().c_str());
        }
    }

/// ## Step 7: Random lookup
///
/// You can also discover random peers via `discoRandomLookup()`,
/// which is useful for network exploration.
    printf("\nRandom lookup by advertiser...\n");
    auto randomRes = advertiser.discoRandomLookup();
    if (randomRes.success && randomRes.value.is_array()) {
        printf("Found %zu random peer(s):\n", randomRes.value.size());
        for (const auto& rec : randomRes.value) {
            printf("  Peer: %s\n",
                   rec["peerId"].get<std::string>().c_str());
        }
    }

/// ## Step 8: Creating and decoding Extended Peer Records (XPR)
///
/// Extended Peer Records are signed records that bundle a peer's
/// addresses and services. They can be shared offline or via
/// side channels.
    printf("\n--- Extended Peer Records ---\n");

    std::vector<std::pair<std::string, std::string>> xprServices = {
        {serviceId, serviceData},
        {"file-sharing", "v2"},
    };

    auto xpr = advertiser.createXpr({}, xprServices, 0);
    if (!xpr.success) {
        printf("Failed to create XPR: %s\n", xpr.error.c_str());
    } else {
        std::string xprStr = xpr.value.get<std::string>();
        printf("Created signed XPR: %zu bytes\n", xprStr.size());

        // Decode it to verify
        auto decoded = advertiser.decodeXpr(xprStr);
        if (!decoded.success) {
            printf("Failed to decode XPR: %s\n", decoded.error.c_str());
        } else {
            printf("Decoded XPR:\n");
            printf("  Peer ID: %s\n",
                   decoded.value["peerId"].get<std::string>().c_str());
            printf("  Sequence: %llu\n",
                   (unsigned long long)decoded.value["seqNo"]
                       .get<uint64_t>());
            printf("  Services: %zu\n",
                   decoded.value["services"].size());
        }
    }

/// ## Step 9: Clean up — unregister, stop advertising, stop disco
    printf("\nCleaning up...\n");
    discoverer.discoUnregisterInterest(serviceId);
    advertiser.discoStopAdvertising(serviceId);

    discoverer.discoStop();
    advertiser.discoStop();
    bootstrap.discoStop();

    discoverer.stop();
    advertiser.stop();
    bootstrap.stop();

    printf("\n=== Tutorial 9 Complete ===\n");

    return 0;
}


/// ## Key Takeaways
///
///   - Service Discovery requires a bootstrap node as rendezvous
///   - `discoStart()` / `discoStop()` control the discovery service
///   - `discoStartAdvertising()` / `discoStopAdvertising()` manage ads
///   - `discoRegisterInterest()` / `discoUnregisterInterest()` manage subscriptions
///   - `discoLookup()` finds peers offering a service
///   - `discoRandomLookup()` discovers random peers
///   - `createXpr()` / `decodeXpr()` work with signed peer records

/// ## Run tutorial
///
/// ```bash
/// ./build/tutorial/tutorial_9_service_discovery
/// ```