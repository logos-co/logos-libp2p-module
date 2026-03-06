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
    }
};

QTEST_MAIN(TestLibp2pModuleConfig)
#include "config.moc"
