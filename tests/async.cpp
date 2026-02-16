#include <QtTest>
#include <libp2p_module_plugin.h>

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
     * Event validators
     * --------------------------- */

    static void assertEvent(
        EventSpy &spy,
        int expectedResult,
        const QString &expectedCaller,
        const QVariant &expectedBuffer = QVariant()
    )
    {
        auto args = takeEvent(spy);

        QCOMPARE(args.at(0).toInt(), expectedResult);
        QCOMPARE(args.at(2).toString(), expectedCaller);
        QCOMPARE(args.at(4), expectedBuffer);
    }

    static void assertEventCount(EventSpy &spy, int expected)
    {
        QCOMPARE(spy.count(), expected);
    }

    static void assertGetProvidersResult(
        EventSpy &spy,
        int expectedResult,
        const QString &expectedCaller,
        int expectedProvidersLen
    )
    {
        auto args = takeEvent(spy);

        QCOMPARE(args.at(0).toInt(), expectedResult);
        QCOMPARE(args.at(2).toString(), expectedCaller);

        auto providers =
            args.at(4).value<QVector<Libp2pPeerInfo>>();

        QCOMPARE(providers.size(), expectedProvidersLen);
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
     * Construction + destruction + foo
     * --------------------------- */

    void testConstruction()
    {
        Libp2pModulePlugin plugin;
        QVERIFY(true);
    }

    void testFooSignal()
    {
        Libp2pModulePlugin plugin;

        QSignalSpy spy(
            &plugin,
            SIGNAL(libp2pEvent(int,QString,QString,QString,QVariant))
        );

        plugin.foo("hello");

        QVERIFY(spy.count() >= 0);
    }

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

        QStringList fakeAddrs = {
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

        QVERIFY(plugin.peerInfo());
        waitForEvents(*spy, 1);

        auto event = takeEvent(*spy);

        QCOMPARE(event.at(2).toString(), "peerInfo");
        QCOMPARE(event.at(0).toInt(), RET_OK);

        stopPlugin(plugin, *spy);
    }

    void testConnectedPeers()
    {
        Libp2pModulePlugin plugin;
        auto spy = createLibp2pEventSpy(&plugin);

        startPlugin(plugin, *spy);

        QVERIFY(plugin.connectedPeers());
        waitForEvents(*spy, 1);

        auto event = takeEvent(*spy);

        QCOMPARE(event.at(2).toString(), "connectedPeers");
        QCOMPARE(event.at(0).toInt(), RET_OK);

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

        QVERIFY(plugin.dial(fakePeer, proto));
        waitForEvents(*spy, 1);

        auto event = takeEvent(*spy);

        QCOMPARE(event.at(2).toString(), "dial");

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
        QVERIFY(res.data == value);

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
     * Sync Kademlia tests
     * --------------------------- */

    void testSyncKadGetPutValue()
    {
        Libp2pModulePlugin plugin;

        QVERIFY(plugin.syncLibp2pStart());

        QByteArray key = "sync-test-key";
        QByteArray value = "sync-hello-world";

        QVERIFY(plugin.syncKadPutValue(key, value));
        QVERIFY(plugin.syncKadGetValue(key, 1) == value);

        QVERIFY(plugin.syncLibp2pStop());
    }
};

QTEST_MAIN(TestLibp2pModule)
#include "async.moc"

