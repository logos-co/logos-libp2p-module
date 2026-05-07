#include <cstdio>
#include "plugin.h"

int main()
{
    Libp2pModuleImpl nodeA;

    printf("Starting nodes...\n");

    if (!nodeA.start().success) {
        fprintf(stderr, "Node A failed to start\n");
        return 1;
    }
    auto infoA = nodeA.peerInfo().value;
    std::string peerIdA = infoA["peerId"].get<std::string>();
    std::vector<std::string> addrsA;
    for (const auto& a : infoA["addrs"]) addrsA.push_back(a.get<std::string>());

    Libp2pModuleImpl nodeB(Libp2pModuleOptions{
        .bootstrapNodes = { {peerIdA, addrsA} }
    });

    if (!nodeB.start().success) {
        fprintf(stderr, "Node B failed to start\n");
        return 1;
    }

    printf("Nodes started\n");

    auto recordsRes = nodeB.kadGetRandomRecords();
    if (recordsRes.success && recordsRes.value.is_array() && recordsRes.value.empty()) {
        printf("Node B has no known peers yet\n");
    }

    std::string key = "demo-key";
    std::string value = "Hello from node A";

    printf("Node A putting value into DHT\n");

    if (!nodeA.kadPutValue(key, value).success) {
        fprintf(stderr, "PutValue failed\n");
        return 1;
    }

    printf("Node B fetching value from DHT\n");

    auto getRes = nodeB.kadGetValue(key, 1);
    std::string received = getRes.value.get<std::string>();

    if (received.empty()) {
        printf("Node B did not find value\n");
    } else {
        printf("Node B received: %s\n", received.c_str());
    }

    auto cidRes = nodeA.toCid(key);
    std::string cid = cidRes.value.get<std::string>();

    printf("CID: %s\n", cid.c_str());

    nodeA.kadStartProviding(cid);

    auto provRes = nodeB.kadGetProviders(cid);
    auto providers = provRes.value;
    printf("Providers found by B: %zu\n", providers.size());
    for (const auto& p : providers) {
        printf("Provider: %s\n", p["peerId"].get<std::string>().c_str());
    }

    nodeA.stop();
    nodeB.stop();

    printf("Done\n");

    return 0;
}
