#include <logos_test.h>
#include <plugin.h>
#include <vector>
#include <memory>
#include <thread>
#include <chrono>
#include "test_helpers.h"

LOGOS_TEST(gossipsub_subscribe_and_publish) {
    Libp2pModuleImpl nodeA;
    Libp2pModuleImpl nodeB;

    LOGOS_ASSERT_TRUE(nodeA.start().success);
    LOGOS_ASSERT_TRUE(nodeB.start().success);

    auto [peerIdA, addrsA] = getPeerInfoPair(nodeA);
    LOGOS_ASSERT_TRUE(nodeB.connectPeer(peerIdA, addrsA, 500).success);

    std::string topic = "integration-topic";
    LOGOS_ASSERT_TRUE(nodeB.gossipsubSubscribe(topic).success);
    LOGOS_ASSERT_TRUE(nodeA.gossipsubSubscribe(topic).success);
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    std::string payload = "Hello from Node A";
    LOGOS_ASSERT_TRUE(nodeA.gossipsubPublish(topic, payload).success);

    auto res = nodeB.gossipsubNextMessage(topic);
    LOGOS_ASSERT_TRUE(res.success);
    LOGOS_ASSERT_TRUE(res.value.get<std::string>() == payload);

    LOGOS_ASSERT_TRUE(nodeA.stop().success);
    LOGOS_ASSERT_TRUE(nodeB.stop().success);
}

LOGOS_TEST(gossipsub_multiple_subscribers) {
    Libp2pModuleImpl nodeA;
    LOGOS_ASSERT_TRUE(nodeA.start().success);

    std::string topic = "multi-topic";
    LOGOS_ASSERT_TRUE(nodeA.gossipsubSubscribe(topic).success);

    const int NUM_SUBS = 3;
    std::vector<std::unique_ptr<Libp2pModuleImpl>> subscribers;

    for (int i = 0; i < NUM_SUBS; ++i) {
        subscribers.emplace_back(std::make_unique<Libp2pModuleImpl>());
        LOGOS_ASSERT_TRUE(subscribers.back()->start().success);

        auto [peerIdA, addrsA] = getPeerInfoPair(nodeA);
        LOGOS_ASSERT_TRUE(subscribers.back()->connectPeer(peerIdA, addrsA, 500).success);
        LOGOS_ASSERT_TRUE(subscribers.back()->gossipsubSubscribe(topic).success);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    std::string payload = "Broadcast message";
    LOGOS_ASSERT_TRUE(nodeA.gossipsubPublish(topic, payload).success);

    for (auto& sub : subscribers) {
        auto res = sub->gossipsubNextMessage(topic);
        LOGOS_ASSERT_TRUE(res.success);
        LOGOS_ASSERT_TRUE(res.value.get<std::string>() == payload);
        LOGOS_ASSERT_TRUE(sub->stop().success);
    }

    LOGOS_ASSERT_TRUE(nodeA.stop().success);
}

LOGOS_TEST(gossipsub_subscribe_unsubscribe) {
    Libp2pModuleImpl node;
    LOGOS_ASSERT_TRUE(node.start().success);

    std::string topic = "temp-topic";
    LOGOS_ASSERT_TRUE(node.gossipsubSubscribe(topic).success);
    LOGOS_ASSERT_TRUE(node.gossipsubUnsubscribe(topic).success);

    std::string payload = "Test after unsubscribe";
    LOGOS_ASSERT_TRUE(node.gossipsubPublish(topic, payload).success);

    LOGOS_ASSERT_FALSE(node.gossipsubNextMessage(topic, 500).success);

    LOGOS_ASSERT_TRUE(node.stop().success);
}

LOGOS_TEST(gossipsub_binary_payload) {
    Libp2pModuleImpl nodeA;
    Libp2pModuleImpl nodeB;

    LOGOS_ASSERT_TRUE(nodeA.start().success);
    LOGOS_ASSERT_TRUE(nodeB.start().success);

    auto [peerIdA, addrsA] = getPeerInfoPair(nodeA);
    LOGOS_ASSERT_TRUE(nodeB.connectPeer(peerIdA, addrsA, 500).success);

    std::string topic = "binary-topic";
    LOGOS_ASSERT_TRUE(nodeB.gossipsubSubscribe(topic).success);
    LOGOS_ASSERT_TRUE(nodeA.gossipsubSubscribe(topic).success);
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    std::string payload;
    payload += '\x01';
    payload += '\x00';
    payload += '\x02';
    payload += '\x00';
    payload += '\x03';
    LOGOS_ASSERT_EQ(payload.size(), size_t(5));

    LOGOS_ASSERT_TRUE(nodeA.gossipsubPublish(topic, payload).success);

    auto res = nodeB.gossipsubNextMessage(topic);
    LOGOS_ASSERT_TRUE(res.success);
    std::string received = res.value.get<std::string>();

    LOGOS_ASSERT_EQ(received.size(), size_t(5));
    LOGOS_ASSERT_TRUE(received == payload);

    LOGOS_ASSERT_TRUE(nodeA.stop().success);
    LOGOS_ASSERT_TRUE(nodeB.stop().success);
}
