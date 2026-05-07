#include <logos_test.h>
#include <plugin.h>
#include <algorithm>

LOGOS_TEST(config_options_defaults) {
    Libp2pModuleOptions opts;
    LOGOS_ASSERT_TRUE(opts.addrs.isEmpty());
    LOGOS_ASSERT_TRUE(opts.bootstrapNodes.isEmpty());
    LOGOS_ASSERT_EQ(opts.transport, LIBP2P_TRANSPORT_TCP);
    LOGOS_ASSERT_FALSE(opts.autonat);
    LOGOS_ASSERT_FALSE(opts.autonatV2);
    LOGOS_ASSERT_FALSE(opts.autonatV2Server);
    LOGOS_ASSERT_FALSE(opts.circuitRelay);
    LOGOS_ASSERT_EQ(opts.maxConnections, 50);
    LOGOS_ASSERT_EQ(opts.maxInConnections, 25);
    LOGOS_ASSERT_EQ(opts.maxOutConnections, 25);
    LOGOS_ASSERT_EQ(opts.maxConnsPerPeer, 1);
    LOGOS_ASSERT_TRUE(opts.gossipsubTriggerSelf);
}

LOGOS_TEST(config_options_designated_init) {
    Libp2pModuleOptions opts{ .circuitRelay = true };
    LOGOS_ASSERT_TRUE(opts.circuitRelay);
    LOGOS_ASSERT_TRUE(opts.addrs.isEmpty());
    LOGOS_ASSERT_TRUE(opts.bootstrapNodes.isEmpty());
    LOGOS_ASSERT_EQ(opts.transport, LIBP2P_TRANSPORT_TCP);
    LOGOS_ASSERT_FALSE(opts.autonat);
    LOGOS_ASSERT_FALSE(opts.autonatV2);
    LOGOS_ASSERT_FALSE(opts.autonatV2Server);
}

LOGOS_TEST(config_custom_listen_address) {
    Libp2pModulePlugin plugin(Libp2pModuleOptions{ .addrs = {"/ip6/::1/tcp/0"} });
    auto startRes = plugin.syncLibp2pStart();
    if (!startRes.ok) return; // IPv6 not available in sandbox

    auto res = plugin.syncPeerInfo();
    LOGOS_ASSERT_TRUE(res.ok);

    PeerInfo peerInfo = res.data.value<PeerInfo>();

    bool hasIp6 = std::any_of(peerInfo.addrs.begin(), peerInfo.addrs.end(),
        [](const QString &addr) { return addr.contains("/ip6/::1"); });
    LOGOS_ASSERT_TRUE(hasIp6);

    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStop().ok);
}

LOGOS_TEST(config_gossipsub_trigger_self_enabled) {
    Libp2pModulePlugin plugin;
    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStart().ok);

    QString topic = "self-test";
    LOGOS_ASSERT_TRUE(plugin.syncGossipsubSubscribe(topic).ok);

    QByteArray payload("hello");
    LOGOS_ASSERT_TRUE(plugin.syncGossipsubPublish(topic, payload).ok);

    auto res = plugin.syncGossipsubNextMessage(topic, 1000);
    LOGOS_ASSERT_TRUE(res.ok);
    LOGOS_ASSERT_TRUE(res.data.value<QByteArray>() == payload);

    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStop().ok);
}

LOGOS_TEST(config_gossipsub_trigger_self_disabled) {
    Libp2pModulePlugin plugin(Libp2pModuleOptions{ .gossipsubTriggerSelf = false });
    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStart().ok);

    QString topic = "self-test";
    LOGOS_ASSERT_TRUE(plugin.syncGossipsubSubscribe(topic).ok);
    LOGOS_ASSERT_TRUE(plugin.syncGossipsubPublish(topic, QByteArray("hello")).ok);

    auto res = plugin.syncGossipsubNextMessage(topic, 500);
    LOGOS_ASSERT_FALSE(res.ok);

    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStop().ok);
}

LOGOS_TEST(config_tcp_transport) {
    Libp2pModulePlugin plugin;
    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStart().ok);

    auto res = plugin.syncPeerInfo();
    LOGOS_ASSERT_TRUE(res.ok);

    PeerInfo peerInfo = res.data.value<PeerInfo>();

    bool hasTcp = std::any_of(peerInfo.addrs.begin(), peerInfo.addrs.end(),
        [](const QString &addr) { return addr.contains("/tcp/"); });
    LOGOS_ASSERT_TRUE(hasTcp);

    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStop().ok);
}

LOGOS_TEST(config_quic_transport) {
    Libp2pModulePlugin plugin(Libp2pModuleOptions{ .transport = LIBP2P_TRANSPORT_QUIC });
    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStart().ok);

    auto res = plugin.syncPeerInfo();
    LOGOS_ASSERT_TRUE(res.ok);

    PeerInfo peerInfo = res.data.value<PeerInfo>();

    bool hasQuic = std::any_of(peerInfo.addrs.begin(), peerInfo.addrs.end(),
        [](const QString &addr) { return addr.contains("/quic-v1"); });
    LOGOS_ASSERT_TRUE(hasQuic);

    LOGOS_ASSERT_TRUE(plugin.syncLibp2pStop().ok);
}
