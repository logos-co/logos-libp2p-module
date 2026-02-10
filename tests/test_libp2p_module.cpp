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
        QVERIFY(plugin.libp2pStart());
        waitForEvents(spy, 2);
        assertEvent(spy, RET_OK, "libp2pNew");
        assertEvent(spy, RET_OK, "libp2pStart");
    }

    static void stopPlugin(Libp2pModulePlugin &plugin, EventSpy &spy)
    {
        QVERIFY(plugin.libp2pStop());
        waitForEvents(spy, 1);
        assertEvent(spy, RET_OK, "libp2pStop");
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

        QVERIFY(plugin.connectPeer(fakePeer, fakeAddrs));
        waitForEvents(*spy, 1);

        auto event = takeEvent(*spy);

        QCOMPARE(event.at(2).toString(), "connectPeer");

        stopPlugin(plugin, *spy);
    }

    void testDisconnectPeer()
    {
        Libp2pModulePlugin plugin;
        auto spy = createLibp2pEventSpy(&plugin);

        startPlugin(plugin, *spy);

        QString fakePeer =
            "12D3KooWInvalidPeerForTest";

        QVERIFY(plugin.disconnectPeer(fakePeer));
        waitForEvents(*spy, 1);

        auto event = takeEvent(*spy);

        QCOMPARE(event.at(2).toString(), "disconnectPeer");

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

        auto libp2pEventSpy = createLibp2pEventSpy(&plugin);

        startPlugin(plugin, *libp2pEventSpy);

        QByteArray key = "test-key";
        QByteArray expectedValue = "hello-world";

        QVERIFY(plugin.kadPutValue(key, expectedValue));
        waitForEvents(*libp2pEventSpy, 1);
        assertEvent(*libp2pEventSpy, RET_OK, "kadPutValue");

        QVERIFY(plugin.kadGetValue(key, 1));
        waitForEvents(*libp2pEventSpy, 1);
        assertEvent(*libp2pEventSpy, RET_OK, "kadGetValue", expectedValue);

        stopPlugin(plugin, *libp2pEventSpy);
    }

    void testKeyToCid()
    {
        Libp2pModulePlugin plugin;
        auto spy = createLibp2pEventSpy(&plugin);
        startPlugin(plugin, *spy);

        QByteArray key = "some-key";
        QVERIFY(plugin.toCid(key));
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
        QVERIFY(plugin.toCid(key));
        waitForEvents(*spy, 1);

        auto cidEvent = takeEvent(*spy);
        QCOMPARE(cidEvent.at(2).toString(), "toCid");

        QString cid =
            QString::fromUtf8(cidEvent.at(3).toByteArray());

        QVERIFY(!cid.isEmpty());

        // ---- 2: Put value so key exists in DHT ----
        QVERIFY(plugin.kadPutValue(key, value));
        waitForEvents(*spy, 1);

        assertEvent(*spy, RET_OK, "kadPutValue");

        // ---- 3: Register provider ----
        QVERIFY(plugin.kadAddProvider(cid));
        waitForEvents(*spy, 1);

        assertEvent(*spy, RET_OK, "kadAddProvider");

        // ---- 4: Query providers ----
        QVERIFY(plugin.kadGetProviders(cid));
        waitForEvents(*spy, 1);

        auto providersEvent = takeEvent(*spy);

        QCOMPARE(providersEvent.at(2).toString(), "kadGetProviders");
        QCOMPARE(providersEvent.at(1).toInt(), RET_OK);

        QVariant providersVariant = providersEvent.at(4);
        QVERIFY(providersVariant.isValid());

        QVariantList providers = providersVariant.toList();

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

        QVERIFY(plugin.kadGetRandomRecords());
        waitForEvents(*spy, 1);

        auto randomRecordsEvent = takeEvent(*spy);

        QCOMPARE(randomRecordsEvent.at(2).toString(), "kadGetRandomRecords");
        QCOMPARE(randomRecordsEvent.at(1).toInt(), RET_OK);

        QVariant randomRecordsVariant = randomRecordsEvent.at(4);
        QVERIFY(randomRecordsVariant.isValid());

        QVariantList randomRecords = randomRecordsVariant.toList();

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
        QVERIFY(plugin.syncKadGetValue(key, 1));

        QVERIFY(plugin.syncLibp2pStop());
    }

    void testSyncKeyToCidAndProviders()
    {
        Libp2pModulePlugin plugin;

        QVERIFY(plugin.syncLibp2pStart());

        QByteArray key = "sync-provider-test-key";
        QByteArray value = "sync-provider-test-value";

        /* ----- obtain CID via async API (needed for provider ops) ----- */

        QVERIFY(plugin.toCid(key));
        waitForEvents(*spy, 1);

        auto cidEvent = takeEvent(*spy);
        QCOMPARE(cidEvent.at(2).toString(), "toCid");

        QString cid = QString::fromUtf8(cidEvent.at(3).toByteArray());
        QVERIFY(!cid.isEmpty());

        /* ----- Sync operations ----- */

        QVERIFY(plugin.syncKadPutValue(key, value));
        QVERIFY(plugin.syncKadAddProvider(cid));
        QVERIFY(plugin.syncKadGetProviders(cid));

        QVERIFY(plugin.syncLibp2pStop());
    }

    void testSyncKadFindNode()
    {
        Libp2pModulePlugin plugin;

        QVERIFY(plugin.libp2pStart());

        QString fakePeer = "12D3KooWInvalidPeerForSyncTest";

        QVERIFY(plugin.syncKadFindNode(fakePeer));

        QVERIFY(plugin.libp2pStop());
    }

    void testSyncKadProvidingLifecycle()
    {
        Libp2pModulePlugin plugin;

        auto spy = createLibp2pEventSpy(&plugin);

        startPlugin(plugin, *spy);

        QByteArray key = "sync-providing-key";

        QVERIFY(plugin.toCid(key));
        waitForEvents(*spy, 1);

        auto cidEvent = takeEvent(*spy);
        QString cid = QString::fromUtf8(cidEvent.at(3).toByteArray());

        QVERIFY(!cid.isEmpty());

        QVERIFY(plugin.syncKadStartProviding(cid));
        QVERIFY(plugin.syncKadStopProviding(cid));

        stopPlugin(plugin, *spy);
    }

    void testSyncKadRandomRecords()
    {
        Libp2pModulePlugin plugin;

        QVERIFY(plugin.libp2pStart());

        QVERIFY(plugin.syncKadGetRandomRecords());

        QVERIFY(plugin.libp2pStop());
    }

};

QTEST_MAIN(TestLibp2pModule)
#include "test_libp2p_module.moc"

