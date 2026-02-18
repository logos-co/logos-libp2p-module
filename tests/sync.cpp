#include <QtTest>
#include <libp2p_module_plugin.h>

class TestLibp2pModuleSync : public QObject
{
    Q_OBJECT

private slots:
    /* ---------------------------
     * Sync Connectivity tests
     * --------------------------- */
    void testSyncConnectDisconnectPeer()
    {
        Libp2pModulePlugin plugin;
        QVERIFY(plugin.syncLibp2pStart());

        QString fakePeer =
            "12D3KooWInvalidPeerForTest";

        QList<QString> fakeAddrs = {
            "/ip4/127.0.0.1/tcp/9999"
        };

        // connect and disconnect should fail
        QVERIFY(!plugin.syncConnectPeer(fakePeer, fakeAddrs));
        QVERIFY(!plugin.syncDisconnectPeer(fakePeer));

        QVERIFY(plugin.syncLibp2pStop());
    }

    void testSyncPeerInfo()
    {
        Libp2pModulePlugin plugin;
        QVERIFY(plugin.syncLibp2pStart());
        QVERIFY(!plugin.syncPeerInfo().peerId.isEmpty());
        QVERIFY(plugin.syncLibp2pStop());
    }

    void testSyncConnectedPeers()
    {
        Libp2pModulePlugin plugin;
        QVERIFY(plugin.syncLibp2pStart());
        QCOMPARE(plugin.syncConnectedPeers().size(), 0);
        QVERIFY(plugin.syncLibp2pStop());
    }

    void testSyncDial()
    {
        Libp2pModulePlugin plugin;
        QVERIFY(plugin.syncLibp2pStart());

        QString fakePeer =
            "12D3KooWInvalidPeerForTest";

        QString proto = "/test/1.0.0";

        QCOMPARE(plugin.syncDial(fakePeer, proto), QVariant{});

        QVERIFY(plugin.syncLibp2pStop());
    }

    /* ---------------------------
     * Sync Kademlia tests
     * --------------------------- */

    void testSyncKadGetPutValue()
    {
        Libp2pModulePlugin plugin;

        QVERIFY(plugin.syncLibp2pStart());

        QByteArray key = "sync-test-key";
        QByteArray value = "sync-hello-world";

        QVERIFY(plugin.syncKadPutValue(key, value));
        QCOMPARE(plugin.syncKadGetValue(key, 1), value);

        QVERIFY(plugin.syncLibp2pStop());
    }

    void testSyncKeyToCidAndProviders()
    {
        Libp2pModulePlugin plugin;

        QVERIFY(plugin.syncLibp2pStart());

        QByteArray key = "sync-provider-test-key";
        QByteArray value = "sync-provider-test-value";

        QString cid = plugin.syncToCid(key);
        QVERIFY(!cid.isEmpty());

        QVERIFY(plugin.syncKadPutValue(key, value));
        QVERIFY(plugin.syncKadAddProvider(cid));
        // no providers are registered yet
        QCOMPARE(plugin.syncKadGetProviders(cid).size(), 0);

        QVERIFY(plugin.syncLibp2pStop());
    }

    void testSyncKadFindNode()
    {
        Libp2pModulePlugin plugin;

        QVERIFY(plugin.syncLibp2pStart());

        QString fakePeer = "12D3KooWInvalidPeerForSyncTest";

        // no nodes are registered yet
        QCOMPARE(plugin.syncKadFindNode(fakePeer).size(), 0);

        QVERIFY(plugin.syncLibp2pStop());
    }

    void testSyncKadProvideLifecycle()
    {
        Libp2pModulePlugin plugin;

        QVERIFY(plugin.syncLibp2pStart());

        QByteArray key = "sync-providing-key";

        QString cid = plugin.syncToCid(key);
        QVERIFY(!cid.isEmpty());

        QVERIFY(plugin.syncKadStartProviding(cid));
        QVERIFY(plugin.syncKadStopProviding(cid));

        QVERIFY(plugin.syncLibp2pStop());
    }

    void testSyncKadRandomRecords()
    {
        Libp2pModulePlugin plugin;

        QVERIFY(plugin.syncLibp2pStart());

        // no registered records yet
        QCOMPARE(plugin.syncKadGetRandomRecords().size(), 0);

        QVERIFY(plugin.syncLibp2pStop());
    }

};

QTEST_MAIN(TestLibp2pModuleSync)
#include "sync.moc"

