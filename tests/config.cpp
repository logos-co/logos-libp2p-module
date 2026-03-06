#include <QtTest>
#include <plugin.h>
#include <algorithm>

class TestLibp2pModuleConfig : public QObject
{
    Q_OBJECT

private slots:

    /* ---------------------------
     * Libp2pModuleOptions struct
     * --------------------------- */

    void testOptionsDefaults()
    {
        Libp2pModuleOptions opts;
        QVERIFY(opts.addrs.isEmpty());
        QVERIFY(opts.bootstrapNodes.isEmpty());
        QCOMPARE(opts.transport, LIBP2P_TRANSPORT_TCP);
        QCOMPARE(opts.autonat, false);
        QCOMPARE(opts.autonatV2, false);
        QCOMPARE(opts.autonatV2Server, false);
        QCOMPARE(opts.circuitRelay, false);
        QCOMPARE(opts.maxConnections, 50);
        QCOMPARE(opts.maxInConnections, 25);
        QCOMPARE(opts.maxOutConnections, 25);
        QCOMPARE(opts.maxConnsPerPeer, 1);
        QCOMPARE(opts.gossipsubTriggerSelf, true);
    }

    void testOptionsDesignatedInit()
    {
        // Setting one field leaves all others at their defaults.
        Libp2pModuleOptions opts{ .circuitRelay = true };
        QCOMPARE(opts.circuitRelay, true);
        QVERIFY(opts.addrs.isEmpty());
        QVERIFY(opts.bootstrapNodes.isEmpty());
        QCOMPARE(opts.transport, LIBP2P_TRANSPORT_TCP);
        QCOMPARE(opts.autonat, false);
        QCOMPARE(opts.autonatV2, false);
        QCOMPARE(opts.autonatV2Server, false);
    }

    /* ---------------------------
     * Options propagation
     * --------------------------- */

    void testCustomListenAddress()
    {
        Libp2pModulePlugin plugin(Libp2pModuleOptions{ .addrs = {"/ip6/::1/tcp/0"} });
        QVERIFY(plugin.syncLibp2pStart().ok);

        auto res = plugin.syncPeerInfo();
        QVERIFY(res.ok);

        PeerInfo peerInfo = res.data.value<PeerInfo>();

        bool hasIp6 = std::any_of(peerInfo.addrs.begin(), peerInfo.addrs.end(),
            [](const QString &addr) { return addr.contains("/ip6/::1"); });
        QVERIFY(hasIp6);

        QVERIFY(plugin.syncLibp2pStop().ok);
    }

    void testGossipsubTriggerSelfEnabled()
    {
        Libp2pModulePlugin plugin; // default: gossipsubTriggerSelf = true
        QVERIFY(plugin.syncLibp2pStart().ok);

        QString topic = "self-test";
        QVERIFY(plugin.syncGossipsubSubscribe(topic).ok);

        QByteArray payload("hello");
        QVERIFY(plugin.syncGossipsubPublish(topic, payload).ok);

        auto res = plugin.syncGossipsubNextMessage(topic, 1000);
        QVERIFY(res.ok);
        QCOMPARE(res.data.value<QByteArray>(), payload);
    
        QVERIFY(plugin.syncLibp2pStop().ok);
    }

    void testGossipsubTriggerSelfDisabled()
    {
        Libp2pModulePlugin plugin(Libp2pModuleOptions{ .gossipsubTriggerSelf = false });
        QVERIFY(plugin.syncLibp2pStart().ok);

        QString topic = "self-test";
        QVERIFY(plugin.syncGossipsubSubscribe(topic).ok);
        QVERIFY(plugin.syncGossipsubPublish(topic, QByteArray("hello")).ok);

        auto res = plugin.syncGossipsubNextMessage(topic, 500);
        QVERIFY(!res.ok); // own message should not be delivered
    
        QVERIFY(plugin.syncLibp2pStop().ok);
    }

    void testTcpTransport()
    {
        Libp2pModulePlugin plugin; // default options → TCP
        QVERIFY(plugin.syncLibp2pStart().ok);

        auto res = plugin.syncPeerInfo();
        QVERIFY(res.ok);

        PeerInfo peerInfo = res.data.value<PeerInfo>();

        bool hasTcp = std::any_of(peerInfo.addrs.begin(), peerInfo.addrs.end(),
            [](const QString &addr) { return addr.contains("/tcp/"); });
        QVERIFY(hasTcp);
        
        QVERIFY(plugin.syncLibp2pStop().ok);
    }

    void testQuicTransport()
    {
        Libp2pModulePlugin plugin(Libp2pModuleOptions{ .transport = LIBP2P_TRANSPORT_QUIC });
        QVERIFY(plugin.syncLibp2pStart().ok);

        auto res = plugin.syncPeerInfo();
        QVERIFY(res.ok);

        PeerInfo peerInfo = res.data.value<PeerInfo>();

        bool hasQuic = std::any_of(peerInfo.addrs.begin(), peerInfo.addrs.end(),
            [](const QString &addr) { return addr.contains("/quic-v1"); });
        QVERIFY(hasQuic);
    
        QVERIFY(plugin.syncLibp2pStop().ok);
    }
};

QTEST_MAIN(TestLibp2pModuleConfig)
#include "config.moc"
