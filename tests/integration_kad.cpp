#include <QtTest>
#include <plugin.h>

class TestKad : public QObject
{
    Q_OBJECT

private slots:

    void kadPutGet()
    {
        // setup node A
        Libp2pModulePlugin nodeA;
        QVERIFY(nodeA.syncLibp2pStart().ok);
        PeerInfo nodeAPeerInfo = nodeA.syncPeerInfo().data.value<PeerInfo>();

        // setup node B
        Libp2pModulePlugin nodeB(Libp2pModuleOptions{ .bootstrapNodes = { nodeAPeerInfo } });
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

    void kadFindNode()
    {
        // node A is the bootstrap node
        Libp2pModulePlugin nodeA;
        QVERIFY(nodeA.syncLibp2pStart().ok);
        PeerInfo infoA = nodeA.syncPeerInfo().data.value<PeerInfo>();

        // nodes B, C, D all bootstrap from A to form a DHT
        Libp2pModulePlugin nodeB(Libp2pModuleOptions{ .bootstrapNodes = { infoA } });
        QVERIFY(nodeB.syncLibp2pStart().ok);
        PeerInfo infoB = nodeB.syncPeerInfo().data.value<PeerInfo>();

        Libp2pModulePlugin nodeC(Libp2pModuleOptions{ .bootstrapNodes = { infoA } });
        QVERIFY(nodeC.syncLibp2pStart().ok);
        PeerInfo infoC = nodeC.syncPeerInfo().data.value<PeerInfo>();

        Libp2pModulePlugin nodeD(Libp2pModuleOptions{ .bootstrapNodes = { infoA } });
        QVERIFY(nodeD.syncLibp2pStart().ok);

        // D looks up B's peer ID — must discover it through the DHT
        Libp2pResult result = nodeD.syncKadFindNode(infoB.peerId);
        QVERIFY(result.ok);

        QList<QString> peers = result.data.value<QList<QString>>();
        QCOMPARE(peers.size(), 3);
        QVERIFY(peers.contains(infoA.peerId));
        QVERIFY(peers.contains(infoB.peerId));
        QVERIFY(peers.contains(infoC.peerId));

        QVERIFY(nodeA.syncLibp2pStop().ok);
        QVERIFY(nodeB.syncLibp2pStop().ok);
        QVERIFY(nodeC.syncLibp2pStop().ok);
        QVERIFY(nodeD.syncLibp2pStop().ok);
    }

    void kadStartStopProviding()
    {
        // node A is the bootstrap node
        Libp2pModulePlugin nodeA;
        QVERIFY(nodeA.syncLibp2pStart().ok);
        PeerInfo infoA = nodeA.syncPeerInfo().data.value<PeerInfo>();

        // node B bootstraps from A so they share DHT routing
        Libp2pModulePlugin nodeB(Libp2pModuleOptions{ .bootstrapNodes = { infoA } });
        QVERIFY(nodeB.syncLibp2pStart().ok);
        PeerInfo infoB = nodeB.syncPeerInfo().data.value<PeerInfo>();

        // A creates a CID and announces itself as provider
        QByteArray key = "provider-test-key";
        Libp2pResult cidResult = nodeA.syncToCid(key);
        QVERIFY(cidResult.ok);
        QString cid = cidResult.data.value<QString>();
        QVERIFY(!cid.isEmpty());

        // A starts providing
        QVERIFY(nodeA.syncKadStartProviding(cid).ok);

        Libp2pResult res = nodeB.syncKadGetProviders(cid);
        QVERIFY(res.ok);
        QList<PeerInfo> providers = res.data.value<QList<PeerInfo>>();

        // B must discover A as a provider for the CID
        QVERIFY(!providers.isEmpty());
        QCOMPARE(providers[0].peerId, infoA.peerId);

        // A stops providing — removes from local providedKeys
        QVERIFY(nodeA.syncKadStopProviding(cid).ok);

        // B queries again — A should no longer appear as provider
        res = nodeB.syncKadGetProviders(cid);
        QVERIFY(res.ok);
        providers = res.data.value<QList<PeerInfo>>();

        QVERIFY(providers.isEmpty());

        QVERIFY(nodeA.syncLibp2pStop().ok);
        QVERIFY(nodeB.syncLibp2pStop().ok);
    }

    void kadRandomRecords()
    {
        // setup node A
        Libp2pModulePlugin nodeA;
        QVERIFY(nodeA.syncLibp2pStart().ok);
        PeerInfo nodeAPeerInfo = nodeA.syncPeerInfo().data.value<PeerInfo>();

        // setup node B
        Libp2pModulePlugin nodeB(Libp2pModuleOptions{ .bootstrapNodes = { nodeAPeerInfo } });
        QVERIFY(nodeB.syncLibp2pStart().ok);

        Libp2pResult res = nodeA.syncKadGetRandomRecords();
        QVERIFY(res.ok);
        QList<ExtendedPeerRecord> records = res.data.value<QList<ExtendedPeerRecord>>();

        QVERIFY(!records.isEmpty());
        QVERIFY(!records[0].peerId.isEmpty());

        QVERIFY(nodeA.syncLibp2pStop().ok);
        QVERIFY(nodeB.syncLibp2pStop().ok);
    }
};

QTEST_MAIN(TestKad)
#include "integration_kad.moc"
