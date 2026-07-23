# Tutorial 7: GossipSub – Pub/Sub Messaging

GossipSub is a pub/sub protocol that lets peers broadcast messages to
everyone subscribed to a topic. It's the foundation for many
decentralized applications — from chat rooms to blockchain transaction
propagation.

In this tutorial we'll:
  - Subscribe two nodes to the same topic
  - Wait for the GossipSub mesh to form
  - Publish a message from one node
  - Receive it on the other node

## How GossipSub Works

1. Peers subscribe to topics by calling `gossipsubSubscribe(topic)`
2. libp2p builds a **mesh** — a set of peer connections per topic
3. When a peer publishes to a topic, the message is forwarded through
   the mesh to all subscribers
4. Subscribers receive messages via `gossipsubNextMessage(topic, timeout)`

> **Note**: The GossipSub mesh takes some time to form after both
> peers have subscribed. A short delay (1-2 seconds) is usually enough.
```cpp
#include <cstdio>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <string>
#include <vector>
#include "plugin.h"

int main()
{
    printf("=== Tutorial 7: GossipSub – Pub/Sub Messaging ===\n\n");

```

## Step 1: Create two nodes with GossipSub enabled

GossipSub is enabled by default (`mountGossipsub: true`).
We keep it explicit here for clarity.
```cpp
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

```

## Step 2: Connect the nodes

GossipSub needs connectivity between peers to build the mesh.
```cpp
    auto infoARes = nodeA.peerInfo();
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
    if (!nodeB.connectPeer(peerIdA, addrsA, 5000).success) {
        fprintf(stderr, "Failed to connect\n");
        return 1;
    }
    printf("Connected\n");

```

## Step 3: Prepare an event callback for Node A

This callback is only needed for Step 8, where Node A receives a
message through the event-driven API. The polling flow in Steps 4-7
uses `gossipsubNextMessage()` instead and does not depend on this
callback.

`emitEvent` lets the module notify application code when something
happens asynchronously. Register it before Node A subscribes, because
the subscription path snapshots the callback used by worker threads.
The callback listens for `"gossipsubMessage"` events, parses the JSON
payload, stores the received message, and wakes any code waiting on
the condition variable.
```cpp
    std::mutex eventMtx;
    std::condition_variable eventCv;
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
            {
                std::lock_guard<std::mutex> lock(eventMtx);
                messageReceived = true;
                eventMessage = msg;
            }
            eventCv.notify_one();
        }
    };

```

## Step 4: Both nodes subscribe to a topic

A topic is just a string identifier. Peers who subscribe to the
same topic will receive each other's messages.
```cpp
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

```

## Step 5: Wait for the GossipSub mesh to form

After subscribing, libp2p needs time to discover subscribers
and build the message-forwarding mesh.
```cpp
    printf("Waiting for GossipSub mesh to form...\n");
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

```

## Step 6: Publish a message

Node A publishes a message to the topic. It will be forwarded
to all subscribers (including Node B).
```cpp
    std::string payload = "Hello from Node A via GossipSub!";
    printf("\nNode A publishing: \"%s\"\n", payload.c_str());
    printf("  Topic: \"%s\"\n", topic.c_str());

    if (!nodeA.gossipsubPublish(topic, payload).success) {
        fprintf(stderr, "Publish failed\n");
        return 1;
    }
    printf("Message published!\n");

```

## Step 7: Receive the message on Node B

`gossipsubNextMessage()` blocks until a message arrives or the
timeout expires. The timeout is in milliseconds.
```cpp
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
        fprintf(stderr,
                "Message content differs (expected: \"%s\", got: \"%s\")\n",
                payload.c_str(), received.c_str());
        return 1;
    }

```

## Step 8: Alternative — listen via event callback

Instead of polling with `gossipsubNextMessage()`, you can listen
for the `"gossipsubMessage"` event via `emitEvent`. This is useful
for event-driven applications. This uses the callback registered in
Step 3.
```cpp
    printf("\n--- Alternative: Event-driven reception ---\n");

    // Publish another message
    std::string payload2 = "Second message via event!";
    {
        std::lock_guard<std::mutex> lock(eventMtx);
        messageReceived = false;
        eventMessage.clear();
    }
    if (!nodeB.gossipsubPublish(topic, payload2).success) {
        fprintf(stderr, "Second publish failed\n");
        return 1;
    }

    bool eventReceived = false;
    std::string msgCopy;
    {
        std::unique_lock<std::mutex> lock(eventMtx);
        eventCv.wait_for(lock, std::chrono::seconds(3),
                         [&] { return messageReceived; });
        eventReceived = messageReceived;
        msgCopy = eventMessage;
    }

    if (eventReceived) {
        printf("Event-driven reception worked: \"%s\"\n",
               msgCopy.c_str());
    } else {
        fprintf(stderr, "Node A did not receive the event-driven message\n");
        return 1;
    }

    if (msgCopy != payload2) {
        fprintf(stderr,
                "Event-driven message differs (expected: \"%s\", got: \"%s\")\n",
                payload2.c_str(), msgCopy.c_str());
        return 1;
    }

```

## Step 9: Unsubscribe and clean up
```cpp
    printf("\nUnsubscribing...\n");
    if (!nodeB.gossipsubUnsubscribe(topic).success) {
        fprintf(stderr, "Node B unsubscribe failed\n");
        return 1;
    }
    if (!nodeA.gossipsubUnsubscribe(topic).success) {
        fprintf(stderr, "Node A unsubscribe failed\n");
        return 1;
    }

    nodeA.stop();
    nodeB.stop();

    printf("\n=== Tutorial 7 Complete ===\n");

    return 0;
}

```

## Key Takeaways

  - GossipSub provides topic-based pub/sub messaging
  - Both publisher and subscriber must subscribe to the topic
  - Allow 1-2 seconds for the mesh to form
  - Use `gossipsubNextMessage(topic, timeout)` for polling
  - Use `emitEvent` for event-driven message reception
  - Always unsubscribe and stop cleanly

## Run tutorial

```bash
./build/tutorial/tutorial_7_gossipsub
```
---

<table width="100%">
  <tr>
<td width="50%" align="left"><a href="tutorial_6_kademlia_providers.md">&larr; Kademlia Provider Records</a></td>
<td width="50%"></td>
  </tr>
</table>
