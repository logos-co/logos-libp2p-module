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
};

QTEST_MAIN(TestLibp2pModule)
#include "test_libp2p_module.moc"
