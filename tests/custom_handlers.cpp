#include <logos_test.h>
#include <plugin.h>
#include <condition_variable>
#include <mutex>
#include <chrono>

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

LOGOS_TEST(custom_handlers_mount_requires_emit_event) {
    Libp2pModuleImpl node;
    LOGOS_ASSERT_TRUE(node.start().success);
    auto res = node.mountProtocol("/test/proto/1.0.0");
    LOGOS_ASSERT_FALSE(res.success);
    LOGOS_ASSERT_CONTAINS(res.error, "emitEvent");
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

    // nodeB must release its server stream first: the Nim protocol handler
    // awaits a releaseWaiter that only fires on streamRelease. Until it fires,
    // nodeB holds its write side open, so nodeA's closeWithEOF (which waits for
    // the remote EOF) would deadlock.
    LOGOS_ASSERT_TRUE(nodeB.streamRelease(serverStreamId).success);
    LOGOS_ASSERT_TRUE(nodeA.streamCloseWithEOF(clientStreamId).success);
    LOGOS_ASSERT_TRUE(nodeA.streamRelease(clientStreamId).success);

    LOGOS_ASSERT_TRUE(nodeA.stop().success);
    LOGOS_ASSERT_TRUE(nodeB.stop().success);
}
