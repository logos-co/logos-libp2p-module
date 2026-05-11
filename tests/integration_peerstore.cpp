#include <logos_test.h>
#include <plugin.h>
#include <thread>
#include <chrono>

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

LOGOS_TEST(peerstore_get_peers_empty) {
    Libp2pModuleImpl node;
    LOGOS_ASSERT_TRUE(node.start().success);

    auto res = node.peerstoreGetPeers();
    LOGOS_ASSERT_TRUE(res.success);
    LOGOS_ASSERT_TRUE(res.value.is_array());
    LOGOS_ASSERT_TRUE(res.value.empty());

    LOGOS_ASSERT_TRUE(node.stop().success);
}

LOGOS_TEST(peerstore_add_and_get_peer) {
    Libp2pModuleImpl nodeA;
    Libp2pModuleImpl nodeB;
    LOGOS_ASSERT_TRUE(nodeA.start().success);
    LOGOS_ASSERT_TRUE(nodeB.start().success);

    auto [peerIdA, addrsA] = getPeerInfoPair(nodeA);
    LOGOS_ASSERT_TRUE(nodeB.connectPeer(peerIdA, addrsA, 5000).success);

    auto res = nodeB.peerstoreGetPeers();
    LOGOS_ASSERT_TRUE(res.success);
    LOGOS_ASSERT_TRUE(res.value.is_array());

    bool found = false;
    for (const auto& p : res.value) {
        if (p.get<std::string>() == peerIdA) { found = true; break; }
    }
    LOGOS_ASSERT_TRUE(found);

    LOGOS_ASSERT_TRUE(nodeA.stop().success);
    LOGOS_ASSERT_TRUE(nodeB.stop().success);
}

LOGOS_TEST(peerstore_get_peer_info) {
    Libp2pModuleImpl nodeA;
    Libp2pModuleImpl nodeB;
    LOGOS_ASSERT_TRUE(nodeA.start().success);
    LOGOS_ASSERT_TRUE(nodeB.start().success);

    auto [peerIdA, addrsA] = getPeerInfoPair(nodeA);
    LOGOS_ASSERT_TRUE(nodeB.connectPeer(peerIdA, addrsA, 5000).success);

    auto res = nodeB.peerstoreGetPeerInfo(peerIdA);
    LOGOS_ASSERT_TRUE(res.success);
    LOGOS_ASSERT_TRUE(res.value["peerId"].get<std::string>() == peerIdA);
    LOGOS_ASSERT_FALSE(res.value["addrs"].empty());

    LOGOS_ASSERT_TRUE(nodeA.stop().success);
    LOGOS_ASSERT_TRUE(nodeB.stop().success);
}

LOGOS_TEST(peerstore_add_peer_explicit) {
    Libp2pModuleImpl nodeA;
    Libp2pModuleImpl nodeB;
    LOGOS_ASSERT_TRUE(nodeA.start().success);
    LOGOS_ASSERT_TRUE(nodeB.start().success);

    auto [peerIdA, addrsA] = getPeerInfoPair(nodeA);
    std::vector<std::string> protos = { "/test/1.0.0" };

    auto res = nodeB.peerstoreAddPeer(peerIdA, addrsA, protos);
    LOGOS_ASSERT_TRUE(res.success);

    LOGOS_ASSERT_TRUE(nodeA.stop().success);
    LOGOS_ASSERT_TRUE(nodeB.stop().success);
}

LOGOS_TEST(peerstore_set_peer_addresses) {
    Libp2pModuleImpl nodeA;
    Libp2pModuleImpl nodeB;
    LOGOS_ASSERT_TRUE(nodeA.start().success);
    LOGOS_ASSERT_TRUE(nodeB.start().success);

    auto [peerIdA, addrsA] = getPeerInfoPair(nodeA);
    LOGOS_ASSERT_TRUE(nodeB.connectPeer(peerIdA, addrsA, 5000).success);

    std::vector<std::string> newAddrs = { "/ip4/127.0.0.1/tcp/5001" };
    auto res = nodeB.peerstoreSetPeerAddresses(peerIdA, newAddrs);
    LOGOS_ASSERT_TRUE(res.success);

    LOGOS_ASSERT_TRUE(nodeA.stop().success);
    LOGOS_ASSERT_TRUE(nodeB.stop().success);
}

LOGOS_TEST(peerstore_set_peer_protocols) {
    Libp2pModuleImpl nodeA;
    Libp2pModuleImpl nodeB;
    LOGOS_ASSERT_TRUE(nodeA.start().success);
    LOGOS_ASSERT_TRUE(nodeB.start().success);

    auto [peerIdA, addrsA] = getPeerInfoPair(nodeA);
    LOGOS_ASSERT_TRUE(nodeB.connectPeer(peerIdA, addrsA, 5000).success);

    std::vector<std::string> protos = { "/test/1.0.0", "/test/2.0.0" };
    auto res = nodeB.peerstoreSetPeerProtocols(peerIdA, protos);
    LOGOS_ASSERT_TRUE(res.success);

    LOGOS_ASSERT_TRUE(nodeA.stop().success);
    LOGOS_ASSERT_TRUE(nodeB.stop().success);
}

LOGOS_TEST(peerstore_delete_peer) {
    Libp2pModuleImpl nodeA;
    Libp2pModuleImpl nodeB;
    LOGOS_ASSERT_TRUE(nodeA.start().success);
    LOGOS_ASSERT_TRUE(nodeB.start().success);

    auto [peerIdA, addrsA] = getPeerInfoPair(nodeA);
    LOGOS_ASSERT_TRUE(nodeB.connectPeer(peerIdA, addrsA, 5000).success);

    auto peersRes = nodeB.peerstoreGetPeers();
    LOGOS_ASSERT_TRUE(peersRes.success);
    bool foundBefore = false;
    for (const auto& p : peersRes.value) {
        if (p.get<std::string>() == peerIdA) { foundBefore = true; break; }
    }
    LOGOS_ASSERT_TRUE(foundBefore);

    LOGOS_ASSERT_TRUE(nodeB.peerstoreDeletePeer(peerIdA).success);

    auto afterRes = nodeB.peerstoreGetPeers();
    LOGOS_ASSERT_TRUE(afterRes.success);
    bool foundAfter = false;
    for (const auto& p : afterRes.value) {
        if (p.get<std::string>() == peerIdA) { foundAfter = true; break; }
    }
    LOGOS_ASSERT_FALSE(foundAfter);

    LOGOS_ASSERT_TRUE(nodeA.stop().success);
    LOGOS_ASSERT_TRUE(nodeB.stop().success);
}
