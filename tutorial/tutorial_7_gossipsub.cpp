/// # Tutorial 7: GossipSub – Pub/Sub Messaging
///
/// GossipSub is a pub/sub protocol that lets peers broadcast messages to
/// everyone subscribed to a topic. It's the foundation for many
/// decentralized applications — from chat rooms to blockchain transaction
/// propagation.
///
/// In this tutorial we'll:
///   - Subscribe two nodes to the same topic
///   - Wait for the GossipSub mesh to form
///   - Publish a message from one node
///   - Receive it on the other node
///
/// ## How GossipSub Works
///
/// 1. Peers subscribe to topics by calling `gossipsubSubscribe(topic)`
/// 2. libp2p builds a **mesh** — a set of peer connections per topic
/// 3. When a peer publishes to a topic, the message is forwarded through
///    the mesh to all subscribers
/// 4. Subscribers receive messages via `gossipsubNextMessage(topic, timeout)`
///
/// > **Note**: The GossipSub mesh takes some time to form after both
/// > peers have subscribed. A short delay (1-2 seconds) is usually enough.
#include <cstdio>
#include <chrono>
#include <thread>
#include <string>
#include "plugin.h"

int main()
{
    printf("=== Tutorial 7: GossipSub – Pub/Sub Messaging ===\n\n");

/// ## Step 1: Create two nodes with GossipSub enabled
///
/// GossipSub is enabled by default (`mountGossipsub: true`).
/// We keep it explicit here for clarity.
    Libp2pModuleOptions optsA, optsB;
    optsA.addrs = {"/ip4/127.0.0.1/tcp/9590"};
    optsA.mountGossipsub = true;

    optsB.addrs = {"/ip4/127.0.0.1/tcp/9591"};
    optsB.mountGossipsub = true;

    Libp2pModuleImpl nodeA(optsA);
    Libp2pModuleImpl nodeB(optsB);

    if (!nodeA.start().success) {
        fprintf(stderr, "Node A failed\n");
        return 1;
    }
    if (!nodeB.start().success) {
        fprintf(stderr, "Node B failed\n");
        return 1;
    }
    printf("Both nodes started\n");

/// ## Step 2: Connect the nodes
///
/// GossipSub needs connectivity between peers to build the mesh.
    auto infoA = nodeA.peerInfo().value;
    std::string peerIdA = infoA["peerId"].get<std::string>();
    std::vector<std::string> addrsA;
    for (const auto& a : infoA["addrs"])
        addrsA.push_back(a.get<std::string>());

    printf("Connecting Node B to Node A...\n");
    if (!nodeB.connectPeer(peerIdA, addrsA, 5000).success) {
        fprintf(stderr, "Failed to connect\n");
        return 1;
    }
    printf("Connected\n");

/// ## Step 3: Both nodes subscribe to a topic
///
/// A topic is just a string identifier. Peers who subscribe to the
/// same topic will receive each other's messages.
    std::string topic = "chat-room-1";
    printf("Node B subscribing to topic: \"%s\"\n", topic.c_str());
    if (!nodeB.gossipsubSubscribe(topic).success) {
        fprintf(stderr, "Node B subscribe failed\n");
        return 1;
    }

    printf("Node A subscribing to topic: \"%s\"\n", topic.c_str());
    if (!nodeA.gossipsubSubscribe(topic).success) {
        fprintf(stderr, "Node A subscribe failed\n");
        return 1;
    }

/// ## Step 4: Wait for the GossipSub mesh to form
///
/// After subscribing, libp2p needs time to discover subscribers
/// and build the message-forwarding mesh.
    printf("Waiting for GossipSub mesh to form...\n");
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

/// ## Step 5: Publish a message
///
/// Node A publishes a message to the topic. It will be forwarded
/// to all subscribers (including Node B).
    std::string payload = "Hello from Node A via GossipSub!";
    printf("\nNode A publishing: \"%s\"\n", payload.c_str());
    printf("  Topic: \"%s\"\n", topic.c_str());

    if (!nodeA.gossipsubPublish(topic, payload).success) {
        fprintf(stderr, "Publish failed\n");
        return 1;
    }
    printf("Message published!\n");

/// ## Step 6: Receive the message on Node B
///
/// `gossipsubNextMessage()` blocks until a message arrives or the
/// timeout expires. The timeout is in milliseconds.
    printf("\nNode B waiting for message...\n");
    auto res = nodeB.gossipsubNextMessage(topic, 3000);
    if (!res.success) {
        fprintf(stderr, "Node B did not receive any messages: %s\n",
                res.error.c_str());
        return 1;
    }

    std::string received = res.value.get<std::string>();
    printf("Node B received: \"%s\"\n", received.c_str());

    if (received == payload) {
        printf("Message verified!\n");
    } else {
        printf("Message content differs (expected: \"%s\")\n",
               payload.c_str());
    }

/// ## Step 7: Alternative — listen via event callback
///
/// Instead of polling with `gossipsubNextMessage()`, you can listen
/// for the `"gossipsubMessage"` event via `emitEvent`. This is useful
/// for event-driven applications.
    printf("\n--- Alternative: Event-driven reception ---\n");

    bool messageReceived = false;
    std::string eventMessage;

    nodeA.emitEvent = [&](const std::string& name,
                          const std::string& data) {
        if (name == "gossipsubMessage") {
            auto j = nlohmann::json::parse(data);
            std::string eventTopic = j["topic"].get<std::string>();
            std::string msg = j["data"].get<std::string>();
            printf("Node A (event): received on topic \"%s\": \"%s\"\n",
                   eventTopic.c_str(), msg.c_str());
            messageReceived = true;
            eventMessage = msg;
        }
    };

    // Publish another message
    std::string payload2 = "Second message via event!";
    nodeB.gossipsubPublish(topic, payload2);

    // Give it a moment to arrive
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    if (messageReceived) {
        printf("Event-driven reception worked: \"%s\"\n",
               eventMessage.c_str());
    }

/// ## Step 8: Unsubscribe and clean up
    printf("\nUnsubscribing...\n");
    nodeB.gossipsubUnsubscribe(topic);
    nodeA.gossipsubUnsubscribe(topic);

    nodeA.stop();
    nodeB.stop();

    printf("\n=== Tutorial 7 Complete ===\n");

/// ## Key Takeaways
///
///   - GossipSub provides topic-based pub/sub messaging
///   - Both publisher and subscriber must subscribe to the topic
///   - Allow 1-2 seconds for the mesh to form
///   - Use `gossipsubNextMessage(topic, timeout)` for polling
///   - Use `emitEvent` for event-driven message reception
///   - Always unsubscribe and stop cleanly
///
/// In the [next tutorial](tutorial_8_service_discovery.md), we'll
/// learn how peers can discover each other by capability using the
/// service discovery API.
    return 0;
}
