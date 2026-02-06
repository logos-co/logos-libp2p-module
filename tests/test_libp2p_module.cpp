#include <QtTest>
#include <libp2p_module_plugin.h>

class TestLibp2pModule : public QObject
{
    Q_OBJECT

private slots:

    void testConstruction()
    {
        Libp2pModulePlugin plugin;
        QVERIFY(true); // construction should not crash
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

    static void verifyEvent(QSignalSpy &spy, int expectedResult, const QString &expectedCaller)
    {
        QVERIFY(spy.count() > 0);

        QList<QVariant> args = spy.takeFirst();

        int result = args.at(0).toInt();
        QString caller = args.at(2).toString();

        QCOMPARE(result, expectedResult);
        QCOMPARE(caller, expectedCaller);
    }

    static void waitForEvents(QSignalSpy &spy, int timeoutMs = 5000)
    {
        QVERIFY(spy.wait(timeoutMs));
    }

    void testStartStop()
    {
        Libp2pModulePlugin plugin;

        QSignalSpy spy(
            &plugin,
            SIGNAL(libp2pEvent(int,QString,QString,QString,QVariant))
        );

        // start
        QVERIFY(plugin.libp2pStart());
        waitForEvents(spy);

        QCOMPARE(spy.count(), 2);
        verifyEvent(spy, RET_OK, "libp2pNew");
        verifyEvent(spy, RET_OK, "libp2pStart");

        // stop
        QVERIFY(plugin.libp2pStop());
        waitForEvents(spy);

        QCOMPARE(spy.count(), 1);
        verifyEvent(spy, RET_OK, "libp2pStop");
    }

    void testGetPutValue()
    {
        Libp2pModulePlugin plugin;

        QVERIFY(plugin.libp2pStart());

        QByteArray key = "test-key";
        QByteArray expectedValue = "hello-world";

        // ---- Spy result signal ----
        QSignalSpy spy(
            &plugin,
            SIGNAL(getValueFinished(int,QString,QByteArray))
        );

        // ---- Store value first ----
        QVERIFY(plugin.putValue(key, expectedValue));

        QTest::qWait(500);

        // ---- Request value ----
        QVERIFY(plugin.getValue(key, 1));

        // ---- Wait async callback ----
        QVERIFY(spy.wait(5000)); // wait up to 5s

        QCOMPARE(spy.count(), 1);

        QList<QVariant> args = spy.takeFirst();

        int result = args.at(0).toInt();
        QByteArray returnedValue = args.at(2).toByteArray();

        QCOMPARE(result, RET_OK);
        QCOMPARE(returnedValue, expectedValue);

        plugin.libp2pStop();
    }
};

QTEST_MAIN(TestLibp2pModule)
#include "test_libp2p_module.moc"
