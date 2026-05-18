#include <cstdio>
#include <condition_variable>
#include <mutex>
#include <string>
#include "plugin.h"

using json = nlohmann::json;

// Demonstrates mounting a custom protocol and exchanging messages over a stream.
//
// NodeA mounts "/example/echo/1.0.0" and echoes back whatever it receives.
// NodeB dials nodeA, sends a message, and reads the echo.

int main()
{
    const std::string proto = "/example/echo/1.0.0";
    const std::string message = "Hello from node B!";

    Libp2pModuleImpl nodeA;
    Libp2pModuleImpl nodeB;

    printf("Starting nodes...\n");
    if (!nodeA.start().success) { fprintf(stderr, "nodeA failed to start\n"); return 1; }
    if (!nodeB.start().success) { fprintf(stderr, "nodeB failed to start\n"); return 1; }

    // Collect nodeA's address so nodeB can connect.
    auto infoA = nodeA.peerInfo().value;
    std::string peerIdA = infoA["peerId"].get<std::string>();
    std::vector<std::string> addrsA;
    for (const auto& a : infoA["addrs"]) addrsA.push_back(a.get<std::string>());

    // Set up nodeA to handle incoming streams on the custom protocol.
    // The protocolStream event delivers the streamId for each new connection.
    std::mutex mtx;
    std::condition_variable cv;
    uint64_t serverStreamId = 0;
    bool streamReady = false;

    nodeA.emitEvent = [&](const std::string& name, const std::string& data) {
        if (name != "protocolStream") return;
        std::lock_guard<std::mutex> lk(mtx);
        serverStreamId = json::parse(data)["streamId"].get<uint64_t>();
        streamReady = true;
        cv.notify_one();
    };

    if (!nodeA.mountProtocol(proto).success) {
        fprintf(stderr, "Failed to mount protocol\n");
        return 1;
    }
    printf("NodeA mounted protocol: %s\n", proto.c_str());

    if (!nodeB.connectPeer(peerIdA, addrsA, 500).success) {
        fprintf(stderr, "Failed to connect\n");
        return 1;
    }

    // NodeB dials nodeA on the custom protocol.
    auto dialRes = nodeB.dial(peerIdA, proto);
    if (!dialRes.success) { fprintf(stderr, "Dial failed: %s\n", dialRes.error.c_str()); return 1; }
    uint64_t clientStreamId = dialRes.value.get<uint64_t>();
    printf("NodeB dialed nodeA, client stream id: %llu\n", (unsigned long long)clientStreamId);

    // Wait for nodeA to receive the incoming stream.
    {
        std::unique_lock<std::mutex> lk(mtx);
        if (!cv.wait_for(lk, std::chrono::seconds(5), [&] { return streamReady; })) {
            fprintf(stderr, "Timed out waiting for incoming stream\n");
            return 1;
        }
    }
    printf("NodeA received stream id: %llu\n", (unsigned long long)serverStreamId);

    // NodeB sends a message.
    printf("NodeB sending: \"%s\"\n", message.c_str());
    if (!nodeB.streamWriteLp(clientStreamId, message).success) {
        fprintf(stderr, "Write failed\n");
        return 1;
    }

    // NodeA reads the message and echoes it back.
    auto readRes = nodeA.streamReadLp(serverStreamId, 4096);
    if (!readRes.success) { fprintf(stderr, "NodeA read failed: %s\n", readRes.error.c_str()); return 1; }
    std::string received = readRes.value.get<std::string>();
    printf("NodeA received: \"%s\"\n", received.c_str());

    if (!nodeA.streamWriteLp(serverStreamId, received).success) {
        fprintf(stderr, "NodeA echo write failed\n");
        return 1;
    }

    // NodeB reads the echo.
    auto echoRes = nodeB.streamReadLp(clientStreamId, 4096);
    if (!echoRes.success) { fprintf(stderr, "NodeB read echo failed: %s\n", echoRes.error.c_str()); return 1; }
    std::string echo = echoRes.value.get<std::string>();
    printf("NodeB received echo: \"%s\"\n", echo.c_str());

    if (echo != message) {
        fprintf(stderr, "Echo mismatch!\n");
        return 1;
    }
    printf("Echo verified\n");

    // Clean up both streams.
    nodeB.streamCloseWithEOF(clientStreamId);
    nodeB.streamRelease(clientStreamId);
    nodeA.streamRelease(serverStreamId);

    nodeA.stop();
    nodeB.stop();

    printf("Done\n");
    return 0;
}
