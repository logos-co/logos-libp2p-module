#include <QtTest>
#include <plugin.h>

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
        QVERIFY(plugin.syncLibp2pStart().ok);

        QString fakePeer =
            "12D3KooWInvalidPeerForTest";

        QList<QString> fakeAddrs = {
            "/ip4/127.0.0.1/tcp/9999"
        };

        // connect and disconnect should fail
        QVERIFY(!plugin.syncConnectPeer(fakePeer, fakeAddrs).ok);
        QVERIFY(!plugin.syncDisconnectPeer(fakePeer).ok);

        QVERIFY(plugin.syncLibp2pStop().ok);
    }

    void testSyncPeerInfo()
    {
        Libp2pModulePlugin plugin;
        QVERIFY(plugin.syncLibp2pStart().ok);

        auto res = plugin.syncPeerInfo();
        QVERIFY(res.ok);

        PeerInfo peerInfo = res.data.value<PeerInfo>();
        QVERIFY(!peerInfo.peerId.isEmpty());

        QVERIFY(plugin.syncLibp2pStop().ok);
    }

    void testSyncConnectedPeers()
    {
        Libp2pModulePlugin plugin;
        QVERIFY(plugin.syncLibp2pStart().ok);

        auto res = plugin.syncConnectedPeers();
        QVERIFY(res.ok);

        QList<QString> connectedPeers = res.data.value<QList<QString>>();
        QCOMPARE(connectedPeers.size(), 0);

        QVERIFY(plugin.syncLibp2pStop().ok);
    }

    void testSyncDial()
    {
        Libp2pModulePlugin plugin;
        QVERIFY(plugin.syncLibp2pStart().ok);

        QString fakePeer =
            "12D3KooWInvalidPeerForTest";

        QString proto = "/test/1.0.0";

        QVERIFY(!plugin.syncDial(fakePeer, proto).ok);

        QVERIFY(plugin.syncLibp2pStop().ok);
    }

    /* ---------------------------
     * Stream
     * --------------------------- */

    void testSyncStreamClose()
    {
        Libp2pModulePlugin plugin;
        QVERIFY(plugin.syncLibp2pStart().ok);

        uint64_t fakeStreamId = 1234;

        // cannot close inexistent stream
        QVERIFY(!plugin.syncStreamClose(fakeStreamId).ok);

        QVERIFY(plugin.syncLibp2pStop().ok);
    }

    void testSyncStreamCloseWithEOF()
    {
        Libp2pModulePlugin plugin;
        QVERIFY(plugin.syncLibp2pStart().ok);

        uint64_t fakeStreamId = 1234;

        // cannot closeWithEOF inexistent stream
        QVERIFY(!plugin.syncStreamCloseWithEOF(fakeStreamId).ok);

        QVERIFY(plugin.syncLibp2pStop().ok);
    }

    void testSyncStreamRelease()
    {
        Libp2pModulePlugin plugin;
        QVERIFY(plugin.syncLibp2pStart().ok);

        uint64_t fakeStreamId = 1234;

        // cannot release inexistent stream
        QVERIFY(!plugin.syncStreamRelease(fakeStreamId).ok);

        QVERIFY(plugin.syncLibp2pStop().ok);
    }

    void testSyncStreamReadExactly()
    {
        Libp2pModulePlugin plugin;
        QVERIFY(plugin.syncLibp2pStart().ok);

        uint64_t fakeStreamId = 1234;
        size_t len = 16;

        // cannot readExactly from inexistent stream
        QVERIFY(!plugin.syncStreamReadExactly(fakeStreamId, len).ok);

        QVERIFY(plugin.syncLibp2pStop().ok);
    }

    void testSyncStreamReadLp()
    {
        Libp2pModulePlugin plugin;
        QVERIFY(plugin.syncLibp2pStart().ok);

        uint64_t fakeStreamId = 1234;
        size_t maxSize = 4096;

        // cannot readLp from inexistent stream
        QVERIFY(!plugin.syncStreamReadLp(fakeStreamId, maxSize).ok);

        QVERIFY(plugin.syncLibp2pStop().ok);
    }

    void testSyncStreamWrite()
    {
        Libp2pModulePlugin plugin;
        QVERIFY(plugin.syncLibp2pStart().ok);

        uint64_t fakeStreamId = 1234;
        QByteArray data = "hello-stream";

        // cannot write to inexistent stream
        QVERIFY(!plugin.syncStreamWrite(fakeStreamId, data).ok);

        QVERIFY(plugin.syncLibp2pStop().ok);
    }

    void testSyncStreamWriteLp()
    {
        Libp2pModulePlugin plugin;
        QVERIFY(plugin.syncLibp2pStart().ok);

        uint64_t fakeStreamId = 1234;
        QByteArray data = "hello-stream-lp";

        // cannot writeLp to inexistent stream
        QVERIFY(!plugin.syncStreamWriteLp(fakeStreamId, data).ok);

        QVERIFY(plugin.syncLibp2pStop().ok);
    }


    /* ---------------------------
     * Kademlia
     * --------------------------- */

    void testSyncKadGetPutValue()
    {
        Libp2pModulePlugin plugin;

        QVERIFY(plugin.syncLibp2pStart().ok);

        QByteArray key = "sync-test-key";
        QByteArray value = "sync-hello-world";

        QVERIFY(plugin.syncKadPutValue(key, value).ok);

        auto res = plugin.syncKadGetValue(key, 1);
        QVERIFY(res.ok);
        QByteArray actual = res.data.value<QByteArray>();
        QCOMPARE(actual, value);

        QVERIFY(plugin.syncLibp2pStop().ok);
    }

    void testSyncKeyToCidAndProviders()
    {
        Libp2pModulePlugin plugin;

        QVERIFY(plugin.syncLibp2pStart().ok);

        QByteArray key = "sync-provider-test-key";
        QByteArray value = "sync-provider-test-value";

        auto res = plugin.syncToCid(key);
        QVERIFY(res.ok);

        QString cid = res.data.value<QString>();
        QVERIFY(!cid.isEmpty());

        QVERIFY(plugin.syncKadPutValue(key, value).ok);
        QVERIFY(plugin.syncKadAddProvider(cid).ok);

        // no providers are registered yet
        res = plugin.syncKadGetProviders(cid);
        QVERIFY(res.ok);

        QList<PeerInfo> providers = res.data.value<QList<PeerInfo>>();
        QCOMPARE(providers.size(), 0);

        QVERIFY(plugin.syncLibp2pStop().ok);
    }

    void testSyncKadFindNode()
    {
        Libp2pModulePlugin plugin;

        QVERIFY(plugin.syncLibp2pStart().ok);

        QString fakePeer = "12D3KooWInvalidPeerForSyncTest";

        // no nodes are registered yet
        QVERIFY(!plugin.syncKadFindNode(fakePeer).ok);

        QVERIFY(plugin.syncLibp2pStop().ok);
    }

    void testSyncKadProvideLifecycle()
    {
        Libp2pModulePlugin plugin;

        QVERIFY(plugin.syncLibp2pStart().ok);

        QByteArray key = "sync-providing-key";

        auto res = plugin.syncToCid(key);
        QVERIFY(res.ok);

        QString cid = res.data.value<QString>();
        QVERIFY(!cid.isEmpty());

        QVERIFY(plugin.syncKadStartProviding(cid).ok);
        QVERIFY(plugin.syncKadStopProviding(cid).ok);

        QVERIFY(plugin.syncLibp2pStop().ok);
    }

    void testSyncKadRandomRecords()
    {
        Libp2pModulePlugin plugin;

        QVERIFY(plugin.syncLibp2pStart().ok);

        // no registered records yet
        QVERIFY(!plugin.syncKadGetRandomRecords().ok);

        QVERIFY(plugin.syncLibp2pStop().ok);
    }

     /* ---------------------------
     * Mix
     * --------------------------- */

    void testSyncMixDial()
    {
        Libp2pModulePlugin plugin;
        QVERIFY(plugin.syncLibp2pStart().ok);

        QString fakePeer = "12D3KooWInvalidMixPeer";
        QString addr = "/ip4/127.0.0.1/tcp/9999";
        QString proto = "/mix/test/1.0.0";

        // expected to fail without a configured mix network
        QVERIFY(!plugin.syncMixDial(fakePeer, addr, proto).ok);

        QVERIFY(plugin.syncLibp2pStop().ok);
    }

    void testSyncMixDialWithReply()
    {
        Libp2pModulePlugin plugin;
        QVERIFY(plugin.syncLibp2pStart().ok);

        QString fakePeer = "12D3KooWInvalidMixPeer";
        QString addr = "/ip4/127.0.0.1/tcp/9999";
        QString proto = "/mix/test/1.0.0";

        QVERIFY(!plugin.syncMixDialWithReply(
            fakePeer,
            addr,
            proto,
            1,
            2
        ).ok);

        QVERIFY(plugin.syncLibp2pStop().ok);
    }

    void testSyncMixRegisterDestReadBehavior()
    {
        Libp2pModulePlugin plugin;
        QVERIFY(plugin.syncLibp2pStart().ok);

        QString proto = "/mix/test/1.0.0";

        auto res = plugin.syncMixRegisterDestReadBehavior(
            proto,
            LIBP2P_MIX_READ_EXACTLY,
            1024
        );

        // data should not be valid (cant read)
        QVERIFY(!res.data.isValid());

        QVERIFY(plugin.syncLibp2pStop().ok);
    }

    void testSyncMixSetNodeInfo()
    {
        Libp2pModulePlugin plugin;
        QVERIFY(plugin.syncLibp2pStart().ok);

        QString addr = "/ip4/127.0.0.1/tcp/4001";
        PeerInfo peerInfo = plugin.syncPeerInfo().data.value<PeerInfo>();

        QVERIFY(plugin.syncMixSetNodeInfo(peerInfo.addrs[0], plugin.mixGeneratePrivKey()).ok);

        QVERIFY(plugin.syncLibp2pStop().ok);
    }

    void testSyncMixNodepoolAdd()
    {
        Libp2pModulePlugin plugin;
        QVERIFY(plugin.syncLibp2pStart().ok);

        QString fakePeer = "12D3KooWInvalidMixPeer";
        QString addr = "/ip4/127.0.0.1/tcp/9999";

        QByteArray mixPubKey = plugin.mixPublicKey(plugin.mixGeneratePrivKey());

        QByteArray fakeLibp2pKey(33, 0x01);

        auto res = plugin.syncMixNodepoolAdd(
            fakePeer,
            addr,
            mixPubKey,
            fakeLibp2pKey
        );

        QVERIFY(!res.data.isValid());

        QVERIFY(plugin.syncLibp2pStop().ok);
    }

};

QTEST_MAIN(TestLibp2pModuleSync)
#include "sync.moc"

