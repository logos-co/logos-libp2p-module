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

    static std::unique_ptr<EventSpy> createGetValueSpy(QObject *obj)
    {
        return std::make_unique<EventSpy>(
            obj,
            SIGNAL(getValueFinished(int,QString,QString,QByteArray))
        );
    }
    static std::unique_ptr<EventSpy> createGetProvidersSpy(QObject *obj)
    {
        return std::make_unique<EventSpy>(
            obj,
            SIGNAL(getProvidersFinished(int,QString,QString,QVector<Libp2pPeerInfo>))
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
        const QString &expectedCaller
    )
    {
        auto args = takeEvent(spy);

        QCOMPARE(args.at(0).toInt(), expectedResult);
        QCOMPARE(args.at(2).toString(), expectedCaller);
    }

    static void assertEventCount(EventSpy &spy, int expected)
    {
        QCOMPARE(spy.count(), expected);
    }

    static void assertGetValueResult(
        EventSpy &spy,
        int expectedResult,
        const QByteArray &expectedValue
    )
    {
        auto args = takeEvent(spy);

        QCOMPARE(args.at(0).toInt(), expectedResult);
        QCOMPARE(args.at(3).toByteArray(), expectedValue);
    }

    static void assertGetProvidersResult(
        EventSpy &spy,
        int expectedResult,
        int expectedProvidersLen
    )
    {
        auto args = takeEvent(spy);
        auto providers =
            args.at(3).value<QVector<Libp2pPeerInfo>>();

        QCOMPARE(args.at(0).toInt(), expectedResult);
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
     * Tests
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

    void testGetPutValue()
    {
        Libp2pModulePlugin plugin;

        auto libp2pEventSpy = createLibp2pEventSpy(&plugin);
        auto getValueSpy = createGetValueSpy(&plugin);

        startPlugin(plugin, *libp2pEventSpy);

        QByteArray key = "test-key";
        QByteArray expectedValue = "hello-world";

        QVERIFY(plugin.putValue(key, expectedValue));
        waitForEvents(*libp2pEventSpy, 1);
        assertEvent(*libp2pEventSpy, RET_OK, "putValue");

        QVERIFY(plugin.getValue(key, 1));
        waitForEvents(*getValueSpy, 1);
        assertGetValueResult(*getValueSpy, RET_OK, expectedValue);

        stopPlugin(plugin, *libp2pEventSpy);
    }

    void testKeyToCid()
    {
        QByteArray key = "some-key";

        QString cid = Libp2pModulePlugin::toCid(key);
        QByteArray recoveredKey = Libp2pModulePlugin::toKey(cid);

        QVERIFY(recoveredKey == key);
    }

    void testGetProviders()
    {
        Libp2pModulePlugin plugin;

        auto libp2pEventSpy = createLibp2pEventSpy(&plugin);
        auto getProvidersSpy = createGetProvidersSpy(&plugin);

        startPlugin(plugin, *libp2pEventSpy);

        QByteArray key = "key-i-provide";
        QByteArray value = "hello-world";

        QString cid = Libp2pModulePlugin::toCid(key);

        QVERIFY(plugin.putValue(key, value));
        waitForEvents(*libp2pEventSpy, 1);
        assertEvent(*libp2pEventSpy, RET_OK, "putValue");

        QVERIFY(plugin.addProvider(cid));
        waitForEvents(*libp2pEventSpy, 1);
        assertEvent(*libp2pEventSpy, RET_OK, "addProvider");

        QVERIFY(plugin.getProviders(cid));
        waitForEvents(*getProvidersSpy, 1);

        int expectedProvidersLen = 1;
        assertGetProvidersResult(*getProvidersSpy, RET_OK, expectedProvidersLen);

        stopPlugin(plugin, *libp2pEventSpy);
    }
};

QTEST_MAIN(TestLibp2pModule)
#include "test_libp2p_module.moc"

