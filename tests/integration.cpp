#include <QtTest>
#include <libp2p_module_plugin.h>

class TestKadIntegration : public QObject
{
    Q_OBJECT

private slots:

    void twoNodesTalk()
    {
        Libp2pModulePlugin nodeA;
        Libp2pModulePlugin nodeB;

        QVERIFY(nodeA.syncLibp2pStart());
        QVERIFY(nodeB.syncLibp2pStart());

        PeerInfo nodeAPeerInfo = nodeA.syncPeerInfo();
        nodeB.syncConnectPeer(nodeAPeerInfo.peerId, nodeAPeerInfo.addrs, 500)

        QByteArray key = "integration-key";
        QByteArray value = "hello";

        QVERIFY(nodeA.syncKadPutValue(key, value));

        auto result = nodeB.syncKadGetValue(key);

        QCOMPARE(result, value);

        nodeB.syncDisconnectPeer(nodeAPeerInfo.peerId);

        QVERIFY(nodeA.syncLibp2pStop());
        QVERIFY(nodeB.syncLibp2pStop());
    }
};

QTEST_MAIN(TestKadIntegration)
#include "integration.moc"
