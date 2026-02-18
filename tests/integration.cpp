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

        QVERIFY(nodeA.syncLibp2pStart().ok);
        QVERIFY(nodeB.syncLibp2pStart().ok);

        PeerInfo nodeAPeerInfo = nodeA.syncPeerInfo().data.value<PeerInfo>();

        // cannot disconnect, invalid peerId
        QVERIFY(!nodeB.syncDisconnectPeer("fakePeerId").ok);

        QVERIFY(nodeB.syncConnectPeer(nodeAPeerInfo.peerId, nodeAPeerInfo.addrs, 500).ok);
        QVERIFY(nodeB.syncDisconnectPeer(nodeAPeerInfo.peerId).ok);

        QVERIFY(nodeA.syncLibp2pStop().ok);
        QVERIFY(nodeB.syncLibp2pStop().ok);
    }

    void kadPutGet()
    {
        // setup node A
        Libp2pModulePlugin nodeA;
        QVERIFY(nodeA.syncLibp2pStart().ok);
        PeerInfo nodeAPeerInfo = nodeA.syncPeerInfo().data.value<PeerInfo>();

        // setup node B
        Libp2pModulePlugin nodeB({ nodeAPeerInfo });
        QVERIFY(nodeB.syncLibp2pStart().ok);

        QByteArray key = "integration-key";
        QByteArray value = "hello";

        // put value until it shows on other peer
        int quorum = 1;
        QByteArray result;
        for (int i = 0; i < 10; ++i) {
            QVERIFY(nodeA.syncKadPutValue(key, value).ok);
            QThread::msleep(200);
            result = nodeB.syncKadGetValue(key, quorum).data.value<QByteArray>();
            if (!result.isEmpty())
                break;

            QThread::msleep(200);
        }

        QCOMPARE(result, value);

        QVERIFY(nodeB.syncDisconnectPeer(nodeAPeerInfo.peerId).ok);

        QVERIFY(nodeA.syncLibp2pStop().ok);
        QVERIFY(nodeB.syncLibp2pStop().ok);
    }
};

QTEST_MAIN(TestKadIntegration)
#include "integration.moc"
