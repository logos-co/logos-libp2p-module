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
     * Construction + destruction
     * --------------------------- */

    void testConstruction()
    {
        Libp2pModulePlugin plugin;
        QVERIFY(true);
    }

    void testStartStop()
    {
        Libp2pModulePlugin plugin;
        auto spy = createLibp2pEventSpy(&plugin);
        startPlugin(plugin, *spy);
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

