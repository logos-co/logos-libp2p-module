/// # Tutorial 4: Custom Protocol Handlers
///
/// In the [previous tutorial](tutorial_3_connecting_peers.md), we used the
/// built-in Ping protocol to exchange data between peers. But real
/// applications need their own protocols!
///
/// This tutorial shows you how to mount a custom protocol on a node so
/// that it can handle incoming streams from peers who dial that protocol.
///
/// ## How Custom Protocols Work
///
/// A protocol in libp2p is identified by a **protocol ID string** — a
/// `/`-separated path like `/myapp/chat/1.0.0`. When a remote peer dials
/// this protocol ID, your node receives a new stream.
///
/// To handle incoming streams you:
///   1. Call `mountProtocol()` to register a protocol ID
///   2. Set an `emitEvent` callback that listens for `"protocolStream"` events
///   3. Read from and write to the stream in the event handler
///
/// The stream lifecycle on the server side is:
///   1. Receive `protocolStream` event with a `streamId`
///   2. Read data from the stream
///   3. Write data to the stream (optional)
///   4. Call `streamRelease()` when done with the stream
///
/// > **Note**: Unlike the dialing side, the server side does **not** call
/// > `streamClose()` — the peer that initiated the stream is responsible
/// > for closing it.
#include <cstdio>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>
#include "plugin.h"

using json = nlohmann::json;

/// ## Defining our custom protocol
///
/// We'll create an **echo protocol**: the server reads a length-prefixed
/// message and echoes it back to the client. This is a common pattern
/// for request-response protocols.

const std::string kEchoProtocol = "/examples/echo/1.0.0";

int main()
{
    printf("=== Tutorial 4: Custom Protocol Handlers ===\n\n");

/// ## Step 1: Create and start two nodes
    Libp2pModuleOptions optsA, optsB;
    optsA.addrs = {"/ip4/127.0.0.1/tcp/9290"};
    optsB.addrs = {"/ip4/127.0.0.1/tcp/9291"};

    Libp2pModuleImpl nodeA(optsA);
    Libp2pModuleImpl nodeB(optsB);

    if (!nodeA.start().success) { fprintf(stderr, "Node A failed\n"); return 1; }
    if (!nodeB.start().success) { fprintf(stderr, "Node B failed\n"); return 1; }
    printf("Both nodes started\n");

/// ## Step 2: Set up the protocol handler on Node A
///
/// We define an `emitEvent` callback on Node A. Whenever a remote peer
/// dials our protocol, a `"protocolStream"` event fires with a JSON
/// payload containing the `streamId`.
///
/// For simplicity, this example reads one message, echoes it, then
/// releases the stream. A real application would likely spawn a
/// background thread to handle concurrent streams.
    struct ServerState {
        std::mutex mtx;
        std::condition_variable cv;
        uint64_t streamId = 0;
        bool ready = false;
    };
    ServerState server;

    nodeA.emitEvent = [&](const std::string& name, const std::string& data) {
        if (name != "protocolStream") return;

        auto j = json::parse(data);
        uint64_t sid = j["streamId"].get<uint64_t>();

        {
            std::lock_guard<std::mutex> lock(server.mtx);
            server.streamId = sid;
            server.ready = true;
        }
        server.cv.notify_one();
    };

/// Register the echo protocol on Node A:
    printf("Mounting protocol '%s' on Node A...\n", kEchoProtocol.c_str());
    if (!nodeA.mountProtocol(kEchoProtocol).success) {
        fprintf(stderr, "Failed to mount protocol\n");
        return 1;
    }

/// ## Step 3: Get Node A's address and connect Node B
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

/// ## Step 4: Node B dials the echo protocol
///
/// When Node B dials our custom protocol, Node A's protocol handler
/// fires, and Node A receives a new stream.
    printf("Node B dialing '%s'...\n", kEchoProtocol.c_str());
    auto dialRes = nodeB.dial(peerIdA, kEchoProtocol);
    if (!dialRes.success) {
        fprintf(stderr, "Dial failed: %s\n", dialRes.error.c_str());
        return 1;
    }
    uint64_t clientStreamId = dialRes.value.get<uint64_t>();
    printf("Node B client stream id: %llu\n",
           (unsigned long long)clientStreamId);

/// ## Step 5: Wait for Node A to receive the stream
    uint64_t serverStreamId = 0;
    {
        std::unique_lock<std::mutex> lock(server.mtx);
        if (!server.cv.wait_for(lock, std::chrono::seconds(5),
                                [&] { return server.ready; })) {
            fprintf(stderr, "Timed out waiting for incoming stream\n");
            return 1;
        }
        serverStreamId = server.streamId;
    }
    printf("Node A received stream id: %llu\n",
           (unsigned long long)serverStreamId);

/// ## Step 6: Node B sends a message
    std::string message = "Hello from Node B!";
    printf("Node B sending: \"%s\"\n", message.c_str());
    if (!nodeB.streamWriteLp(clientStreamId, message).success) {
        fprintf(stderr, "Write failed\n");
        return 1;
    }

/// ## Step 7: Node A reads the message and echoes it back
    auto readRes = nodeA.streamReadLp(serverStreamId, 4096);
    if (!readRes.success) {
        fprintf(stderr, "Node A read failed: %s\n",
                readRes.error.c_str());
        return 1;
    }
    std::string received = base64Decode(readRes.value.get<std::string>());
    printf("Node A received: \"%s\"\n", received.c_str());

/// Echo it back:
    if (!nodeA.streamWriteLp(serverStreamId, received).success) {
        fprintf(stderr, "Node A echo write failed\n");
        return 1;
    }

/// ## Step 8: Node B reads the echo
    auto echoRes = nodeB.streamReadLp(clientStreamId, 4096);
    if (!echoRes.success) {
        fprintf(stderr, "Node B read echo failed: %s\n",
                echoRes.error.c_str());
        return 1;
    }
    std::string echo = base64Decode(echoRes.value.get<std::string>());
    printf("Node B received echo: \"%s\"\n", echo.c_str());

    if (echo != message) {
        fprintf(stderr, "Echo mismatch! Got '%s'\n", echo.c_str());
        return 1;
    }
    printf("Echo verified successfully!\n");

/// ## Step 9: Clean up
///
/// The client (dialing side) closes its stream with `streamCloseWithEOF()`
/// to signal that it's done writing. Both sides release their stream
/// resources with `streamRelease()`.
    nodeB.streamCloseWithEOF(clientStreamId);
    nodeB.streamRelease(clientStreamId);
    nodeA.streamRelease(serverStreamId);

    nodeA.stop();
    nodeB.stop();

    printf("\n=== Tutorial 4 Complete ===\n");

    return 0;
}

/// ## Key Takeaways
///
///   - `mountProtocol()` registers a handler on the server side
///   - `emitEvent` with the `"protocolStream"` event delivers incoming streams
///   - The dialing side uses `streamClose()`/`streamCloseWithEOF()`
///   - The server side uses `streamRelease()` (no close)
///   - Use `streamWriteLp()` / `streamReadLp()` for length-prefixed messages

/// ## Run tutorial
///
/// ```bash
/// ./build/tutorial/tutorial_4_custom_protocol
/// ```
