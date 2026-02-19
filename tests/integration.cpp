#include <QtTest>
#include <plugin.h>

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

    void mixBetweenTwoNodes()
    {
        Libp2pModulePlugin nodeA;
        Libp2pModulePlugin nodeB;

        QVERIFY(nodeA.syncLibp2pStart().ok);
        QVERIFY(nodeB.syncLibp2pStart().ok);

        auto privA = nodeA.syncMixGeneratePrivKey();
        auto privB = nodeB.syncMixGeneratePrivKey();

        QVERIFY(privA.ok);
        QVERIFY(privB.ok);

        QByteArray privKeyA = privA.data.value<QByteArray>();
        QByteArray privKeyB = privB.data.value<QByteArray>();

        auto pubA = nodeA.syncMixPublicFromPrivate(privKeyA);
        auto pubB = nodeB.syncMixPublicFromPrivate(privKeyB);

        QVERIFY(pubA.ok);
        QVERIFY(pubB.ok);

        QByteArray pubKeyA = pubA.data.value<QByteArray>();
        QByteArray pubKeyB = pubB.data.value<QByteArray>();

        QVERIFY(!pubKeyA.isEmpty());
        QVERIFY(!pubKeyB.isEmpty());

        // optional: derive shared secrets if you exposed helper
        auto secretA = nodeA.syncMixSharedSecret(privKeyA, pubKeyB);
        auto secretB = nodeB.syncMixSharedSecret(privKeyB, pubKeyA);

        QVERIFY(secretA.ok);
        QVERIFY(secretB.ok);

        QByteArray sA = secretA.data.value<QByteArray>();
        QByteArray sB = secretB.data.value<QByteArray>();

        QCOMPARE(sA, sB);

        QVERIFY(nodeA.syncLibp2pStop().ok);
        QVERIFY(nodeB.syncLibp2pStop().ok);
    }
};

QTEST_MAIN(TestKadIntegration)
#include "integration.moc"
