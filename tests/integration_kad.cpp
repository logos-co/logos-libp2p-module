#include <logos_test.h>
#include <plugin.h>

LOGOS_TEST(kad_put_get) {
    Libp2pModulePlugin nodeA;
    LOGOS_ASSERT_TRUE(nodeA.syncLibp2pStart().ok);
    PeerInfo nodeAPeerInfo = nodeA.syncPeerInfo().data.value<PeerInfo>();

    Libp2pModulePlugin nodeB(Libp2pModuleOptions{ .bootstrapNodes = { nodeAPeerInfo } });
    LOGOS_ASSERT_TRUE(nodeB.syncLibp2pStart().ok);

    QByteArray key = "integration-key";
    QByteArray value = "hello";
    LOGOS_ASSERT_TRUE(nodeA.syncKadPutValue(key, value).ok);

    int quorum = 1;
    Libp2pResult result = nodeB.syncKadGetValue(key, quorum);
    LOGOS_ASSERT_TRUE(result.ok);

    QByteArray record = result.data.value<QByteArray>();
    LOGOS_ASSERT_TRUE(record == value);

    LOGOS_ASSERT_TRUE(nodeA.syncLibp2pStop().ok);
    LOGOS_ASSERT_TRUE(nodeB.syncLibp2pStop().ok);
}

LOGOS_TEST(kad_find_node) {
    Libp2pModulePlugin nodeA;
    LOGOS_ASSERT_TRUE(nodeA.syncLibp2pStart().ok);
    PeerInfo infoA = nodeA.syncPeerInfo().data.value<PeerInfo>();

    Libp2pModulePlugin nodeB(Libp2pModuleOptions{ .bootstrapNodes = { infoA } });
    LOGOS_ASSERT_TRUE(nodeB.syncLibp2pStart().ok);
    PeerInfo infoB = nodeB.syncPeerInfo().data.value<PeerInfo>();

    Libp2pModulePlugin nodeC(Libp2pModuleOptions{ .bootstrapNodes = { infoA } });
    LOGOS_ASSERT_TRUE(nodeC.syncLibp2pStart().ok);
    PeerInfo infoC = nodeC.syncPeerInfo().data.value<PeerInfo>();

    Libp2pModulePlugin nodeD(Libp2pModuleOptions{ .bootstrapNodes = { infoA } });
    LOGOS_ASSERT_TRUE(nodeD.syncLibp2pStart().ok);

    Libp2pResult result = nodeD.syncKadFindNode(infoB.peerId);
    LOGOS_ASSERT_TRUE(result.ok);

    QList<QString> peers = result.data.value<QList<QString>>();
    LOGOS_ASSERT_EQ(peers.size(), qsizetype(3));
    LOGOS_ASSERT_TRUE(peers.contains(infoA.peerId));
    LOGOS_ASSERT_TRUE(peers.contains(infoB.peerId));
    LOGOS_ASSERT_TRUE(peers.contains(infoC.peerId));

    LOGOS_ASSERT_TRUE(nodeA.syncLibp2pStop().ok);
    LOGOS_ASSERT_TRUE(nodeB.syncLibp2pStop().ok);
    LOGOS_ASSERT_TRUE(nodeC.syncLibp2pStop().ok);
    LOGOS_ASSERT_TRUE(nodeD.syncLibp2pStop().ok);
}

LOGOS_TEST(kad_start_stop_providing) {
    Libp2pModulePlugin nodeA;
    LOGOS_ASSERT_TRUE(nodeA.syncLibp2pStart().ok);
    PeerInfo infoA = nodeA.syncPeerInfo().data.value<PeerInfo>();

    Libp2pModulePlugin nodeB(Libp2pModuleOptions{ .bootstrapNodes = { infoA } });
    LOGOS_ASSERT_TRUE(nodeB.syncLibp2pStart().ok);

    QByteArray key = "provider-test-key";
    Libp2pResult cidResult = nodeA.syncToCid(key);
    LOGOS_ASSERT_TRUE(cidResult.ok);
    QString cid = cidResult.data.value<QString>();
    LOGOS_ASSERT_FALSE(cid.isEmpty());

    LOGOS_ASSERT_TRUE(nodeA.syncKadStartProviding(cid).ok);

    Libp2pResult res = nodeB.syncKadGetProviders(cid);
    LOGOS_ASSERT_TRUE(res.ok);
    QList<PeerInfo> providers = res.data.value<QList<PeerInfo>>();

    LOGOS_ASSERT_FALSE(providers.isEmpty());
    LOGOS_ASSERT_TRUE(providers[0].peerId == infoA.peerId);

    LOGOS_ASSERT_TRUE(nodeA.syncKadStopProviding(cid).ok);

    res = nodeB.syncKadGetProviders(cid);
    LOGOS_ASSERT_TRUE(res.ok);
    providers = res.data.value<QList<PeerInfo>>();
    LOGOS_ASSERT_TRUE(providers.isEmpty());

    LOGOS_ASSERT_TRUE(nodeA.syncLibp2pStop().ok);
    LOGOS_ASSERT_TRUE(nodeB.syncLibp2pStop().ok);
}

LOGOS_TEST(kad_random_records) {
    Libp2pModulePlugin nodeA(Libp2pModuleOptions{ .mountServiceDiscovery = true });
    LOGOS_ASSERT_TRUE(nodeA.syncLibp2pStart().ok);
    PeerInfo nodeAPeerInfo = nodeA.syncPeerInfo().data.value<PeerInfo>();

    Libp2pModulePlugin nodeB(Libp2pModuleOptions{ .bootstrapNodes = { nodeAPeerInfo }, .mountServiceDiscovery = true });
    LOGOS_ASSERT_TRUE(nodeB.syncLibp2pStart().ok);
    PeerInfo nodeBPeerInfo = nodeB.syncPeerInfo().data.value<PeerInfo>();

    Libp2pResult res = nodeA.syncKadGetRandomRecords();
    LOGOS_ASSERT_TRUE(res.ok);
    QList<ExtendedPeerRecord> records = res.data.value<QList<ExtendedPeerRecord>>();

    LOGOS_ASSERT_FALSE(records.isEmpty());
    LOGOS_ASSERT_TRUE(records[0].peerId == nodeBPeerInfo.peerId);

    LOGOS_ASSERT_TRUE(nodeA.syncLibp2pStop().ok);
    LOGOS_ASSERT_TRUE(nodeB.syncLibp2pStop().ok);
}
