#include <logos_test.h>
#include <plugin.h>
#include <vector>
#include <memory>
#include <set>
#include <thread>
#include <chrono>
#include <string>
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

    auto res = nodeB.gossipsubNextMessage(topic, 1000);
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
        auto res = sub->gossipsubNextMessage(topic, 1000);
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

// The per-topic queue has no capacity bound: publish many before draining any,
// then drain them all to confirm nothing is dropped.
LOGOS_TEST(gossipsub_queue_buffers_many_messages_unbounded) {
    Libp2pModuleImpl node;
    LOGOS_ASSERT_TRUE(node.start().success);

    std::string topic = "queue-capacity-topic";
    LOGOS_ASSERT_TRUE(node.gossipsubSubscribe(topic).success);

    const int NUM_MSGS = 50;
    for (int i = 0; i < NUM_MSGS; ++i) {
        LOGOS_ASSERT_TRUE(
            node.gossipsubPublish(topic, "msg-" + std::to_string(i)).success);
    }

    // Every published message is buffered and drainable; delivery order is not
    // asserted, only that none were dropped.
    std::set<std::string> drained;
    for (int i = 0; i < NUM_MSGS; ++i) {
        auto res = node.gossipsubNextMessage(topic, 2000);
        LOGOS_ASSERT_TRUE(res.success);
        drained.insert(res.value.get<std::string>());
    }
    LOGOS_ASSERT_EQ(drained.size(), size_t(NUM_MSGS));
    for (int i = 0; i < NUM_MSGS; ++i) {
        LOGOS_ASSERT_TRUE(drained.count("msg-" + std::to_string(i)) == 1);
    }

    // Nothing left once the buffered backlog is drained.
    LOGOS_ASSERT_FALSE(node.gossipsubNextMessage(topic, 200).success);

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

    auto res = nodeB.gossipsubNextMessage(topic, 1000);
    LOGOS_ASSERT_TRUE(res.success);
    std::string received = res.value.get<std::string>();

    LOGOS_ASSERT_EQ(received.size(), size_t(5));
    LOGOS_ASSERT_TRUE(received == payload);

    LOGOS_ASSERT_TRUE(nodeA.stop().success);
    LOGOS_ASSERT_TRUE(nodeB.stop().success);
}
