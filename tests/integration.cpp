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
        QVERIFY(nodeA.syncLibp2pStart());

        PeerInfo nodeAPeerInfo = nodeA.syncPeerInfo();

        Libp2pModulePlugin nodeB({ nodeAPeerInfo });
        QVERIFY(nodeB.syncLibp2pStart());
        PeerInfo nodeBPeerInfo = nodeB.syncPeerInfo();

        QVERIFY(nodeB.syncConnectPeer(nodeAPeerInfo.peerId, nodeAPeerInfo.addrs, 500));

        // warmup routing tables
        nodeA.syncKadFindNode(nodeAPeerInfo.peerId);
        nodeB.syncKadFindNode(nodeAPeerInfo.peerId);

        QByteArray key = "integration-key";
        QByteArray value = "hello";

        // put value until it shows on other peer
        int quorum = 1;
        QByteArray result;
        for (int i = 0; i < 10; ++i) {
            QVERIFY(nodeA.syncKadPutValue(key, value));
            QThread::msleep(200);
            result = nodeB.syncKadGetValue(key, quorum);
            if (!result.isEmpty())
                break;

            QThread::msleep(200);
        }

        QCOMPARE(result, value);

        QVERIFY(nodeB.syncDisconnectPeer(nodeAPeerInfo.peerId));

        QVERIFY(nodeA.syncLibp2pStop());
        QVERIFY(nodeB.syncLibp2pStop());
    }
};

QTEST_MAIN(TestKadIntegration)
#include "integration.moc"
