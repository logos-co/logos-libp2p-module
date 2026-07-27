#include <logos_test.h>
#include <plugin.h>
#include <condition_variable>
#include <mutex>
#include <chrono>
#include <thread>

using json = nlohmann::json;

static std::pair<std::string, std::vector<std::string>> getPeerInfoPair(Libp2pModuleImpl& node) {
    auto res = node.peerInfo();
    LOGOS_ASSERT_TRUE(res.success);
    auto info = res.value;
    std::string peerId = info["peerId"].get<std::string>();
    std::vector<std::string> addrs;
    for (const auto& a : info["addrs"])
        addrs.push_back(a.get<std::string>());
    return {peerId, addrs};
}

static auto noopEmitEvent = [](const std::string&, const std::string&) {};

LOGOS_TEST(custom_handlers_mount_without_emit_event_ok) {
    Libp2pModuleImpl node;
    LOGOS_ASSERT_TRUE(node.start().success);
    LOGOS_ASSERT_TRUE(node.mountProtocol("/test/proto/1.0.0").success);
    LOGOS_ASSERT_TRUE(node.stop().success);
}

LOGOS_TEST(custom_handlers_mount_empty_proto) {
    Libp2pModuleImpl node;
    node.emitEvent = noopEmitEvent;
    LOGOS_ASSERT_TRUE(node.start().success);
    auto res = node.mountProtocol("");
    LOGOS_ASSERT_FALSE(res.success);
    LOGOS_ASSERT_TRUE(res.error == "Protocol string is empty");
    LOGOS_ASSERT_TRUE(node.stop().success);
}

LOGOS_TEST(custom_handlers_mount_valid_proto) {
    Libp2pModuleImpl node;
    node.emitEvent = noopEmitEvent;
    LOGOS_ASSERT_TRUE(node.start().success);
    LOGOS_ASSERT_TRUE(node.mountProtocol("/test/proto/1.0.0").success);
    LOGOS_ASSERT_TRUE(node.stop().success);
}

LOGOS_TEST(custom_handlers_mount_multiple_protocols) {
    Libp2pModuleImpl node;
    node.emitEvent = noopEmitEvent;
    LOGOS_ASSERT_TRUE(node.start().success);
    LOGOS_ASSERT_TRUE(node.mountProtocol("/test/proto/1.0.0").success);
    LOGOS_ASSERT_TRUE(node.mountProtocol("/test/proto/2.0.0").success);
    LOGOS_ASSERT_TRUE(node.stop().success);
}

LOGOS_TEST(custom_handlers_protocol_stream_event) {
    const std::string proto = "/test/custom/1.0.0";

    Libp2pModuleImpl nodeA;
    Libp2pModuleImpl nodeB;

    std::mutex mtx;
    std::condition_variable cv;
    std::string capturedData;
    bool eventReceived = false;

    nodeB.emitEvent = [&](const std::string& name, const std::string& data) {
        if (name == "protocolStream") {
            std::lock_guard<std::mutex> lk(mtx);
            capturedData = data;
            eventReceived = true;
            cv.notify_one();
        }
    };

    LOGOS_ASSERT_TRUE(nodeB.start().success);
    LOGOS_ASSERT_TRUE(nodeB.mountProtocol(proto).success);

    LOGOS_ASSERT_TRUE(nodeA.start().success);
    auto [peerIdB, addrsB] = getPeerInfoPair(nodeB);
    LOGOS_ASSERT_TRUE(nodeA.connectPeer(peerIdB, addrsB, 500).success);

    auto dialResult = nodeA.dial(peerIdB, proto);
    LOGOS_ASSERT_TRUE(dialResult.success);
    uint64_t clientStreamId = dialResult.value.get<uint64_t>();

    {
        std::unique_lock<std::mutex> lk(mtx);
        bool received = cv.wait_for(lk, std::chrono::seconds(5), [&] { return eventReceived; });
        LOGOS_ASSERT_TRUE(received);
    }

    auto j = json::parse(capturedData);
    LOGOS_ASSERT_TRUE(j["proto"].get<std::string>() == proto);
    uint64_t serverStreamId = j["streamId"].get<uint64_t>();
    LOGOS_ASSERT_NE(serverStreamId, static_cast<uint64_t>(0));

    // nodeB must release its server stream first, else its write side stays open
    // and nodeA's closeWithEOF deadlocks waiting for the remote EOF.
    LOGOS_ASSERT_TRUE(nodeB.streamRelease(serverStreamId).success);
    LOGOS_ASSERT_TRUE(nodeA.streamCloseWithEOF(clientStreamId).success);
    LOGOS_ASSERT_TRUE(nodeA.streamRelease(clientStreamId).success);

    LOGOS_ASSERT_TRUE(nodeA.stop().success);
    LOGOS_ASSERT_TRUE(nodeB.stop().success);
}

LOGOS_TEST(protocol_bridge_request_accept_roundtrip) {
    const std::string proto = "/test/bridge/1.0.0";

    Libp2pModuleImpl nodeA;
    Libp2pModuleImpl nodeB;

    LOGOS_ASSERT_TRUE(nodeB.start().success);
    LOGOS_ASSERT_TRUE(nodeB.mountProtocol(proto).success);
    LOGOS_ASSERT_TRUE(nodeA.start().success);

    const std::string request = "ping-over-the-bridge";
    auto [peerIdB, addrsB] = getPeerInfoPair(nodeB);

    std::string serverSawRequest;
    bool serverOk = false;
    std::thread server([&] {
        auto acc = nodeB.protocolAcceptStream(json{{"proto", proto}, {"timeoutMs", 5000}}.dump());
        if (!acc.success) return;
        uint64_t sid = acc.value["streamId"].get<uint64_t>();
        if (sid == 0) return;

        auto rd = nodeB.streamReadLpJson(json{{"streamId", sid}, {"timeoutMs", 5000}}.dump());
        if (!rd.success) return;
        serverSawRequest = base64Decode(rd.value["dataB64"].get<std::string>());

        std::string resp = "echo:" + serverSawRequest;
        std::vector<uint8_t> respBytes(resp.begin(), resp.end());
        auto w = nodeB.streamWriteLpJson(
            json{{"streamId", sid}, {"dataB64", base64Encode(respBytes)}}.dump());
        auto r = nodeB.streamReleaseJson(json{{"streamId", sid}}.dump());
        serverOk = w.success && r.success;
    });

    std::vector<uint8_t> reqBytes(request.begin(), request.end());
    auto resp = nodeA.protocolRequest(json{
        {"peerId", peerIdB},
        {"multiaddrs", addrsB},
        {"proto", proto},
        {"requestB64", base64Encode(reqBytes)},
        {"timeoutMs", 5000},
    }.dump());
    server.join();

    LOGOS_ASSERT_TRUE(serverOk);
    LOGOS_ASSERT_TRUE(serverSawRequest == request);
    LOGOS_ASSERT_TRUE(resp.success);
    std::string respStr = base64Decode(resp.value["responseB64"].get<std::string>());
    LOGOS_ASSERT_TRUE(respStr == "echo:" + request);

    LOGOS_ASSERT_TRUE(nodeA.stop().success);
    LOGOS_ASSERT_TRUE(nodeB.stop().success);
}

LOGOS_TEST(protocol_bridge_request_no_response) {
    const std::string proto = "/test/bridge/noresp/1.0.0";

    Libp2pModuleImpl nodeA;
    Libp2pModuleImpl nodeB;

    LOGOS_ASSERT_TRUE(nodeB.start().success);
    LOGOS_ASSERT_TRUE(nodeB.mountProtocol(proto).success);
    LOGOS_ASSERT_TRUE(nodeA.start().success);

    const std::string request = "fire-and-forget";
    auto [peerIdB, addrsB] = getPeerInfoPair(nodeB);
    std::string serverSawRequest;
    bool serverOk = false;
    std::thread server([&] {
        auto acc = nodeB.protocolAcceptStream(json{{"proto", proto}, {"timeoutMs", 5000}}.dump());
        if (!acc.success) return;
        uint64_t sid = acc.value["streamId"].get<uint64_t>();
        if (sid == 0) return;
        auto rd = nodeB.streamReadLpJson(json{{"streamId", sid}, {"timeoutMs", 5000}}.dump());
        if (!rd.success) return;
        serverSawRequest = base64Decode(rd.value["dataB64"].get<std::string>());
        serverOk = nodeB.streamReleaseJson(json{{"streamId", sid}}.dump()).success;
    });

    std::vector<uint8_t> reqBytes(request.begin(), request.end());
    auto resp = nodeA.protocolRequest(json{
        {"peerId", peerIdB},
        {"multiaddrs", addrsB},
        {"proto", proto},
        {"requestB64", base64Encode(reqBytes)},
        {"timeoutMs", 5000},
        {"expectResponse", false},
    }.dump());
    server.join();

    LOGOS_ASSERT_TRUE(resp.success);
    LOGOS_ASSERT_TRUE(serverOk);
    LOGOS_ASSERT_TRUE(serverSawRequest == request);

    LOGOS_ASSERT_TRUE(nodeA.stop().success);
    LOGOS_ASSERT_TRUE(nodeB.stop().success);
}

LOGOS_TEST(protocol_bridge_release_purges_inbound_queue) {
    const std::string proto = "/test/bridge/purge/1.0.0";

    Libp2pModuleImpl nodeA;
    Libp2pModuleImpl nodeB;

    std::mutex mtx;
    std::condition_variable cv;
    uint64_t serverStreamId = 0;
    bool eventReceived = false;

    nodeB.emitEvent = [&](const std::string& name, const std::string& data) {
        if (name != "protocolStream") return;
        auto j = json::parse(data, nullptr, false);
        if (j.is_discarded()) return;
        std::lock_guard<std::mutex> lk(mtx);
        serverStreamId = j["streamId"].get<uint64_t>();
        eventReceived = true;
        cv.notify_one();
    };

    LOGOS_ASSERT_TRUE(nodeB.start().success);
    LOGOS_ASSERT_TRUE(nodeB.mountProtocol(proto).success);
    LOGOS_ASSERT_TRUE(nodeA.start().success);

    auto [peerIdB, addrsB] = getPeerInfoPair(nodeB);
    LOGOS_ASSERT_TRUE(nodeA.connectPeer(peerIdB, addrsB, 500).success);
    auto dialResult = nodeA.dial(peerIdB, proto);
    LOGOS_ASSERT_TRUE(dialResult.success);
    uint64_t clientStreamId = dialResult.value.get<uint64_t>();

    {
        std::unique_lock<std::mutex> lk(mtx);
        LOGOS_ASSERT_TRUE(cv.wait_for(lk, std::chrono::seconds(5), [&] { return eventReceived; }));
    }
    LOGOS_ASSERT_NE(serverStreamId, static_cast<uint64_t>(0));

    LOGOS_ASSERT_TRUE(nodeB.streamRelease(serverStreamId).success);

    auto acc = nodeB.protocolAcceptStream(json{{"proto", proto}, {"timeoutMs", 200}}.dump());
    LOGOS_ASSERT_FALSE(acc.success);

    LOGOS_ASSERT_TRUE(nodeA.streamRelease(clientStreamId).success);
    LOGOS_ASSERT_TRUE(nodeA.stop().success);
    LOGOS_ASSERT_TRUE(nodeB.stop().success);
}

LOGOS_TEST(protocol_bridge_write_requires_dataB64) {
    Libp2pModuleImpl node;
    LOGOS_ASSERT_TRUE(node.start().success);
    auto r = node.streamWriteLpJson(json{{"streamId", 1}}.dump());
    LOGOS_ASSERT_FALSE(r.success);
    LOGOS_ASSERT_TRUE(node.stop().success);
}
