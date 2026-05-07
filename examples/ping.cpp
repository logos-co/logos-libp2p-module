#include <cstdio>
#include "plugin.h"

int main()
{
    Libp2pModuleImpl nodeA;

    printf("Starting node A...\n");
    if (!nodeA.start().success) {
        fprintf(stderr, "Node A failed to start\n");
        return 1;
    }

    auto infoRes = nodeA.peerInfo();
    auto infoA = infoRes.value;
    std::string peerIdA = infoA["peerId"].get<std::string>();
    std::vector<std::string> addrsA;
    for (const auto& a : infoA["addrs"]) addrsA.push_back(a.get<std::string>());

    Libp2pModuleImpl nodeB(Libp2pModuleOptions{
        .bootstrapNodes = { {peerIdA, addrsA} }
    });

    printf("Starting node B...\n");
    if (!nodeB.start().success) {
        fprintf(stderr, "Node B failed to start\n");
        return 1;
    }

    printf("Nodes started\n");

    if (!nodeB.connectPeer(peerIdA, addrsA, 500).success) {
        fprintf(stderr, "Failed to connect peers\n");
        return 1;
    }

    printf("Connected\n");

    auto dialRes = nodeB.dial(peerIdA, "/ipfs/ping/1.0.0");
    if (!dialRes.success) {
        fprintf(stderr, "Dial failed\n");
        return 1;
    }

    uint64_t streamId = dialRes.value.get<uint64_t>();

    const int PING_SIZE = 32;
    std::string payload(PING_SIZE, '\0');
    for (int i = 0; i < PING_SIZE; ++i)
        payload[i] = static_cast<char>(i);

    printf("Sending ping...\n");
    if (!nodeB.streamWrite(streamId, payload).success) {
        fprintf(stderr, "Write failed\n");
        return 1;
    }

    auto readRes = nodeB.streamReadExactly(streamId, PING_SIZE);
    if (!readRes.success) {
        fprintf(stderr, "Read failed\n");
        return 1;
    }

    std::string reply = readRes.value.get<std::string>();

    if (reply.size() != payload.size()) {
        fprintf(stderr, "Ping response size mismatch\n");
        return 1;
    }

    if (reply != payload) {
        fprintf(stderr, "Ping payload mismatch\n");
        return 1;
    }

    printf("Ping successful — payload verified\n");

    nodeB.streamClose(streamId);
    nodeB.streamRelease(streamId);

    nodeA.stop();
    nodeB.stop();

    printf("Done\n");

    return 0;
}
