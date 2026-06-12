#include <cstdio>
#include <thread>
#include <chrono>
#include "plugin.h"

int main()
{
    Libp2pModuleImpl nodeA;
    Libp2pModuleImpl nodeB;

    printf("Starting Nodes...\n");
    if (!nodeA.start().success) {
        fprintf(stderr, "Node A failed to start\n");
        return 1;
    }
    if (!nodeB.start().success) {
        fprintf(stderr, "Node B failed to start\n");
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

    std::string topic = "demo-topic";
    printf("Node B subscribing to topic: %s\n", topic.c_str());
    if (!nodeB.gossipsubSubscribe(topic).success) {
        fprintf(stderr, "Node B subscription failed\n");
        return 1;
    }
    printf("Node A subscribing to topic: %s\n", topic.c_str());
    if (!nodeA.gossipsubSubscribe(topic).success) {
        fprintf(stderr, "Node A subscription failed\n");
        return 1;
    }

    printf("Waiting for mesh to form\n");
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    std::string payload = "Hello from Node A via gossipsub!";
    printf("Node A publishing message to topic: %s\n", topic.c_str());
    if (!nodeA.gossipsubPublish(topic, payload).success) {
        fprintf(stderr, "Node A publish failed\n");
        return 1;
    }

    auto res = nodeB.gossipsubNextMessage(topic, 1000);
    if (!res.success) {
        fprintf(stderr, "Node B did not receive any messages\n");
        return 1;
    }
    printf("Node B received: %s\n", res.value.get<std::string>().c_str());

    nodeA.stop();
    nodeB.stop();

    printf("Done\n");

    return 0;
}
