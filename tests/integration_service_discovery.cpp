#include <logos_test.h>
#include <plugin.h>
#include <thread>
#include <chrono>

static Libp2pModuleOptions discoOptions() {
    return Libp2pModuleOptions{ .mountServiceDiscovery = true };
}

static std::pair<std::string, std::vector<std::string>> getPeerInfoPair(Libp2pModuleImpl& node) {
    auto res = node.peerInfo();
    auto info = res.value;
    std::string peerId = info["peerId"].get<std::string>();
    std::vector<std::string> addrs;
    for (const auto& a : info["addrs"]) {
        addrs.push_back(a.get<std::string>());
    }
    return {peerId, addrs};
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
    LOGOS_ASSERT_TRUE(nodeA.discoStartAdvertising(serviceId).success);
    LOGOS_ASSERT_TRUE(nodeB.discoStartDiscovering(serviceId).success);

    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    auto res = nodeB.discoLookup(serviceId);
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
    LOGOS_ASSERT_TRUE(nodeB.discoStartDiscovering(serviceId).success);

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

    LOGOS_ASSERT_TRUE(nodeA.discoStartAdvertising(serviceId).success);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    LOGOS_ASSERT_TRUE(nodeA.discoStopAdvertising(serviceId).success);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    auto res = nodeB.discoLookup(serviceId);
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
