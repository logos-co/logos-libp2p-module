#include <cstdio>
#include <thread>
#include <chrono>
#include <string>
#include <utility>
#include <vector>
#include "plugin.h"

static std::pair<std::string, std::vector<std::string>> peerInfoOf(Libp2pModuleImpl& node)
{
    auto info = node.peerInfo().value;
    std::string peerId = info["peerId"].get<std::string>();
    std::vector<std::string> addrs;
    for (const auto& a : info["addrs"]) addrs.push_back(a.get<std::string>());
    return {peerId, addrs};
}

static void printRecord(const nlohmann::json& rec)
{
    printf("  peer: %s seq: %llu\n",
           rec["peerId"].get<std::string>().c_str(),
           (unsigned long long)rec["seqNo"].get<uint64_t>());
    for (const auto& addr : rec["addrs"]) {
        printf("    addr: %s\n", addr.get<std::string>().c_str());
    }
    for (const auto& svc : rec["services"]) {
        printf("    service: %s data: %s\n",
               svc["id"].get<std::string>().c_str(),
               svc["data"].get<std::string>().c_str());
    }
}

static nlohmann::json lookupUntilFound(Libp2pModuleImpl& node,
                                       const std::string& serviceId,
                                       const std::string& serviceData,
                                       int attempts, int delayMs)
{
    for (int i = 0; i < attempts; ++i) {
        auto res = node.discoLookup(serviceId, serviceData);
        if (res.success && !res.value.empty()) {
            return res.value;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
    }
    return nlohmann::json::array();
}

int main()
{
    printf("Starting nodes...\n");

    Libp2pModuleImpl bootstrap;
    if (!bootstrap.start().success) {
        fprintf(stderr, "Bootstrap node failed to start\n");
        return 1;
    }
    if (!bootstrap.discoStart().success) {
        fprintf(stderr, "Bootstrap node: discoStart failed\n");
        return 1;
    }
    auto [bootstrapId, bootstrapAddrs] = peerInfoOf(bootstrap);

    // connectPeer below forces an immediate connection instead of waiting on background bootstrap.
    Libp2pModuleImpl advertiser(Libp2pModuleOptions{
        .bootstrapNodes = { {bootstrapId, bootstrapAddrs} }
    });
    if (!advertiser.start().success) {
        fprintf(stderr, "Advertiser failed to start\n");
        return 1;
    }
    if (!advertiser.discoStart().success) {
        fprintf(stderr, "Advertiser: discoStart failed\n");
        return 1;
    }
    if (!advertiser.connectPeer(bootstrapId, bootstrapAddrs, 5000).success) {
        fprintf(stderr, "Advertiser failed to connect to bootstrap\n");
        return 1;
    }

    Libp2pModuleImpl discoverer(Libp2pModuleOptions{
        .bootstrapNodes = { {bootstrapId, bootstrapAddrs} }
    });
    if (!discoverer.start().success) {
        fprintf(stderr, "Discoverer failed to start\n");
        return 1;
    }
    if (!discoverer.discoStart().success) {
        fprintf(stderr, "Discoverer: discoStart failed\n");
        return 1;
    }
    if (!discoverer.connectPeer(bootstrapId, bootstrapAddrs, 5000).success) {
        fprintf(stderr, "Discoverer failed to connect to bootstrap\n");
        return 1;
    }

    std::string serviceId = "demo-service";
    std::string serviceData = "version=1";

    printf("Advertiser: advertising %s\n", serviceId.c_str());
    if (!advertiser.discoStartAdvertising(serviceId, serviceData).success) {
        fprintf(stderr, "Advertiser: discoStartAdvertising failed\n");
        return 1;
    }

    printf("Discoverer: registering interest in %s\n", serviceId.c_str());
    if (!discoverer.discoRegisterInterest(serviceId).success) {
        fprintf(stderr, "Discoverer: discoRegisterInterest failed\n");
        return 1;
    }

    printf("Discoverer: looking up %s\n", serviceId.c_str());
    constexpr int lookupAttempts = 20;
    constexpr int lookupDelayMs = 250; // ~5s total to let the advertisement propagate
    auto records = lookupUntilFound(discoverer, serviceId, serviceData, lookupAttempts, lookupDelayMs);

    printf("Discoverer found %zu peer(s) advertising %s\n", records.size(), serviceId.c_str());
    for (const auto& rec : records) {
        printRecord(rec);
    }

    std::string advertiserId = peerInfoOf(advertiser).first;
    bool matched = !records.empty() && records[0]["peerId"].get<std::string>() == advertiserId;
    printf(matched ? "Discoverer matched the advertiser: %s\n"
                   : "Discoverer did not find the advertiser (%s) yet\n",
           advertiserId.c_str());

    printf("Advertiser: random lookup\n");
    auto randRes = advertiser.discoRandomLookup();
    if (randRes.success) {
        printf("Random lookup returned %zu peer(s)\n", randRes.value.size());
    }

    printf("Discoverer: unregistering interest in %s\n", serviceId.c_str());
    discoverer.discoUnregisterInterest(serviceId);

    printf("Advertiser: stopping advertising %s\n", serviceId.c_str());
    advertiser.discoStopAdvertising(serviceId);

    discoverer.discoStop();
    advertiser.discoStop();
    bootstrap.discoStop();

    discoverer.stop();
    advertiser.stop();
    bootstrap.stop();

    printf("Done\n");
    return 0;
}
