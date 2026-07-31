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
///         Bootstrap
///         /       \
///   Advertiser  Discoverer
/// ```
///
/// -----------

#include <cstdio>
#include <chrono>
#include <map>
#include <thread>
#include <string>
#include <vector>
#include "plugin.h"

int main()
{
    printf("=== Tutorial 9: Service Discovery ===\n\n");

    setLogLevel(LogLevel::Fatal);

/// ## Step 1: Create a bootstrap node
///
/// The bootstrap is a well-known node that all peers connect to.
/// It relays service advertisements and lookup queries.
    Libp2pModuleOptions optsBootstrap;
    optsBootstrap.addrs = {"/ip4/127.0.0.1/tcp/9690"};
    optsBootstrap.mountServiceDiscovery = true;

    Libp2pModuleImpl bootstrap(optsBootstrap);
    StdLogosResult bootstrapStartRes = bootstrap.start();
    if (!bootstrapStartRes.success) {
        fprintf(stderr, "Bootstrap failed: %s\n",
                bootstrapStartRes.error.c_str());
        return 1;
    }
    StdLogosResult bootstrapDiscoStartRes = bootstrap.discoStart();
    if (!bootstrapDiscoStartRes.success) {
        fprintf(stderr, "Bootstrap discoStart failed: %s\n",
                bootstrapDiscoStartRes.error.c_str());
        return 1;
    }

    StdLogosResult bootstrapInfoRes = bootstrap.peerInfo();
    if (!bootstrapInfoRes.success) {
        fprintf(stderr, "Failed to get bootstrap info: %s\n",
                bootstrapInfoRes.error.c_str());
        return 1;
    }
    std::string bootstrapId =
        bootstrapInfoRes.value["peerId"].get<std::string>();
    std::vector<std::string> bootstrapAddrs;
    for (const auto& a : bootstrapInfoRes.value["addrs"]) {
        bootstrapAddrs.push_back(a.get<std::string>());
    }
    printf("Bootstrap node started: %s\n", bootstrapId.c_str());

/// ## Step 2: Create an advertiser node
///
/// This node will advertise a service that others can discover.
    Libp2pModuleOptions optsAdvertiser;
    optsAdvertiser.addrs = {"/ip4/127.0.0.1/tcp/9691"};
    optsAdvertiser.bootstrapNodes = {{bootstrapId, bootstrapAddrs}};
    optsAdvertiser.mountServiceDiscovery = true;

    Libp2pModuleImpl advertiser(optsAdvertiser);
    StdLogosResult advertiserStartRes = advertiser.start();
    if (!advertiserStartRes.success) {
        fprintf(stderr, "Advertiser failed: %s\n",
                advertiserStartRes.error.c_str());
        return 1;
    }
    StdLogosResult advertiserDiscoStartRes = advertiser.discoStart();
    if (!advertiserDiscoStartRes.success) {
        fprintf(stderr, "Advertiser discoStart failed: %s\n",
                advertiserDiscoStartRes.error.c_str());
        return 1;
    }

    // Connect to bootstrap
    StdLogosResult advertiserConnectRes =
        advertiser.connectPeer(bootstrapId, bootstrapAddrs, 5000);
    if (!advertiserConnectRes.success) {
        fprintf(stderr, "Advertiser failed to connect to bootstrap: %s\n",
                advertiserConnectRes.error.c_str());
        return 1;
    }
    printf("Advertiser connected to bootstrap\n");

/// ## Step 3: Create a discoverer node
    Libp2pModuleOptions optsDiscoverer;
    optsDiscoverer.addrs = {"/ip4/127.0.0.1/tcp/9692"};
    optsDiscoverer.bootstrapNodes = {{bootstrapId, bootstrapAddrs}};
    optsDiscoverer.mountServiceDiscovery = true;

    Libp2pModuleImpl discoverer(optsDiscoverer);
    StdLogosResult discovererStartRes = discoverer.start();
    if (!discovererStartRes.success) {
        fprintf(stderr, "Discoverer failed: %s\n",
                discovererStartRes.error.c_str());
        return 1;
    }
    StdLogosResult discovererDiscoStartRes = discoverer.discoStart();
    if (!discovererDiscoStartRes.success) {
        fprintf(stderr, "Discoverer discoStart failed: %s\n",
                discovererDiscoStartRes.error.c_str());
        return 1;
    }

    StdLogosResult discovererConnectRes =
        discoverer.connectPeer(bootstrapId, bootstrapAddrs, 5000);
    if (!discovererConnectRes.success) {
        fprintf(stderr, "Discoverer failed to connect to bootstrap: %s\n",
                discovererConnectRes.error.c_str());
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
    StdLogosResult advertiseRes =
        advertiser.discoStartAdvertising(serviceId, serviceData);
    if (!advertiseRes.success) {
        fprintf(stderr, "discoStartAdvertising failed: %s\n",
                advertiseRes.error.c_str());
        return 1;
    }
    printf("Advertising started\n");

/// ## Step 5: The discoverer registers interest
///
/// Registering interest tells the service discovery system that
/// this peer wants to know about providers of this service.
    printf("Discoverer registering interest in \"%s\"\n", serviceId.c_str());
    StdLogosResult registerInterestRes =
        discoverer.discoRegisterInterest(serviceId);
    if (!registerInterestRes.success) {
        fprintf(stderr, "discoRegisterInterest failed: %s\n",
                registerInterestRes.error.c_str());
        return 1;
    }

/// ## Step 6: Discoverer looks up the service
///
/// The lookup queries the bootstrap for peers advertising this
/// service. It may take a moment for the advertisement to propagate.
    printf("Discoverer looking up \"%s\"...\n", serviceId.c_str());

    constexpr int kLookupAttempts = 10;
    constexpr int kLookupDelayMs = 500;
    StdLogosResult lookupRes;
    for (int i = 0; i < kLookupAttempts; ++i) {
        lookupRes = discoverer.discoLookup(serviceId, serviceData);
        if (!lookupRes.success) {
            fprintf(stderr, "Service lookup failed: %s\n",
                    lookupRes.error.c_str());
            return 1;
        }
        if (!lookupRes.value.empty()) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(kLookupDelayMs));
    }
    if (lookupRes.value.empty()) {
        fprintf(stderr, "Service lookup did not find any providers\n");
        return 1;
    }
    auto records = lookupRes.value;

    printf("Discoverer found %zu provider(s):\n", records.size());
    for (const auto& rec : records) {
        printf("  Peer: %s\n",
               rec["peerId"].get<std::string>().c_str());
        for (const auto& s : rec["services"]) {
            std::string decodedData = base64Decode(s["data"].get<std::string>());
            printf("    Service: %s (data: %s)\n",
                   s["id"].get<std::string>().c_str(),
                   decodedData.c_str());
        }
    }

/// ## Step 7: Random lookup
///
/// You can also discover random peers via `discoRandomLookup()`,
/// which is useful for network exploration.
    printf("\nRandom lookup by advertiser...\n");
    StdLogosResult randomRes = advertiser.discoRandomLookup();
    if (!randomRes.success) {
        fprintf(stderr, "Random lookup failed: %s\n",
                randomRes.error.c_str());
        return 1;
    }
    printf("Found %zu random peer(s):\n", randomRes.value.size());
    for (const auto& rec : randomRes.value) {
        printf("  Peer: %s\n",
               rec["peerId"].get<std::string>().c_str());
    }

/// ## Step 8: Creating and decoding Extended Peer Records (XPR)
///
/// Extended Peer Records are signed records that bundle a peer's
/// addresses and services. They can be shared offline or via
/// side channels.
///
/// Services are a map of service id to its advertised payload. The
/// payload is raw bytes, not text, so it is a `std::vector<uint8_t>`
/// and reaches the record byte for byte — advertise a compressed blob
/// or a protobuf and nothing is mangled on the way in.
    printf("\n--- Extended Peer Records ---\n");

    std::map<std::string, std::vector<uint8_t>> xprServices = {
        {serviceId, {serviceData.begin(), serviceData.end()}},
        {"file-sharing", {'v', '2'}},
    };

    StdLogosResult xpr = advertiser.createXpr({}, xprServices, 0);
    if (!xpr.success) {
        fprintf(stderr, "Failed to create XPR: %s\n", xpr.error.c_str());
        return 1;
    }
    std::string xprStr = xpr.value.get<std::string>();
    printf("Created signed XPR\n");

    // Decode it to verify
    StdLogosResult decoded = advertiser.decodeXpr(xprStr);
    if (!decoded.success) {
        fprintf(stderr, "Failed to decode XPR: %s\n", decoded.error.c_str());
        return 1;
    }
    printf("Decoded XPR:\n");
    printf("  Peer ID: %s\n",
           decoded.value["peerId"].get<std::string>().c_str());
    printf("  Sequence: %llu\n",
           (unsigned long long)decoded.value["seqNo"]
               .get<uint64_t>());
    printf("  Services: %zu\n",
           decoded.value["services"].size());

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
///   - Service Discovery requires a bootstrap node as rendezvous point
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
