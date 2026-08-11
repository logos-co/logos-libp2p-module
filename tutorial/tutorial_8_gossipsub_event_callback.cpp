/// # Tutorial 8: GossipSub - Event Callback Messages
///
/// In the [previous tutorial](tutorial_7_gossipsub.md), Node B waited for
/// messages by calling `gossipsubNextMessage()`. That polling style is
/// simple and direct, but event-driven applications often prefer callbacks.
///
/// In this tutorial we'll:
///   - Register an `emitEvent` callback before subscribing
///   - Subscribe two nodes to the same topic
///   - Publish a message from one node
///   - Receive it on the other node through the callback
///
/// ## How Callback Reception Works
///
/// The module emits a `"gossipsubMessage"` event when a subscribed node
/// receives a GossipSub message. The event payload is JSON with the topic
/// and message data.
///
/// Register the callback before subscribing. The subscription path snapshots
/// the callback used by worker threads, so registering it late can miss
/// messages.
///
/// Every delivered message also lands in the per-topic queue that
/// `gossipsubNextMessage()` drains, which a callback-only application never
/// reads. Set `{ "gossipsubQueueMaxBytes": 0 }` in the module config to skip
/// that queue; the `"gossipsubMessage"` event still fires.
///
/// -----------

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "plugin.h"

int main()
{
    printf("=== Tutorial 8: GossipSub - Event Callback Messages ===\n\n");

    setLogLevel("fatal");

/// ## Step 1: Create two nodes with GossipSub enabled
    Libp2pModuleOptions optsA, optsB;
    optsA.addrs = {"/ip4/127.0.0.1/tcp/9600"};
    optsA.mountGossipsub = true;

    optsB.addrs = {"/ip4/127.0.0.1/tcp/9601"};
    optsB.mountGossipsub = true;

    Libp2pModuleImpl nodeA(optsA);
    Libp2pModuleImpl nodeB(optsB);

    StdLogosResult startARes = nodeA.start();
    if (!startARes.success) {
        fprintf(stderr, "Node A failed: %s\n", startARes.error.c_str());
        return 1;
    }
    StdLogosResult startBRes = nodeB.start();
    if (!startBRes.success) {
        fprintf(stderr, "Node B failed: %s\n", startBRes.error.c_str());
        return 1;
    }
    printf("Both nodes started\n");

/// ## Step 2: Connect the nodes
    StdLogosResult infoARes = nodeA.peerInfo();
    if (!infoARes.success) {
        fprintf(stderr, "Failed to get Node A info: %s\n",
                infoARes.error.c_str());
        return 1;
    }
    auto infoA = infoARes.value;
    std::string peerIdA = infoA["peerId"].get<std::string>();
    std::vector<std::string> addrsA;
    for (const auto& a : infoA["addrs"])
        addrsA.push_back(a.get<std::string>());

    printf("Connecting Node B to Node A...\n");
    StdLogosResult connectRes = nodeB.connectPeer(peerIdA, addrsA, 5000);
    if (!connectRes.success) {
        fprintf(stderr, "Failed to connect: %s\n",
                connectRes.error.c_str());
        return 1;
    }
    printf("Connected\n");

/// ## Step 3: Register Node A's event callback
///
/// The callback listens for `"gossipsubMessage"` events, parses the JSON
/// payload, stores the received message, and wakes the waiting main thread.
    std::mutex eventMtx;
    std::condition_variable eventCv;
    bool messageReceived = false;
    std::string eventMessage;

    nodeA.emitEvent = [&](const std::string& name,
                          const std::string& data) {
        if (name != "gossipsubMessage")
            return;

        auto j = nlohmann::json::parse(data);
        std::string eventTopic = j["topic"].get<std::string>();
        std::string msg = j["data"].get<std::string>();
        printf("Node A callback received on topic \"%s\": \"%s\"\n",
               eventTopic.c_str(), msg.c_str());

        {
            std::lock_guard<std::mutex> lock(eventMtx);
            messageReceived = true;
            eventMessage = msg;
        }
        eventCv.notify_one();
    };

/// ## Step 4: Both nodes subscribe to a topic
    std::string topic = "chat-room-1";
    printf("Node A subscribing to topic: \"%s\"\n", topic.c_str());
    StdLogosResult subscribeARes = nodeA.gossipsubSubscribe(topic);
    if (!subscribeARes.success) {
        fprintf(stderr, "Node A subscribe failed: %s\n",
                subscribeARes.error.c_str());
        return 1;
    }

    printf("Node B subscribing to topic: \"%s\"\n", topic.c_str());
    StdLogosResult subscribeBRes = nodeB.gossipsubSubscribe(topic);
    if (!subscribeBRes.success) {
        fprintf(stderr, "Node B subscribe failed: %s\n",
                subscribeBRes.error.c_str());
        return 1;
    }

/// ## Step 5: Wait for the GossipSub mesh to form
    printf("Waiting for GossipSub mesh to form...\n");
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

/// ## Step 6: Publish a message
///
/// Node B publishes so that Node A receives the message through its
/// registered callback.
    std::string payload = "Hello from Node B via event callback!";
    printf("\nNode B publishing: \"%s\"\n", payload.c_str());
    printf("  Topic: \"%s\"\n", topic.c_str());

    StdLogosResult publishRes = nodeB.gossipsubPublish(topic, payload);
    if (!publishRes.success) {
        fprintf(stderr, "Publish failed: %s\n", publishRes.error.c_str());
        return 1;
    }
    printf("Message published!\n");

/// ## Step 7: Wait for the callback
///
/// The main thread waits on a condition variable while the callback handles
/// asynchronous delivery.
    bool eventReceived = false;
    std::string msgCopy;
    {
        std::unique_lock<std::mutex> lock(eventMtx);
        eventCv.wait_for(lock, std::chrono::seconds(3),
                         [&] { return messageReceived; });
        eventReceived = messageReceived;
        msgCopy = eventMessage;
    }

    if (!eventReceived) {
        fprintf(stderr, "Node A did not receive the event-driven message\n");
        return 1;
    }

    if (msgCopy == payload) {
        printf("Event-driven message verified!\n");
    } else {
        fprintf(stderr,
                "Event-driven message differs (expected: \"%s\", got: \"%s\")\n",
                payload.c_str(), msgCopy.c_str());
        return 1;
    }

/// ## Step 8: Unsubscribe and clean up
    printf("\nUnsubscribing...\n");
    if (!nodeA.gossipsubUnsubscribe(topic).success) {
        fprintf(stderr, "Node A unsubscribe failed\n");
        return 1;
    }
    if (!nodeB.gossipsubUnsubscribe(topic).success) {
        fprintf(stderr, "Node B unsubscribe failed\n");
        return 1;
    }

    nodeA.stop();
    nodeB.stop();

    printf("\n=== Tutorial 8 Complete ===\n");

    return 0;
}

/// ## Key Takeaways
///
///   - Use `emitEvent` for event-driven GossipSub message reception
///   - Register the callback before subscribing
///   - The `"gossipsubMessage"` event payload contains the topic and data
///   - Set `gossipsubQueueMaxBytes` to 0 so the unread poll queue costs nothing
///   - A condition variable is one way to bridge callback delivery back to
///     synchronous example code
///   - Always unsubscribe and stop cleanly

/// ## Run tutorial
///
/// ```bash
/// ./build/tutorial/tutorial_8_gossipsub_event_callback
/// ```
