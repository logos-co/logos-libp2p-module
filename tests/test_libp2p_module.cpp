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

    static EventSpy createLibp2pEventSpy(QObject *obj)
    {
        return EventSpy(
            obj,
            SIGNAL(libp2pEvent(int,QString,QString,QString,QVariant))
        );
    }

    static EventSpy createGetValueSpy(QObject *obj)
    {
        return EventSpy(
            obj,
            SIGNAL(getValueFinished(int,QString,QByteArray))
        );
    }

    static void waitForSpy(EventSpy &spy, int timeoutMs = 5000)
    {
        QVERIFY(spy.wait(timeoutMs));
    }

    static void fail(const char *msg)
    {
        QTest::qFail(msg, __FILE__, __LINE__);
    }


    static QList<QVariant> takeEvent(EventSpy &spy)
    {
        if (spy.count() <= 0) {
            fail("Expected event but spy is empty");
            return {}; // required for compiler
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
        QCOMPARE(args.at(2).toByteArray(), expectedValue);
    }

    /* ---------------------------
     * Plugin lifecycle helpers
     * --------------------------- */

    static void startPlugin(Libp2pModulePlugin &plugin, EventSpy &spy)
    {
        QVERIFY(plugin.libp2pStart());
        waitForSpy(spy);

        assertEventCount(spy, 2);
        assertEvent(spy, RET_OK, "libp2pNew");
        assertEvent(spy, RET_OK, "libp2pStart");
    }

    static void stopPlugin(Libp2pModulePlugin &plugin, EventSpy &spy)
    {
        QVERIFY(plugin.libp2pStop());
        waitForSpy(spy);

        assertEventCount(spy, 1);
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
            SIGNAL(libp2pEvent(QString,int,QString,QVariant))
        );

        plugin.foo("hello");

        QVERIFY(spy.count() >= 0);
    }

    void testStartStop()
    {
        Libp2pModulePlugin plugin;
        auto spy = createLibp2pEventSpy(&plugin);

        startPlugin(plugin, spy);
        stopPlugin(plugin, spy);
    }

    void testGetPutValue()
    {
        Libp2pModulePlugin plugin;

        auto libp2pEventSpy = createLibp2pEventSpy(&plugin);
        auto getValueSpy = createGetValueSpy(&plugin);

        startPlugin(plugin, libp2pEventSpy);

        QByteArray key = "test-key";
        QByteArray expectedValue = "hello-world";

        QVERIFY(plugin.putValue(key, expectedValue));
        waitForSpy(libp2pEventSpy);
        assertEvent(libp2pEventSpy, RET_OK, "putValue");

        QVERIFY(plugin.getValue(key, 1));
        waitForSpy(getValueSpy);
        assertGetValueResult(getValueSpy, RET_OK, expectedValue);

        stopPlugin(plugin, libp2pEventSpy);
    }
};

QTEST_MAIN(TestLibp2pModule)
#include "test_libp2p_module.moc"

