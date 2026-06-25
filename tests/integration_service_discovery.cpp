#include <logos_test.h>
#include <plugin.h>
#include <thread>
#include <chrono>
#include "test_helpers.h"

static Libp2pModuleOptions discoOptions() {
    return Libp2pModuleOptions{ .mountServiceDiscovery = true };
}

LOGOS_TEST(disco_start_stop) {
    Libp2pModuleImpl node(discoOptions());
    LOGOS_ASSERT_TRUE(node.start().success);

    LOGOS_ASSERT_TRUE(node.discoStart().success);
    LOGOS_ASSERT_TRUE(node.discoStop().success);

    LOGOS_ASSERT_TRUE(node.stop().success);
}

LOGOS_TEST(disco_advertise_and_lookup) {
    Libp2pModuleImpl nodeC(discoOptions());
    LOGOS_ASSERT_TRUE(nodeC.start().success);
    LOGOS_ASSERT_TRUE(nodeC.discoStart().success);
    auto [peerIdC, addrsC] = getPeerInfoPair(nodeC);

    Libp2pModuleOptions optsA = discoOptions();
    optsA.bootstrapNodes = { {peerIdC, addrsC} };
    Libp2pModuleImpl nodeA(optsA);
    LOGOS_ASSERT_TRUE(nodeA.start().success);
    LOGOS_ASSERT_TRUE(nodeA.discoStart().success);
    LOGOS_ASSERT_TRUE(nodeA.connectPeer(peerIdC, addrsC, 5000).success);

    Libp2pModuleOptions optsB = discoOptions();
    optsB.bootstrapNodes = { {peerIdC, addrsC} };
    Libp2pModuleImpl nodeB(optsB);
    LOGOS_ASSERT_TRUE(nodeB.start().success);
    LOGOS_ASSERT_TRUE(nodeB.discoStart().success);
    LOGOS_ASSERT_TRUE(nodeB.connectPeer(peerIdC, addrsC, 5000).success);

    std::string serviceId = "test-service";
    LOGOS_ASSERT_TRUE(nodeA.discoStartAdvertising(serviceId, "").success);
    LOGOS_ASSERT_TRUE(nodeB.discoRegisterInterest(serviceId).success);

    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    auto res = nodeB.discoLookup(serviceId, "");
    LOGOS_ASSERT_TRUE(res.success);

    auto records = res.value;
    LOGOS_ASSERT_FALSE(records.empty());

    auto [peerIdA, addrsA] = getPeerInfoPair(nodeA);
    LOGOS_ASSERT_TRUE(records[0]["peerId"].get<std::string>() == peerIdA);

    LOGOS_ASSERT_TRUE(nodeA.discoStop().success);
    LOGOS_ASSERT_TRUE(nodeB.discoStop().success);
    LOGOS_ASSERT_TRUE(nodeC.discoStop().success);

    LOGOS_ASSERT_TRUE(nodeA.stop().success);
    LOGOS_ASSERT_TRUE(nodeB.stop().success);
    LOGOS_ASSERT_TRUE(nodeC.stop().success);
}

LOGOS_TEST(disco_advertise_with_data) {
    Libp2pModuleImpl nodeC(discoOptions());
    LOGOS_ASSERT_TRUE(nodeC.start().success);
    LOGOS_ASSERT_TRUE(nodeC.discoStart().success);
    auto [peerIdC, addrsC] = getPeerInfoPair(nodeC);

    Libp2pModuleOptions optsA = discoOptions();
    optsA.bootstrapNodes = { {peerIdC, addrsC} };
    Libp2pModuleImpl nodeA(optsA);
    LOGOS_ASSERT_TRUE(nodeA.start().success);
    LOGOS_ASSERT_TRUE(nodeA.discoStart().success);
    LOGOS_ASSERT_TRUE(nodeA.connectPeer(peerIdC, addrsC, 5000).success);

    Libp2pModuleOptions optsB = discoOptions();
    optsB.bootstrapNodes = { {peerIdC, addrsC} };
    Libp2pModuleImpl nodeB(optsB);
    LOGOS_ASSERT_TRUE(nodeB.start().success);
    LOGOS_ASSERT_TRUE(nodeB.discoStart().success);
    LOGOS_ASSERT_TRUE(nodeB.connectPeer(peerIdC, addrsC, 5000).success);

    std::string serviceId = "data-service";
    std::string serviceData = "version=2;proto=test";

    LOGOS_ASSERT_TRUE(nodeA.discoStartAdvertising(serviceId, serviceData).success);
    LOGOS_ASSERT_TRUE(nodeB.discoRegisterInterest(serviceId).success);

    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    auto res = nodeB.discoLookup(serviceId, serviceData);
    LOGOS_ASSERT_TRUE(res.success);

    auto records = res.value;
    LOGOS_ASSERT_FALSE(records.empty());

    LOGOS_ASSERT_TRUE(nodeA.discoStop().success);
    LOGOS_ASSERT_TRUE(nodeB.discoStop().success);
    LOGOS_ASSERT_TRUE(nodeC.discoStop().success);

    LOGOS_ASSERT_TRUE(nodeA.stop().success);
    LOGOS_ASSERT_TRUE(nodeB.stop().success);
    LOGOS_ASSERT_TRUE(nodeC.stop().success);
}

LOGOS_TEST(disco_start_stop_advertising) {
    Libp2pModuleImpl nodeA(discoOptions());
    Libp2pModuleImpl nodeB(discoOptions());

    LOGOS_ASSERT_TRUE(nodeA.start().success);
    LOGOS_ASSERT_TRUE(nodeB.start().success);

    LOGOS_ASSERT_TRUE(nodeA.discoStart().success);
    LOGOS_ASSERT_TRUE(nodeB.discoStart().success);

    auto [peerIdA, addrsA] = getPeerInfoPair(nodeA);
    LOGOS_ASSERT_TRUE(nodeB.connectPeer(peerIdA, addrsA, 500).success);

    std::string serviceId = "ephemeral-service";

    LOGOS_ASSERT_TRUE(nodeA.discoStartAdvertising(serviceId, "").success);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    LOGOS_ASSERT_TRUE(nodeA.discoStopAdvertising(serviceId).success);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    auto res = nodeB.discoLookup(serviceId, "");
    LOGOS_ASSERT_TRUE(res.success);

    auto records = res.value;
    LOGOS_ASSERT_TRUE(records.empty());

    LOGOS_ASSERT_TRUE(nodeA.discoStop().success);
    LOGOS_ASSERT_TRUE(nodeB.discoStop().success);

    LOGOS_ASSERT_TRUE(nodeA.stop().success);
    LOGOS_ASSERT_TRUE(nodeB.stop().success);
}

LOGOS_TEST(disco_random_lookup) {
    Libp2pModuleImpl nodeA(discoOptions());
    Libp2pModuleImpl nodeB(discoOptions());

    LOGOS_ASSERT_TRUE(nodeA.start().success);
    LOGOS_ASSERT_TRUE(nodeB.start().success);

    LOGOS_ASSERT_TRUE(nodeA.discoStart().success);
    LOGOS_ASSERT_TRUE(nodeB.discoStart().success);

    auto [peerIdA, addrsA] = getPeerInfoPair(nodeA);
    LOGOS_ASSERT_TRUE(nodeB.connectPeer(peerIdA, addrsA, 500).success);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    auto res = nodeA.discoRandomLookup();
    LOGOS_ASSERT_TRUE(res.success);

    LOGOS_ASSERT_TRUE(nodeA.discoStop().success);
    LOGOS_ASSERT_TRUE(nodeB.discoStop().success);

    LOGOS_ASSERT_TRUE(nodeA.stop().success);
    LOGOS_ASSERT_TRUE(nodeB.stop().success);
}

LOGOS_TEST(create_xpr) {
    Libp2pModuleImpl node(discoOptions());
    LOGOS_ASSERT_TRUE(node.start().success);

    auto [peerId, addrs] = getPeerInfoPair(node);

    std::vector<std::pair<std::string, std::string>> services = {
        {"chat", std::string{0x01, 0x02, 0x03}},
        {"file-share", ""},
    };

    auto res = node.createXpr(addrs, services, 42);
    LOGOS_ASSERT_TRUE(res.success);
    LOGOS_ASSERT_FALSE(res.value.get<std::string>().empty());

    // Only seqNo changes, so a differing payload isolates the seqNo branch.
    auto resSeq = node.createXpr(addrs, services, 43);
    LOGOS_ASSERT_TRUE(resSeq.success);
    LOGOS_ASSERT_TRUE(res.value.get<std::string>() != resSeq.value.get<std::string>());

    // Empty addrs falls back to listen addresses; seqNo 0 uses current time.
    auto resDefaults = node.createXpr({}, {}, 0);
    LOGOS_ASSERT_TRUE(resDefaults.success);
    LOGOS_ASSERT_FALSE(resDefaults.value.get<std::string>().empty());

    LOGOS_ASSERT_TRUE(node.stop().success);
}

LOGOS_TEST(decode_xpr) {
    Libp2pModuleImpl node(discoOptions());
    LOGOS_ASSERT_TRUE(node.start().success);

    auto [peerId, addrs] = getPeerInfoPair(node);

    std::vector<std::pair<std::string, std::string>> services = {
        {"chat", std::string{0x01, 0x02, 0x03}},
        {"file-share", ""},
    };

    auto created = node.createXpr(addrs, services, 42);
    LOGOS_ASSERT_TRUE(created.success);

    std::string signedXpr = base64Decode(created.value.get<std::string>());
    auto decoded = node.decodeXpr(signedXpr);
    LOGOS_ASSERT_TRUE(decoded.success);
    LOGOS_ASSERT_EQ(decoded.value["peerId"].get<std::string>(), peerId);
    LOGOS_ASSERT_EQ(decoded.value["seqNo"].get<uint64_t>(), 42u);
    LOGOS_ASSERT_EQ(decoded.value["services"].size(), services.size());
    LOGOS_ASSERT_EQ(decoded.value["services"][0]["id"].get<std::string>(), "chat");
    LOGOS_ASSERT_EQ(decoded.value["services"][0]["data"].get<std::string>(),
                    (std::string{0x01, 0x02, 0x03}));

    // A flipped byte must fail signature verification, not silently decode.
    std::string tampered = signedXpr;
    tampered[tampered.size() / 2] ^= 0xff;
    LOGOS_ASSERT_FALSE(node.decodeXpr(tampered).success);

    LOGOS_ASSERT_FALSE(node.decodeXpr("").success);

    LOGOS_ASSERT_TRUE(node.stop().success);
}
