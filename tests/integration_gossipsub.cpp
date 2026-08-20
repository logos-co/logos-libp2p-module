#include <logos_test.h>
#include <plugin.h>
#include <atomic>
#include <vector>
#include <memory>
#include <set>
#include <thread>
#include <chrono>
#include <string>
#include "test_helpers.h"

namespace {
// -1 when the topic has no drop series yet.
int64_t droppedCount(Libp2pModuleImpl& node, const std::string& topic) {
    // Iterating collectMetrics()["metrics"] walks the temporary after it dies.
    const auto payload = node.collectMetrics();
    for (const auto& m : payload["metrics"]) {
        if (m.value("name", std::string{}) != "libp2p_module_gossipsub_queue_dropped_total") {
            continue;
        }
        if (m["labels"].value("topic", std::string{}) != topic) {
            continue;
        }
        return static_cast<int64_t>(m["value"].get<double>());
    }
    return -1;
}

// Delivery is asynchronous, so poll to a deadline instead of sleeping a fixed
// amount and hoping a loaded runner kept up.
void awaitDropped(Libp2pModuleImpl& node, const std::string& topic, int64_t expected) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (droppedCount(node, topic) != expected &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    LOGOS_ASSERT_EQ(droppedCount(node, topic), expected);
}

// Publishes `sent` payloads of `payloadSize` bytes each, then checks that the
// bound kept exactly the `kept` oldest of them.
void assertDropsNewest(const Libp2pModuleOptions& opts, const std::string& topic,
                       size_t payloadSize, int sent, int kept) {
    Libp2pModuleImpl node(opts);
    LOGOS_ASSERT_TRUE(node.start().success);
    LOGOS_ASSERT_TRUE(node.gossipsubSubscribe(topic).success);

    auto marker = [](int i) { return std::to_string(i) + ":"; };
    for (int i = 0; i < sent; ++i) {
        LOGOS_ASSERT_TRUE(
            node.gossipsubPublish(topic, marker(i) + std::string(payloadSize, 'x')).success);
    }
    awaitDropped(node, topic, sent - kept);

    for (int i = 0; i < kept; ++i) {
        auto res = node.gossipsubNextMessage(topic, 1000);
        LOGOS_ASSERT_TRUE(res.success);
        LOGOS_ASSERT_TRUE(res.value.get<std::string>().rfind(marker(i), 0) == 0);
    }
    LOGOS_ASSERT_FALSE(node.gossipsubNextMessage(topic, 200).success);

    // The counter is exported as a Prometheus counter, so unsubscribing must
    // not reset it.
    LOGOS_ASSERT_TRUE(node.gossipsubUnsubscribe(topic).success);
    LOGOS_ASSERT_EQ(droppedCount(node, topic), int64_t(sent - kept));

    LOGOS_ASSERT_TRUE(node.stop().success);
}
}

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

LOGOS_TEST(gossipsub_unsubscribe_releases_queue) {
    Libp2pModuleImpl node;
    LOGOS_ASSERT_TRUE(node.start().success);

    std::string topic = "released-topic";
    LOGOS_ASSERT_TRUE(node.gossipsubSubscribe(topic).success);
    LOGOS_ASSERT_TRUE(node.gossipsubPublish(topic, "buffered before unsubscribe").success);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    LOGOS_ASSERT_TRUE(node.gossipsubUnsubscribe(topic).success);
    LOGOS_ASSERT_FALSE(node.gossipsubNextMessage(topic, 500).success);

    LOGOS_ASSERT_TRUE(node.stop().success);
}

// Publish many below the cap before draining any, then drain them all to
// confirm the bound does not evict anything it should keep.
LOGOS_TEST(gossipsub_queue_buffers_up_to_cap) {
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

LOGOS_TEST(gossipsub_queue_drops_newest_over_message_bound) {
    Libp2pModuleOptions opts;
    opts.gossipsubQueueMaxMessages = 4;
    assertDropsNewest(opts, "message-bound-topic", 8, 12, 4);
}

// 402-byte payloads: two fit in 1024, the third overshoots.
LOGOS_TEST(gossipsub_queue_drops_newest_over_byte_bound) {
    Libp2pModuleOptions opts;
    opts.gossipsubQueueMaxBytes = 1024;
    assertDropsNewest(opts, "byte-bound-topic", 400, 10, 2);
}

// The byte bound holds on an empty queue too: a payload larger than the bound
// is dropped and counted, and the event still fires.
LOGOS_TEST(gossipsub_queue_drops_oversized_message_when_empty) {
    Libp2pModuleOptions opts;
    opts.gossipsubQueueMaxBytes = 64;
    Libp2pModuleImpl node(opts);
    std::atomic<int> delivered{0};
    node.emitEvent = [&](const std::string& name, const std::string&) {
        if (name == "gossipsubMessage") ++delivered;
    };
    LOGOS_ASSERT_TRUE(node.start().success);

    std::string topic = "oversized-topic";
    LOGOS_ASSERT_TRUE(node.gossipsubSubscribe(topic).success);
    LOGOS_ASSERT_TRUE(node.gossipsubPublish(topic, std::string(4096, 'y')).success);

    awaitDropped(node, topic, 1);
    LOGOS_ASSERT_FALSE(node.gossipsubNextMessage(topic, 200).success);
    LOGOS_ASSERT_EQ(delivered.load(), 1);

    LOGOS_ASSERT_TRUE(node.stop().success);
}

// Either bound at 0 skips the backlog while the event still fires.
LOGOS_TEST(gossipsub_queue_disabled_when_a_bound_is_zero) {
    Libp2pModuleOptions byMessages;
    byMessages.gossipsubQueueMaxMessages = 0;
    Libp2pModuleOptions byBytes;
    byBytes.gossipsubQueueMaxBytes = 0;

    for (const auto& opts : {byMessages, byBytes}) {
        Libp2pModuleImpl node(opts);
        std::atomic<int> delivered{0};
        node.emitEvent = [&](const std::string& name, const std::string&) {
            if (name == "gossipsubMessage") ++delivered;
        };
        LOGOS_ASSERT_TRUE(node.start().success);

        std::string topic = "no-queue-topic";
        LOGOS_ASSERT_TRUE(node.gossipsubSubscribe(topic).success);
        LOGOS_ASSERT_TRUE(node.gossipsubPublish(topic, "never buffered").success);

        LOGOS_ASSERT_FALSE(node.gossipsubNextMessage(topic, 500).success);
        LOGOS_ASSERT_EQ(delivered.load(), 1);
        LOGOS_ASSERT_TRUE(node.stop().success);
    }
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
