#include <logos_test.h>
#include <plugin.h>

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

LOGOS_TEST(kad_put_get) {
    Libp2pModuleImpl nodeA;
    LOGOS_ASSERT_TRUE(nodeA.start().success);
    auto [peerIdA, addrsA] = getPeerInfoPair(nodeA);

    Libp2pModuleImpl nodeB(Libp2pModuleOptions{
        .bootstrapNodes = { {peerIdA, addrsA} }
    });
    LOGOS_ASSERT_TRUE(nodeB.start().success);

    std::string key = "integration-key";
    std::string value = "hello";
    LOGOS_ASSERT_TRUE(nodeA.kadPutValue(key, value).success);

    int quorum = 1;
    auto result = nodeB.kadGetValue(key, quorum);
    LOGOS_ASSERT_TRUE(result.success);

    std::string record = result.value.get<std::string>();
    LOGOS_ASSERT_TRUE(record == value);

    LOGOS_ASSERT_TRUE(nodeA.stop().success);
    LOGOS_ASSERT_TRUE(nodeB.stop().success);
}

LOGOS_TEST(kad_find_node) {
    Libp2pModuleImpl nodeA;
    LOGOS_ASSERT_TRUE(nodeA.start().success);
    auto [peerIdA, addrsA] = getPeerInfoPair(nodeA);

    Libp2pModuleImpl nodeB(Libp2pModuleOptions{
        .bootstrapNodes = { {peerIdA, addrsA} }
    });
    LOGOS_ASSERT_TRUE(nodeB.start().success);
    auto [peerIdB, addrsB] = getPeerInfoPair(nodeB);

    Libp2pModuleImpl nodeC(Libp2pModuleOptions{
        .bootstrapNodes = { {peerIdA, addrsA} }
    });
    LOGOS_ASSERT_TRUE(nodeC.start().success);
    auto [peerIdC, addrsC] = getPeerInfoPair(nodeC);

    Libp2pModuleImpl nodeD(Libp2pModuleOptions{
        .bootstrapNodes = { {peerIdA, addrsA} }
    });
    LOGOS_ASSERT_TRUE(nodeD.start().success);

    auto result = nodeD.kadFindNode(peerIdB);
    LOGOS_ASSERT_TRUE(result.success);

    auto peers = result.value;
    LOGOS_ASSERT_EQ(peers.size(), size_t(3));

    bool foundA = false, foundB = false, foundC = false;
    for (const auto& p : peers) {
        std::string pid = p.get<std::string>();
        if (pid == peerIdA) foundA = true;
        if (pid == peerIdB) foundB = true;
        if (pid == peerIdC) foundC = true;
    }
    LOGOS_ASSERT_TRUE(foundA);
    LOGOS_ASSERT_TRUE(foundB);
    LOGOS_ASSERT_TRUE(foundC);

    LOGOS_ASSERT_TRUE(nodeA.stop().success);
    LOGOS_ASSERT_TRUE(nodeB.stop().success);
    LOGOS_ASSERT_TRUE(nodeC.stop().success);
    LOGOS_ASSERT_TRUE(nodeD.stop().success);
}

LOGOS_TEST(kad_start_stop_providing) {
    Libp2pModuleImpl nodeA;
    LOGOS_ASSERT_TRUE(nodeA.start().success);
    auto [peerIdA, addrsA] = getPeerInfoPair(nodeA);

    Libp2pModuleImpl nodeB(Libp2pModuleOptions{
        .bootstrapNodes = { {peerIdA, addrsA} }
    });
    LOGOS_ASSERT_TRUE(nodeB.start().success);

    std::string key = "provider-test-key";
    auto cidResult = nodeA.toCid(key);
    LOGOS_ASSERT_TRUE(cidResult.success);
    std::string cid = cidResult.value.get<std::string>();
    LOGOS_ASSERT_FALSE(cid.empty());

    LOGOS_ASSERT_TRUE(nodeA.kadStartProviding(cid).success);

    auto res = nodeB.kadGetProviders(cid);
    LOGOS_ASSERT_TRUE(res.success);
    auto providers = res.value;

    LOGOS_ASSERT_FALSE(providers.empty());
    LOGOS_ASSERT_TRUE(providers[0]["peerId"].get<std::string>() == peerIdA);

    LOGOS_ASSERT_TRUE(nodeA.kadStopProviding(cid).success);

    res = nodeB.kadGetProviders(cid);
    LOGOS_ASSERT_TRUE(res.success);
    providers = res.value;
    LOGOS_ASSERT_TRUE(providers.empty());

    LOGOS_ASSERT_TRUE(nodeA.stop().success);
    LOGOS_ASSERT_TRUE(nodeB.stop().success);
}

LOGOS_TEST(kad_random_records) {
    Libp2pModuleImpl nodeA(Libp2pModuleOptions{ .mountServiceDiscovery = true });
    LOGOS_ASSERT_TRUE(nodeA.start().success);
    auto [peerIdA, addrsA] = getPeerInfoPair(nodeA);

    Libp2pModuleOptions optsB;
    optsB.mountServiceDiscovery = true;
    optsB.bootstrapNodes = { {peerIdA, addrsA} };
    Libp2pModuleImpl nodeB(optsB);
    LOGOS_ASSERT_TRUE(nodeB.start().success);
    auto [peerIdB, addrsB] = getPeerInfoPair(nodeB);

    auto res = nodeA.kadGetRandomRecords();
    LOGOS_ASSERT_TRUE(res.success);
    auto records = res.value;

    LOGOS_ASSERT_FALSE(records.empty());
    LOGOS_ASSERT_TRUE(records[0]["peerId"].get<std::string>() == peerIdB);

    LOGOS_ASSERT_TRUE(nodeA.stop().success);
    LOGOS_ASSERT_TRUE(nodeB.stop().success);
}
