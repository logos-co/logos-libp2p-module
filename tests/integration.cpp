#include <QtTest>
#include <libp2p_module_plugin.h>

class TestKadIntegration : public QObject
{
    Q_OBJECT

private slots:

    void connectAndDisconnect()
    {
        Libp2pModulePlugin nodeA;
        Libp2pModulePlugin nodeB;

        QVERIFY(nodeA.syncLibp2pStart());
        QVERIFY(nodeB.syncLibp2pStart());

        PeerInfo nodeAPeerInfo = nodeA.syncPeerInfo();

        // cannot disconnect, invalid peerId
        QVERIFY(!nodeB.syncDisconnectPeer("fakePeerId"));

        QVERIFY(nodeB.syncConnectPeer(nodeAPeerInfo.peerId, nodeAPeerInfo.addrs, 500));
        QVERIFY(nodeB.syncDisconnectPeer(nodeAPeerInfo.peerId));

        QVERIFY(nodeA.syncLibp2pStop());
        QVERIFY(nodeB.syncLibp2pStop());
    }

    void kadPutGet()
    {
        Libp2pModulePlugin nodeA;
        Libp2pModulePlugin nodeB;

        QVERIFY(nodeA.syncLibp2pStart());
        QVERIFY(nodeB.syncLibp2pStart());

        PeerInfo nodeAPeerInfo = nodeA.syncPeerInfo();
        QVERIFY(nodeB.syncConnectPeer(nodeAPeerInfo.peerId, nodeAPeerInfo.addrs, 500));

        QByteArray key = "integration-key";
        QByteArray value = "hello";

        QVERIFY(nodeA.syncKadPutValue(key, value));

        // give time for key to be there
        QThread::msleep(1000);

        int quorum = 1;
        auto result = nodeB.syncKadGetValue(key, quorum);

        QCOMPARE(result, value);

        QVERIFY(nodeB.syncDisconnectPeer(nodeAPeerInfo.peerId));

        QVERIFY(nodeA.syncLibp2pStop());
        QVERIFY(nodeB.syncLibp2pStop());
    }
};

QTEST_MAIN(TestKadIntegration)
#include "integration.moc"
