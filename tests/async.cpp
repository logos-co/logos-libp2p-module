#include <QtTest>
#include <plugin.h>

class TestLibp2pModule : public QObject
{
    Q_OBJECT

private:
    /* ---------------------------
     * Shared helpers
     * --------------------------- */

    using EventSpy = QSignalSpy;

    static std::unique_ptr<EventSpy> createLibp2pEventSpy(QObject *obj)
    {
        return std::make_unique<EventSpy>(
            obj,
            SIGNAL(libp2pEvent(int,QString,QString,QString,QVariant))
        );
    }

    static void fail(const char *msg)
    {
        QTest::qFail(msg, __FILE__, __LINE__);
    }

    /* ---------------------------
     * Replacement helper for synchronous API tests
     * --------------------------- */
    static WaitResult waitForUuid(
        Libp2pModulePlugin &plugin,
        QSignalSpy &spy,
        const QString &uuid,
        const QString &caller,
        int timeoutMs = 5000
    )
    {
        WaitResult result{false, QVariant{}};
        if (uuid.isEmpty()) {
            return result;
        }

        QElapsedTimer timer;
        timer.start();

        while (timer.elapsed() < timeoutMs) {
            for (int i = 0; i < spy.count(); ++i) {
                auto args = spy.at(i);
                QString eventUuid = args.at(1).toString();
                QString eventCaller = args.at(2).toString();

                // ignore libp2pNew and libp2pDestroy events
                if (eventCaller == "libp2pNew" || eventCaller == "libp2pDestroy") {
                    spy.remove(i);
                    break;
                }

                if (eventUuid == uuid && eventCaller == caller) {
                    result.ok = (args.at(0).toInt() == RET_OK);
                    result.data = args.at(4);
                    spy.remove(i);
                    return result;
                }
            }

            // wait for new signals if none matched yet
            if (!spy.wait(50)) {
                QCoreApplication::processEvents();
            }
        }

        // return a default value so the compiler is happy
        return WaitResult{false, QVariant{}};
    }



    static void waitForEvents(EventSpy &spy, int expected, int timeout = 5000)
    {
        QElapsedTimer timer;
        timer.start();

        while (spy.count() < expected) {
            if (!spy.wait(timeout - timer.elapsed())) {
                fail("Timeout waiting for expected signals");
                return;
            }
        }
    }

    static QList<QVariant> takeEvent(EventSpy &spy)
    {
        if (spy.count() <= 0) {
            fail("Expected event but spy is empty");
            throw std::runtime_error("empty spy");
        }
        return spy.takeFirst();
    }

    /* ---------------------------
     * Plugin lifecycle helpers
     * --------------------------- */

    static void startPlugin(Libp2pModulePlugin &plugin, EventSpy &spy)
    {
        QString uuid = plugin.libp2pStart();
        auto res = waitForUuid(plugin, spy, uuid, "libp2pStart");
        QVERIFY(res.ok);
    }

    static void stopPlugin(Libp2pModulePlugin &plugin, EventSpy &spy)
    {
        QString uuid = plugin.libp2pStop();
        auto res = waitForUuid(plugin, spy, uuid, "libp2pStop");
        QVERIFY(res.ok);
    }

private slots:

    /* ---------------------------
     * Start/stop
     * --------------------------- */

    void testStartStop()
    {
        Libp2pModulePlugin plugin;
        auto spy = createLibp2pEventSpy(&plugin);
        startPlugin(plugin, *spy);
        stopPlugin(plugin, *spy);
    }

    /* ---------------------------
     * Connectivity tests
     * --------------------------- */

    void testConnectPeer()
    {
        Libp2pModulePlugin plugin;
        auto spy = createLibp2pEventSpy(&plugin);
        startPlugin(plugin, *spy);

        QString fakePeer =
            "12D3KooWInvalidPeerForTest";

        QList<QString> fakeAddrs = {
            "/ip4/127.0.0.1/tcp/9999"
        };

        QString uuid = plugin.connectPeer(fakePeer, fakeAddrs);
        auto res = waitForUuid(plugin, *spy, uuid, "connectPeer");

        // should return false, peer is fake
        QVERIFY(!res.ok);

        stopPlugin(plugin, *spy);
    }

    void testDisconnectPeer()
    {
        Libp2pModulePlugin plugin;
        auto spy = createLibp2pEventSpy(&plugin);
        startPlugin(plugin, *spy);

        QString fakePeer =
            "12D3KooWInvalidPeerForTest";

        QString uuid = plugin.disconnectPeer(fakePeer);
        auto res = waitForUuid(plugin, *spy, uuid, "disconnectPeer");

        // should return false, peer is fake
        QVERIFY(!res.ok);

        stopPlugin(plugin, *spy);
    }

    void testPeerInfo()
    {
        Libp2pModulePlugin plugin;
        auto spy = createLibp2pEventSpy(&plugin);
        startPlugin(plugin, *spy);

        QString uuid = plugin.peerInfo();
        auto res = waitForUuid(plugin, *spy, uuid, "peerInfo");

        QVERIFY(res.ok);

        PeerInfo peerInfo = res.data.value<PeerInfo>();
        QVERIFY(!peerInfo.peerId.isEmpty());

        stopPlugin(plugin, *spy);
    }

    void testConnectedPeers()
    {
        Libp2pModulePlugin plugin;
        auto spy = createLibp2pEventSpy(&plugin);
        startPlugin(plugin, *spy);

        QString uuid = plugin.connectedPeers();
        auto res = waitForUuid(plugin, *spy, uuid, "connectedPeers");

        QVERIFY(res.ok);

        QCOMPARE(res.data.value<QList<QString>>().size(), 0);

        stopPlugin(plugin, *spy);
    }

    void testDial()
    {
        Libp2pModulePlugin plugin;
        auto spy = createLibp2pEventSpy(&plugin);
        startPlugin(plugin, *spy);

        QString fakePeer =
            "12D3KooWInvalidPeerForTest";

        QString proto = "/test/1.0.0";

        QString uuid = plugin.dial(fakePeer, proto);
        auto res = waitForUuid(plugin, *spy, uuid, "dial");

        // cannot dial fake peer
        QVERIFY(!res.ok);

        uint64_t streamId = res.data.value<uint64_t>();
        QCOMPARE(streamId, 0);

        stopPlugin(plugin, *spy);
    }

    /* ---------------------------
     * Stream tests
     * --------------------------- */

    void testStreamClose()
    {
        Libp2pModulePlugin plugin;
        auto spy = createLibp2pEventSpy(&plugin);
        startPlugin(plugin, *spy);

        uint64_t fakeStreamId = 1234;
        QString uuid = plugin.streamClose(fakeStreamId);
        auto res = waitForUuid(plugin, *spy, uuid, "streamClose");

        // cannot close inexistent stream
        QVERIFY(!res.ok);

        stopPlugin(plugin, *spy);
    }

    void testStreamCloseWithEOF()
    {
        Libp2pModulePlugin plugin;
        auto spy = createLibp2pEventSpy(&plugin);
        startPlugin(plugin, *spy);

        uint64_t fakeStreamId = 1234;
        QString uuid = plugin.streamCloseWithEOF(fakeStreamId);
        auto res = waitForUuid(plugin, *spy, uuid, "streamCloseWithEOF");

        // cannot closeWithEOF inexistent stream
        QVERIFY(!res.ok);

        stopPlugin(plugin, *spy);
    }

    void testStreamRelease()
    {
        Libp2pModulePlugin plugin;
        auto spy = createLibp2pEventSpy(&plugin);
        startPlugin(plugin, *spy);

        uint64_t fakeStreamId = 1234;
        QString uuid = plugin.streamRelease(fakeStreamId);
        auto res = waitForUuid(plugin, *spy, uuid, "streamRelease");

        // cannot release inexistent stream
        QVERIFY(!res.ok);

        stopPlugin(plugin, *spy);
    }

    void testStreamReadExactly()
    {
        Libp2pModulePlugin plugin;
        auto spy = createLibp2pEventSpy(&plugin);
        startPlugin(plugin, *spy);

        uint64_t fakeStreamId = 1234;
        size_t len = 16;
        QString uuid = plugin.streamReadExactly(fakeStreamId, len);
        auto res = waitForUuid(plugin, *spy, uuid, "streamReadExactly");

        // cannot readExactly from inexistent stream
        QVERIFY(!res.ok);

        stopPlugin(plugin, *spy);
    }

    void testStreamReadLp()
    {
        Libp2pModulePlugin plugin;
        auto spy = createLibp2pEventSpy(&plugin);
        startPlugin(plugin, *spy);

        uint64_t fakeStreamId = 1234;
        size_t maxSize = 4096;
        QString uuid = plugin.streamReadLp(fakeStreamId, maxSize);
        auto res = waitForUuid(plugin, *spy, uuid, "streamReadLp");

        // cannot readLp from inexistent stream
        QVERIFY(!res.ok);

        stopPlugin(plugin, *spy);
    }

    void testStreamWrite()
    {
        Libp2pModulePlugin plugin;
        auto spy = createLibp2pEventSpy(&plugin);
        startPlugin(plugin, *spy);

        uint64_t fakeStreamId = 1234;
        QByteArray data = "hello-stream";
        QString uuid = plugin.streamWrite(fakeStreamId, data);
        auto res = waitForUuid(plugin, *spy, uuid, "streamWrite");

        // cannot write to inexistent stream
        QVERIFY(!res.ok);

        stopPlugin(plugin, *spy);
    }

    void testStreamWriteLp()
    {
        Libp2pModulePlugin plugin;
        auto spy = createLibp2pEventSpy(&plugin);
        startPlugin(plugin, *spy);

        uint64_t fakeStreamId = 1234;
        QByteArray data = "hello-stream-lp";
        QString uuid = plugin.streamWriteLp(fakeStreamId, data);
        auto res = waitForUuid(plugin, *spy, uuid, "streamWriteLp");

        // cannot writeLp to inexistent stream
        QVERIFY(!res.ok);

        stopPlugin(plugin, *spy);
    }

    /* ---------------------------
     * Kademlia tests
     * --------------------------- */

    void testKadGetPutValue()
    {
        Libp2pModulePlugin plugin;
        auto spy = createLibp2pEventSpy(&plugin);
        startPlugin(plugin, *spy);

        QByteArray key = "test-key";
        QByteArray value = "hello-world";

        QString uuid = plugin.kadPutValue(key, value);
        auto res = waitForUuid(plugin, *spy, uuid, "kadPutValue");
        QVERIFY(res.ok);

        uuid = plugin.kadGetValue(key, 1);
        res = waitForUuid(plugin, *spy, uuid, "kadGetValue");
        QVERIFY(res.ok);
        QCOMPARE(res.data, value);

        stopPlugin(plugin, *spy);
    }

    void testKeyToCid()
    {
        Libp2pModulePlugin plugin;
        auto spy = createLibp2pEventSpy(&plugin);
        startPlugin(plugin, *spy);

        QByteArray key = "some-key";
        QVERIFY(!plugin.toCid(key).isEmpty());
        waitForEvents(*spy, 1);
        auto args = takeEvent(*spy);

        QCOMPARE(args.at(2).toString(), "toCid");

        // in toCid msg is set to the cid string
        QByteArray cidBytes = args.at(3).toByteArray();
        QVERIFY(!cidBytes.isEmpty());
    }


    void testKadGetProviders()
    {
        Libp2pModulePlugin plugin;
        auto spy = createLibp2pEventSpy(&plugin);
        startPlugin(plugin, *spy);

        QByteArray key = "provider-test-key";
        QByteArray value = "provider-test-value";

        // ---- 1: Generate CID from key ----
        QVERIFY(!plugin.toCid(key).isEmpty());
        waitForEvents(*spy, 1);

        auto cidEvent = takeEvent(*spy);
        QCOMPARE(cidEvent.at(2).toString(), "toCid");

        QString cid =
            QString::fromUtf8(cidEvent.at(3).toByteArray());

        QVERIFY(!cid.isEmpty());

        // ---- 2: Put value so key exists in DHT ----
        QString uuid = plugin.kadPutValue(key, value);
        auto res = waitForUuid(plugin, *spy, uuid, "kadPutValue");
        QVERIFY(res.ok);

        // ---- 3: Register provider ----
        uuid = plugin.kadAddProvider(cid);
        res = waitForUuid(plugin, *spy, uuid, "kadAddProvider");
        QVERIFY(res.ok);

        // ---- 4: Query providers ----
        uuid = plugin.kadGetProviders(cid);
        res = waitForUuid(plugin, *spy, uuid, "kadGetProviders");
        QVERIFY(res.ok);
        QVERIFY(res.data.isValid());

        QVariantList providers = res.data.toList();

        // We expect at least one provider (ourselves)
        // TODO: this should not be empty, but for that we need more peers
        // TODO: QVERIFY(!providers.isEmpty());

        stopPlugin(plugin, *spy);
    }

    void testKadGetRandomRecords()
    {
        Libp2pModulePlugin plugin;
        auto spy = createLibp2pEventSpy(&plugin);
        startPlugin(plugin, *spy);

        QString uuid = plugin.kadGetRandomRecords();
        auto res = waitForUuid(plugin, *spy, uuid, "kadGetRandomRecords");

        // should fail: kademlia discovery is not mounted
        QVERIFY(!res.ok);

        // QVERIFY(res.data.isValid());
        // QVariantList randomRecords = res.data.toList();

        // We expect at least one record (ourselves)
        // TODO: this should not be empty, but for that we need more peers
        // TODO: QVERIFY(!randomRecords.isEmpty());

        stopPlugin(plugin, *spy);
    }

    /* ---------------------------
     * Mix tests
     * --------------------------- */

    void testMixGeneratePrivKey()
    {
        Libp2pModulePlugin plugin;
        auto spy = createLibp2pEventSpy(&plugin);
        startPlugin(plugin, *spy);

        QByteArray data = plugin.mixGeneratePrivKey();
        QCOMPARE(data.size(), sizeof(libp2p_curve25519_key_t));

        stopPlugin(plugin, *spy);
    }

    void testMixPublicKey()
    {
        Libp2pModulePlugin plugin;
        auto spy = createLibp2pEventSpy(&plugin);
        startPlugin(plugin, *spy);

        QByteArray privKey = plugin.mixGeneratePrivKey();
        QCOMPARE(privKey.size(), sizeof(libp2p_curve25519_key_t));

        QByteArray pubKey = plugin.mixPublicKey(privKey);
        QCOMPARE(pubKey.size(), sizeof(libp2p_curve25519_key_t));

        stopPlugin(plugin, *spy);
    }

    void testMixDial()
    {
        Libp2pModulePlugin plugin;
        auto spy = createLibp2pEventSpy(&plugin);
        startPlugin(plugin, *spy);

        QString fakePeer = "12D3KooWInvalidMixPeer";
        QString addr = "/ip4/127.0.0.1/tcp/9999";
        QString proto = "/mix/test/1.0.0";

        QString uuid = plugin.mixDial(fakePeer, addr, proto);
        auto res = waitForUuid(plugin, *spy, uuid, "mixDial");

        // expected to fail without a mix node
        QVERIFY(!res.ok);

        stopPlugin(plugin, *spy);
    }

    void testMixDialWithReply()
    {
        Libp2pModulePlugin plugin;
        auto spy = createLibp2pEventSpy(&plugin);
        startPlugin(plugin, *spy);

        QString fakePeer = "12D3KooWInvalidMixPeer";
        QString addr = "/ip4/127.0.0.1/tcp/9999";
        QString proto = "/mix/test/1.0.0";

        QString uuid = plugin.mixDialWithReply(
            fakePeer,
            addr,
            proto,
            1,
            2
        );

        auto res = waitForUuid(plugin, *spy, uuid, "mixDialWithReply");

        QVERIFY(!res.ok);

        stopPlugin(plugin, *spy);
    }

    void testMixRegisterDestReadBehavior()
    {
        Libp2pModulePlugin plugin;
        auto spy = createLibp2pEventSpy(&plugin);
        startPlugin(plugin, *spy);

        QString proto = "/mix/test/1.0.0";

        QString uuid = plugin.mixRegisterDestReadBehavior(
            proto,
            LIBP2P_MIX_READ_EXACTLY,
            1024
        );

        auto res = waitForUuid(
            plugin,
            *spy,
            uuid,
            "mixRegisterDestReadBehavior"
        );

        // data should not be valid (cant read)
        QVERIFY(!res.data.isValid());

        stopPlugin(plugin, *spy);
    }

    void testMixSetNodeInfo()
    {
        Libp2pModulePlugin plugin;
        auto spy = createLibp2pEventSpy(&plugin);
        startPlugin(plugin, *spy);

        QByteArray key = plugin.mixGeneratePrivKey();

        // get peerInfo
        QString uuid = plugin.peerInfo();
        auto res = waitForUuid(plugin, *spy, uuid, "peerInfo");
        PeerInfo peerInfo = res.data.value<PeerInfo>();

        // set mix node info
        uuid = plugin.mixSetNodeInfo(peerInfo.addrs[0], key);
        res = waitForUuid(plugin, *spy, uuid, "mixSetNodeInfo");
        QVERIFY(res.ok);

        stopPlugin(plugin, *spy);
    }

    void testMixNodepoolAdd()
    {
        Libp2pModulePlugin plugin;
        auto spy = createLibp2pEventSpy(&plugin);
        startPlugin(plugin, *spy);

        QString fakePeer = "12D3KooWInvalidMixPeer";
        QString addr = "/ip4/127.0.0.1/tcp/9999";

        QByteArray privKey = plugin.mixGeneratePrivKey();
        QByteArray pubKey = plugin.mixPublicKey(privKey);

        QByteArray fakeLibp2pKey(33, 0x01);

        QString uuid = plugin.mixNodepoolAdd(
            fakePeer,
            addr,
            pubKey,
            fakeLibp2pKey
        );

        auto res = waitForUuid(plugin, *spy, uuid, "mixNodepoolAdd");

        // fake peer doesnt exist
        QVERIFY(!res.data.isValid());

        stopPlugin(plugin, *spy);
    }
};

QTEST_MAIN(TestLibp2pModule)
#include "async.moc"

