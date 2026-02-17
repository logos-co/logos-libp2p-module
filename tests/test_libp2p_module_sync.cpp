#include <QtTest>
#include <libp2p_module_plugin.h>

class TestLibp2pModuleSync : public QObject
{
    Q_OBJECT

private slots:

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
        QVERIFY(plugin.syncKadGetProviders(cid).size() == 0);

        QVERIFY(plugin.syncLibp2pStop());
    }

    void testSyncKadFindNode()
    {
        Libp2pModulePlugin plugin;

        QVERIFY(plugin.syncLibp2pStart());

        QString fakePeer = "12D3KooWInvalidPeerForSyncTest";

        // no nodes are registered yet
        QVERIFY(plugin.syncKadFindNode(fakePeer).size() == 0);

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
        QVERIFY(plugin.syncKadGetRandomRecords().size() == 0);

        QVERIFY(plugin.syncLibp2pStop());
    }

};

QTEST_MAIN(TestLibp2pModuleSync)
#include "test_libp2p_module_sync.moc"

