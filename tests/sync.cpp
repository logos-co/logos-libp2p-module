#include <logos_test.h>
#include <plugin.h>

LOGOS_TEST(sync_connect_disconnect_peer) {
    Libp2pModulePlugin plugin;
    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStart().ok);

    QString fakePeer = "12D3KooWInvalidPeerForTest";
    QList<QString> fakeAddrs = { "/ip4/127.0.0.1/tcp/9999" };

    LOGOS_ASSERT_FALSE(plugin.syncConnectPeer(fakePeer, fakeAddrs).ok);
    LOGOS_ASSERT_FALSE(plugin.syncDisconnectPeer(fakePeer).ok);

    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStop().ok);
}

LOGOS_TEST(sync_peer_info) {
    Libp2pModulePlugin plugin;
    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStart().ok);

    auto res = plugin.syncPeerInfo();
    LOGOS_ASSERT_TRUE(res.ok);

    PeerInfo peerInfo = res.data.value<PeerInfo>();
    LOGOS_ASSERT_FALSE(peerInfo.peerId.isEmpty());

    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStop().ok);
}

LOGOS_TEST(sync_connected_peers) {
    Libp2pModulePlugin plugin;
    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStart().ok);

    auto res = plugin.syncConnectedPeers();
    LOGOS_ASSERT_TRUE(res.ok);

    QList<QString> connectedPeers = res.data.value<QList<QString>>();
    LOGOS_ASSERT_EQ(connectedPeers.size(), qsizetype(0));

    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStop().ok);
}

LOGOS_TEST(sync_dial) {
    Libp2pModulePlugin plugin;
    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStart().ok);

    QString fakePeer = "12D3KooWInvalidPeerForTest";
    QString proto = "/test/1.0.0";

    LOGOS_ASSERT_FALSE(plugin.syncDial(fakePeer, proto).ok);

    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStop().ok);
}

LOGOS_TEST(sync_public_key) {
    Libp2pModulePlugin plugin;
    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStart().ok);
    LOGOS_ASSERT_TRUE(plugin.syncLibp2pPublicKey().ok);
    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStop().ok);
}

LOGOS_TEST(sync_stream_close) {
    Libp2pModulePlugin plugin;
    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStart().ok);

    uint64_t fakeStreamId = 1234;
    LOGOS_ASSERT_FALSE(plugin.syncStreamClose(fakeStreamId).ok);

    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStop().ok);
}

LOGOS_TEST(sync_stream_close_with_eof) {
    Libp2pModulePlugin plugin;
    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStart().ok);

    uint64_t fakeStreamId = 1234;
    LOGOS_ASSERT_FALSE(plugin.syncStreamCloseWithEOF(fakeStreamId).ok);

    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStop().ok);
}

LOGOS_TEST(sync_stream_release) {
    Libp2pModulePlugin plugin;
    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStart().ok);

    uint64_t fakeStreamId = 1234;
    LOGOS_ASSERT_FALSE(plugin.syncStreamRelease(fakeStreamId).ok);

    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStop().ok);
}

LOGOS_TEST(sync_stream_read_exactly) {
    Libp2pModulePlugin plugin;
    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStart().ok);

    uint64_t fakeStreamId = 1234;
    size_t len = 16;
    LOGOS_ASSERT_FALSE(plugin.syncStreamReadExactly(fakeStreamId, len).ok);

    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStop().ok);
}

LOGOS_TEST(sync_stream_read_lp) {
    Libp2pModulePlugin plugin;
    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStart().ok);

    uint64_t fakeStreamId = 1234;
    size_t maxSize = 4096;
    LOGOS_ASSERT_FALSE(plugin.syncStreamReadLp(fakeStreamId, maxSize).ok);

    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStop().ok);
}

LOGOS_TEST(sync_stream_write) {
    Libp2pModulePlugin plugin;
    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStart().ok);

    uint64_t fakeStreamId = 1234;
    QByteArray data = "hello-stream";
    LOGOS_ASSERT_FALSE(plugin.syncStreamWrite(fakeStreamId, data).ok);

    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStop().ok);
}

LOGOS_TEST(sync_stream_write_lp) {
    Libp2pModulePlugin plugin;
    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStart().ok);

    uint64_t fakeStreamId = 1234;
    QByteArray data = "hello-stream-lp";
    LOGOS_ASSERT_FALSE(plugin.syncStreamWriteLp(fakeStreamId, data).ok);

    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStop().ok);
}

LOGOS_TEST(sync_gossipsub_subscribe_publish) {
    Libp2pModulePlugin plugin;
    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStart().ok);

    QString topic = "sync-topic";
    QByteArray msg = "sync-gossipsub-msg";

    LOGOS_ASSERT_TRUE(plugin.syncGossipsubSubscribe(topic).ok);
    LOGOS_ASSERT_TRUE(plugin.syncGossipsubPublish(topic, msg).ok);

    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStop().ok);
}

LOGOS_TEST(sync_gossipsub_unsubscribe) {
    Libp2pModulePlugin plugin;
    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStart().ok);

    QString topic = "sync-topic";

    LOGOS_ASSERT_TRUE(plugin.syncGossipsubSubscribe(topic).ok);
    LOGOS_ASSERT_TRUE(plugin.syncGossipsubUnsubscribe(topic).ok);

    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStop().ok);
}

LOGOS_TEST(sync_kad_get_put_value) {
    Libp2pModulePlugin plugin;
    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStart().ok);

    QByteArray key = "sync-test-key";
    QByteArray value = "sync-hello-world";

    LOGOS_ASSERT_TRUE(plugin.syncKadPutValue(key, value).ok);

    auto res = plugin.syncKadGetValue(key, 1);
    LOGOS_ASSERT_TRUE(res.ok);
    QByteArray actual = res.data.value<QByteArray>();
    LOGOS_ASSERT_TRUE(actual == value);

    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStop().ok);
}

LOGOS_TEST(sync_key_to_cid_and_providers) {
    Libp2pModulePlugin plugin;
    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStart().ok);

    QByteArray key = "sync-provider-test-key";
    QByteArray value = "sync-provider-test-value";

    auto res = plugin.syncToCid(key);
    LOGOS_ASSERT_TRUE(res.ok);

    QString cid = res.data.value<QString>();
    LOGOS_ASSERT_FALSE(cid.isEmpty());

    LOGOS_ASSERT_TRUE(plugin.syncKadPutValue(key, value).ok);
    LOGOS_ASSERT_TRUE(plugin.syncKadAddProvider(cid).ok);

    res = plugin.syncKadGetProviders(cid);
    LOGOS_ASSERT_TRUE(res.ok);

    QList<PeerInfo> providers = res.data.value<QList<PeerInfo>>();
    LOGOS_ASSERT_EQ(providers.size(), qsizetype(0));

    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStop().ok);
}

LOGOS_TEST(sync_kad_find_node) {
    Libp2pModulePlugin plugin;
    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStart().ok);

    QString fakePeer = "12D3KooWInvalidPeerForSyncTest";
    LOGOS_ASSERT_FALSE(plugin.syncKadFindNode(fakePeer).ok);

    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStop().ok);
}

LOGOS_TEST(sync_kad_provide_lifecycle) {
    Libp2pModulePlugin plugin;
    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStart().ok);

    QByteArray key = "sync-providing-key";

    auto res = plugin.syncToCid(key);
    LOGOS_ASSERT_TRUE(res.ok);

    QString cid = res.data.value<QString>();
    LOGOS_ASSERT_FALSE(cid.isEmpty());

    LOGOS_ASSERT_TRUE(plugin.syncKadStartProviding(cid).ok);
    LOGOS_ASSERT_TRUE(plugin.syncKadStopProviding(cid).ok);

    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStop().ok);
}

LOGOS_TEST(sync_kad_random_records) {
    Libp2pModulePlugin plugin(Libp2pModuleOptions{ .mountServiceDiscovery = true });
    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStart().ok);

    auto res = plugin.syncKadGetRandomRecords();
    LOGOS_ASSERT_TRUE(res.ok);
    LOGOS_ASSERT_TRUE(res.data.isValid());

    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStop().ok);
}

LOGOS_TEST(sync_mix_dial) {
    Libp2pModulePlugin plugin;
    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStart().ok);

    QString fakePeer = "12D3KooWInvalidMixPeer";
    QString addr = "/ip4/127.0.0.1/tcp/9999";
    QString proto = "/mix/test/1.0.0";

    LOGOS_ASSERT_FALSE(plugin.syncMixDial(fakePeer, addr, proto).ok);

    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStop().ok);
}

LOGOS_TEST(sync_mix_dial_with_reply) {
    Libp2pModulePlugin plugin;
    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStart().ok);

    QString fakePeer = "12D3KooWInvalidMixPeer";
    QString addr = "/ip4/127.0.0.1/tcp/9999";
    QString proto = "/mix/test/1.0.0";

    LOGOS_ASSERT_FALSE(plugin.syncMixDialWithReply(fakePeer, addr, proto, 1, 2).ok);

    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStop().ok);
}

LOGOS_TEST(sync_mix_register_dest_read_behavior) {
    Libp2pModulePlugin plugin;
    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStart().ok);

    QString proto = "/mix/test/1.0.0";
    auto res = plugin.syncMixRegisterDestReadBehavior(proto, LIBP2P_MIX_READ_EXACTLY, 1024);
    LOGOS_ASSERT_FALSE(res.data.isValid());

    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStop().ok);
}

LOGOS_TEST(sync_mix_set_node_info) {
    Libp2pModulePlugin plugin;
    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStart().ok);

    PeerInfo peerInfo = plugin.syncPeerInfo().data.value<PeerInfo>();
    LOGOS_ASSERT_TRUE(plugin.syncMixSetNodeInfo(peerInfo.addrs[0], plugin.mixGeneratePrivKey()).ok);

    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStop().ok);
}

LOGOS_TEST(sync_mix_nodepool_add) {
    Libp2pModulePlugin plugin;
    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStart().ok);

    QString fakePeer = "12D3KooWInvalidMixPeer";
    QString addr = "/ip4/127.0.0.1/tcp/9999";

    QByteArray mixPubKey = plugin.mixPublicKey(plugin.mixGeneratePrivKey());
    QByteArray fakeLibp2pKey(33, 0x01);

    auto res = plugin.syncMixNodepoolAdd(fakePeer, addr, mixPubKey, fakeLibp2pKey);
    LOGOS_ASSERT_FALSE(res.data.isValid());

    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStop().ok);
}
