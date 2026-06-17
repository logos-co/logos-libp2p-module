#include <logos_test.h>
#include <plugin.h>

LOGOS_TEST(sync_connect_disconnect_peer) {
    Libp2pModuleImpl plugin;
    LOGOS_ASSERT_TRUE(plugin.start().success);

    std::string fakePeer = "12D3KooWInvalidPeerForTest";
    std::vector<std::string> fakeAddrs = { "/ip4/127.0.0.1/tcp/9999" };

    LOGOS_ASSERT_FALSE(plugin.connectPeer(fakePeer, fakeAddrs).success);
    LOGOS_ASSERT_FALSE(plugin.disconnectPeer(fakePeer).success);

    LOGOS_ASSERT_TRUE(plugin.stop().success);
}

LOGOS_TEST(sync_peer_info) {
    Libp2pModuleImpl plugin;
    LOGOS_ASSERT_TRUE(plugin.start().success);

    auto res = plugin.peerInfo();
    LOGOS_ASSERT_TRUE(res.success);

    LOGOS_ASSERT_FALSE(res.value["peerId"].get<std::string>().empty());

    LOGOS_ASSERT_TRUE(plugin.stop().success);
}

LOGOS_TEST(sync_connected_peers) {
    Libp2pModuleImpl plugin;
    LOGOS_ASSERT_TRUE(plugin.start().success);

    auto res = plugin.connectedPeers(0);
    LOGOS_ASSERT_TRUE(res.success);

    auto peers = res.value;
    LOGOS_ASSERT_TRUE(peers.is_array());
    LOGOS_ASSERT_EQ(peers.size(), size_t(0));

    LOGOS_ASSERT_TRUE(plugin.stop().success);
}

LOGOS_TEST(sync_dial) {
    Libp2pModuleImpl plugin;
    LOGOS_ASSERT_TRUE(plugin.start().success);

    std::string fakePeer = "12D3KooWInvalidPeerForTest";
    std::string proto = "/test/1.0.0";

    LOGOS_ASSERT_FALSE(plugin.dial(fakePeer, proto).success);

    LOGOS_ASSERT_TRUE(plugin.stop().success);
}

LOGOS_TEST(sync_public_key) {
    Libp2pModuleImpl plugin;
    LOGOS_ASSERT_TRUE(plugin.start().success);
    LOGOS_ASSERT_TRUE(plugin.publicKey().success);
    LOGOS_ASSERT_TRUE(plugin.stop().success);
}

LOGOS_TEST(sync_stream_close) {
    Libp2pModuleImpl plugin;
    LOGOS_ASSERT_TRUE(plugin.start().success);

    uint64_t fakeStreamId = 1234;
    LOGOS_ASSERT_FALSE(plugin.streamClose(fakeStreamId).success);

    LOGOS_ASSERT_TRUE(plugin.stop().success);
}

LOGOS_TEST(sync_stream_close_with_eof) {
    Libp2pModuleImpl plugin;
    LOGOS_ASSERT_TRUE(plugin.start().success);

    uint64_t fakeStreamId = 1234;
    LOGOS_ASSERT_FALSE(plugin.streamCloseWithEOF(fakeStreamId).success);

    LOGOS_ASSERT_TRUE(plugin.stop().success);
}

LOGOS_TEST(sync_stream_release) {
    Libp2pModuleImpl plugin;
    LOGOS_ASSERT_TRUE(plugin.start().success);

    uint64_t fakeStreamId = 1234;
    LOGOS_ASSERT_FALSE(plugin.streamRelease(fakeStreamId).success);

    LOGOS_ASSERT_TRUE(plugin.stop().success);
}

LOGOS_TEST(sync_stream_read_exactly) {
    Libp2pModuleImpl plugin;
    LOGOS_ASSERT_TRUE(plugin.start().success);

    uint64_t fakeStreamId = 1234;
    size_t len = 16;
    LOGOS_ASSERT_FALSE(plugin.streamReadExactly(fakeStreamId, len).success);

    LOGOS_ASSERT_TRUE(plugin.stop().success);
}

LOGOS_TEST(sync_stream_read_lp) {
    Libp2pModuleImpl plugin;
    LOGOS_ASSERT_TRUE(plugin.start().success);

    uint64_t fakeStreamId = 1234;
    size_t maxSize = 4096;
    LOGOS_ASSERT_FALSE(plugin.streamReadLp(fakeStreamId, maxSize).success);

    LOGOS_ASSERT_TRUE(plugin.stop().success);
}

LOGOS_TEST(sync_stream_write) {
    Libp2pModuleImpl plugin;
    LOGOS_ASSERT_TRUE(plugin.start().success);

    uint64_t fakeStreamId = 1234;
    std::string data = "hello-stream";
    LOGOS_ASSERT_FALSE(plugin.streamWrite(fakeStreamId, data).success);

    LOGOS_ASSERT_TRUE(plugin.stop().success);
}

LOGOS_TEST(sync_stream_write_lp) {
    Libp2pModuleImpl plugin;
    LOGOS_ASSERT_TRUE(plugin.start().success);

    uint64_t fakeStreamId = 1234;
    std::string data = "hello-stream-lp";
    LOGOS_ASSERT_FALSE(plugin.streamWriteLp(fakeStreamId, data).success);

    LOGOS_ASSERT_TRUE(plugin.stop().success);
}

LOGOS_TEST(sync_gossipsub_subscribe_publish) {
    Libp2pModuleImpl plugin;
    LOGOS_ASSERT_TRUE(plugin.start().success);

    std::string topic = "sync-topic";
    std::string msg = "sync-gossipsub-msg";

    LOGOS_ASSERT_TRUE(plugin.gossipsubSubscribe(topic).success);
    LOGOS_ASSERT_TRUE(plugin.gossipsubPublish(topic, msg).success);

    LOGOS_ASSERT_TRUE(plugin.stop().success);
}

LOGOS_TEST(sync_gossipsub_unsubscribe) {
    Libp2pModuleImpl plugin;
    LOGOS_ASSERT_TRUE(plugin.start().success);

    std::string topic = "sync-topic";

    LOGOS_ASSERT_TRUE(plugin.gossipsubSubscribe(topic).success);
    LOGOS_ASSERT_TRUE(plugin.gossipsubUnsubscribe(topic).success);

    LOGOS_ASSERT_TRUE(plugin.stop().success);
}

LOGOS_TEST(sync_kad_get_put_value) {
    Libp2pModuleImpl plugin;
    LOGOS_ASSERT_TRUE(plugin.start().success);

    std::string key = "sync-test-key";
    std::string value = "sync-hello-world";

    LOGOS_ASSERT_TRUE(plugin.kadPutValue(key, value).success);

    auto res = plugin.kadGetValue(key, 1);
    LOGOS_ASSERT_TRUE(res.success);
    LOGOS_ASSERT_TRUE(res.value.get<std::string>() == value);

    LOGOS_ASSERT_TRUE(plugin.stop().success);
}

LOGOS_TEST(sync_key_to_cid_and_providers) {
    Libp2pModuleImpl plugin;
    LOGOS_ASSERT_TRUE(plugin.start().success);

    std::string key = "sync-provider-test-key";
    std::string value = "sync-provider-test-value";

    auto res = plugin.toCid(key);
    LOGOS_ASSERT_TRUE(res.success);

    std::string cid = res.value.get<std::string>();
    LOGOS_ASSERT_FALSE(cid.empty());

    LOGOS_ASSERT_TRUE(plugin.kadPutValue(key, value).success);
    LOGOS_ASSERT_TRUE(plugin.kadAddProvider(cid).success);

    auto provRes = plugin.kadGetProviders(cid);
    LOGOS_ASSERT_TRUE(provRes.success);

    auto providers = provRes.value;
    LOGOS_ASSERT_TRUE(providers.is_array());
    LOGOS_ASSERT_EQ(providers.size(), size_t(0));

    LOGOS_ASSERT_TRUE(plugin.stop().success);
}

LOGOS_TEST(sync_kad_find_node) {
    Libp2pModuleImpl plugin;
    LOGOS_ASSERT_TRUE(plugin.start().success);

    std::string fakePeer = "12D3KooWInvalidPeerForSyncTest";
    LOGOS_ASSERT_FALSE(plugin.kadFindNode(fakePeer).success);

    LOGOS_ASSERT_TRUE(plugin.stop().success);
}

LOGOS_TEST(sync_kad_provide_lifecycle) {
    Libp2pModuleImpl plugin;
    LOGOS_ASSERT_TRUE(plugin.start().success);

    std::string key = "sync-providing-key";

    auto res = plugin.toCid(key);
    LOGOS_ASSERT_TRUE(res.success);

    std::string cid = res.value.get<std::string>();
    LOGOS_ASSERT_FALSE(cid.empty());

    LOGOS_ASSERT_TRUE(plugin.kadStartProviding(cid).success);
    LOGOS_ASSERT_TRUE(plugin.kadStopProviding(cid).success);

    LOGOS_ASSERT_TRUE(plugin.stop().success);
}

LOGOS_TEST(sync_kad_random_records) {
    Libp2pModuleImpl plugin(Libp2pModuleOptions{ .mountServiceDiscovery = true });
    LOGOS_ASSERT_TRUE(plugin.start().success);

    auto res = plugin.kadGetRandomRecords();
    LOGOS_ASSERT_TRUE(res.success);
    LOGOS_ASSERT_TRUE(res.value.is_array());

    LOGOS_ASSERT_TRUE(plugin.stop().success);
}
