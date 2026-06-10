#include <cstdio>
#include <random>
#include <memory>
#include <vector>
#include "plugin.h"

int main()
{
    const int NUM_NODES = 5;

    std::vector<std::unique_ptr<Libp2pModuleImpl>> nodes;
    std::vector<std::pair<std::string, std::vector<std::string>>> infos;

    std::vector<std::string> mixPrivKeys;
    std::vector<std::string> mixPubKeys;
    std::vector<std::string> libp2pPubKeys;

    printf("Starting mix nodes...\n");

    for (int i = 0; i < NUM_NODES; ++i) {
        nodes.push_back(std::make_unique<Libp2pModuleImpl>(Libp2pModuleOptions{}));

        if (!nodes[i]->start().success) {
            fprintf(stderr, "Node failed to start\n");
            return 1;
        }

        auto info = nodes[i]->peerInfo().value;
        std::string peerId = info["peerId"].get<std::string>();
        std::vector<std::string> addrs;
        for (const auto& a : info["addrs"]) addrs.push_back(a.get<std::string>());
        infos.push_back({peerId, addrs});

        printf("Node started: %s\n", peerId.c_str());
        printf("Address: %s\n", addrs[0].c_str());

        auto privRes = nodes[i]->mixGeneratePrivKey();
        auto pubRes = nodes[i]->mixPublicKey(privRes.value.get<std::string>());
        auto lpPubRes = nodes[i]->publicKey();

        mixPrivKeys.push_back(privRes.value.get<std::string>());
        mixPubKeys.push_back(pubRes.value.get<std::string>());
        libp2pPubKeys.push_back(lpPubRes.value.get<std::string>());

        if (!nodes[i]->mixSetNodeInfo(addrs[0], mixPrivKeys[i]).success) {
            fprintf(stderr, "mixSetNodeInfo failed\n");
            return 1;
        }

        if (!nodes[i]->mixRegisterDestReadBehavior(
                "/ipfs/ping/1.0.0", LIBP2P_MIX_READ_EXACTLY, 32).success) {
            fprintf(stderr, "mixRegisterDestReadBehavior failed\n");
            return 1;
        }
    }

    printf("All nodes started\n");
    printf("Populating mix pools\n");

    for (int i = 0; i < NUM_NODES; ++i) {
        for (int j = 0; j < NUM_NODES; ++j) {
            if (i == j) continue;

            if (!nodes[i]->mixNodepoolAdd(
                    infos[j].first, infos[j].second[0],
                    mixPubKeys[j], libp2pPubKeys[j]).success) {
                fprintf(stderr, "mixNodepoolAdd failed\n");
                return 1;
            }
        }
    }

    printf("Mix pools ready\n");
    printf("Dialing through mix\n");

    auto dialRes = nodes[0]->mixDialWithReply(
        infos[4].first, infos[4].second[0],
        "/ipfs/ping/1.0.0", 1, 0);

    if (!dialRes.success) {
        fprintf(stderr, "Mix dial failed\n");
        return 1;
    }

    uint64_t streamId = dialRes.value.get<uint64_t>();
    printf("Mix stream opened: %llu\n", (unsigned long long)streamId);

    std::string payload(32, '\0');
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, 255);
    for (size_t i = 0; i < payload.size(); ++i)
        payload[i] = static_cast<char>(dist(gen));

    printf("Sending payload\n");

    if (!nodes[0]->streamWrite(streamId, payload).success) {
        fprintf(stderr, "Stream write failed\n");
        return 1;
    }

    auto readRes = nodes[0]->streamReadExactly(streamId, payload.size());
    if (!readRes.success) {
        fprintf(stderr, "Stream read failed\n");
        return 1;
    }

    std::string received = readRes.value.get<std::string>();
    printf("Received bytes: %zu\n", received.size());

    nodes[0]->streamClose(streamId);
    nodes[0]->streamRelease(streamId);

    printf("Stream closed\n");

    for (int i = 0; i < NUM_NODES; ++i) {
        nodes[i]->stop();
    }

    printf("Done\n");

    return 0;
}
