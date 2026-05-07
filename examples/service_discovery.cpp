#include <cstdio>
#include <thread>
#include <chrono>
#include "plugin.h"

int main()
{
    Libp2pModuleImpl nodeA(Libp2pModuleOptions{ .mountServiceDiscovery = true });
    Libp2pModuleImpl nodeB(Libp2pModuleOptions{ .mountServiceDiscovery = true });

    printf("Starting nodes...\n");

    if (!nodeA.start().success) {
        fprintf(stderr, "Node A failed to start\n");
        return 1;
    }

    if (!nodeB.start().success) {
        fprintf(stderr, "Node B failed to start\n");
        return 1;
    }

    if (!nodeA.discoStart().success) {
        fprintf(stderr, "Node A: discoStart failed\n");
        return 1;
    }

    if (!nodeB.discoStart().success) {
        fprintf(stderr, "Node B: discoStart failed\n");
        return 1;
    }

    auto infoA = nodeA.peerInfo().value;
    std::string peerIdA = infoA["peerId"].get<std::string>();
    std::vector<std::string> addrsA;
    for (const auto& a : infoA["addrs"]) addrsA.push_back(a.get<std::string>());

    if (!nodeB.connectPeer(peerIdA, addrsA, 500).success) {
        fprintf(stderr, "Node B failed to connect to Node A\n");
        return 1;
    }

    std::string serviceId = "demo-service";
    std::string serviceData = "version=1";

    printf("Node A: starting advertising %s\n", serviceId.c_str());
    if (!nodeA.discoStartAdvertising(serviceId, serviceData).success) {
        fprintf(stderr, "Node A: discoStartAdvertising failed\n");
        return 1;
    }

    printf("Node B: starting discovering %s\n", serviceId.c_str());
    if (!nodeB.discoStartDiscovering(serviceId).success) {
        fprintf(stderr, "Node B: discoStartDiscovering failed\n");
        return 1;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    printf("Node B: looking up %s\n", serviceId.c_str());
    auto res = nodeB.discoLookup(serviceId, serviceData);

    if (!res.success) {
        printf("Lookup returned no results\n");
    } else {
        auto records = res.value;
        printf("Node B found %zu peer(s) advertising %s\n", records.size(), serviceId.c_str());
        for (const auto& rec : records) {
            printf("  peer: %s seq: %llu addrs: %zu\n",
                   rec["peerId"].get<std::string>().c_str(),
                   (unsigned long long)rec["seqNo"].get<uint64_t>(),
                   rec["addrs"].size());
        }
    }

    printf("Node A: random lookup\n");
    auto randRes = nodeA.discoRandomLookup();
    if (randRes.success) {
        printf("Random lookup returned %zu peer(s)\n", randRes.value.size());
    }

    printf("Node B: stopping discovering %s\n", serviceId.c_str());
    nodeB.discoStopDiscovering(serviceId);

    printf("Node A: stopping advertising %s\n", serviceId.c_str());
    nodeA.discoStopAdvertising(serviceId);

    nodeA.discoStop();
    nodeB.discoStop();

    nodeA.stop();
    nodeB.stop();

    printf("Done\n");

    return 0;
}
