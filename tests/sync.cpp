#include <QtTest>
#include <libp2p_module_plugin.h>

class TestLibp2pModuleSync : public QObject
{
    Q_OBJECT

private slots:

    /* ---------------------------
     * Connectivity
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

        QCOMPARE(plugin.syncDial(fakePeer, proto), 0);

        QVERIFY(plugin.syncLibp2pStop());
    }

    /* ---------------------------
     * Stream
     * --------------------------- */

    void testSyncStreamClose()
    {
        Libp2pModulePlugin plugin;
        QVERIFY(plugin.syncLibp2pStart());

        uint64_t fakeStreamId = 1234;

        // cannot close inexistent stream
        QVERIFY(!plugin.syncStreamClose(fakeStreamId));

        QVERIFY(plugin.syncLibp2pStop());
    }

    void testSyncStreamCloseEOF()
    {
        Libp2pModulePlugin plugin;
        QVERIFY(plugin.syncLibp2pStart());

        uint64_t fakeStreamId = 1234;

        // cannot closeEOF inexistent stream
        QVERIFY(!plugin.syncStreamCloseEOF(fakeStreamId));

        QVERIFY(plugin.syncLibp2pStop());
    }

    void testSyncStreamRelease()
    {
        Libp2pModulePlugin plugin;
        QVERIFY(plugin.syncLibp2pStart());

        uint64_t fakeStreamId = 1234;

        // cannot release inexistent stream
        QVERIFY(!plugin.syncStreamRelease(fakeStreamId));

        QVERIFY(plugin.syncLibp2pStop());
    }

    void testSyncStreamReadExactly()
    {
        Libp2pModulePlugin plugin;
        QVERIFY(plugin.syncLibp2pStart());

        uint64_t fakeStreamId = 1234;
        size_t len = 16;

        // cannot readExactly from inexistent stream
        QCOMPARE(plugin.syncStreamReadExactly(fakeStreamId, len).size(), 0);

        QVERIFY(plugin.syncLibp2pStop());
    }

    void testSyncStreamReadLp()
    {
        Libp2pModulePlugin plugin;
        QVERIFY(plugin.syncLibp2pStart());

        uint64_t fakeStreamId = 1234;
        size_t maxSize = 4096;

        // cannot readExactly from inexistent stream
        QCOMPARE(plugin.syncStreamReadLp(fakeStreamId, maxSize).size(), 0);

        QVERIFY(plugin.syncLibp2pStop());
    }

    void testSyncStreamWrite()
    {
        Libp2pModulePlugin plugin;
        QVERIFY(plugin.syncLibp2pStart());

        uint64_t fakeStreamId = 1234;
        QByteArray data = "hello-stream";

        // cannot write to inexistent stream
        QVERIFY(!plugin.syncStreamWrite(fakeStreamId, data));

        QVERIFY(plugin.syncLibp2pStop());
    }

    void testSyncStreamWriteLp()
    {
        Libp2pModulePlugin plugin;
        QVERIFY(plugin.syncLibp2pStart());

        uint64_t fakeStreamId = 1234;
        QByteArray data = "hello-stream-lp";

        // cannot writeLp to inexistent stream
        QVERIFY(!plugin.syncStreamWriteLp(fakeStreamId, data));

        QVERIFY(plugin.syncLibp2pStop());
    }


    /* ---------------------------
     * Kademlia
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

