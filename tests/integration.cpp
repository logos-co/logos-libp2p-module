#include <QtTest>
#include <vector>
#include <random>
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

    void mixDialAndReply()
    {
        const int NUM_NODES = 5;

        std::vector<std::unique_ptr<Libp2pModulePlugin>> nodes;
        QVector<PeerInfo> infos(NUM_NODES);
        QVector<QByteArray> mixPrivKeys(NUM_NODES);
        QVector<QByteArray> mixPubKeys(NUM_NODES);
        QVector<QByteArray> libp2pPubKeys(NUM_NODES);

        // --- Start nodes ---
        for (int i = 0; i < NUM_NODES; ++i) {
            nodes.push_back(std::make_unique<Libp2pModulePlugin>());
            QVERIFY(nodes[i]->syncLibp2pStart().ok);

            infos[i] = nodes[i]->syncPeerInfo().data.value<PeerInfo>();
            QVERIFY(!infos[i].peerId.isEmpty());
            QVERIFY(!infos[i].addrs.isEmpty());

            mixPrivKeys[i] = nodes[i]->mixGeneratePrivKey();
            mixPubKeys[i] = nodes[i]->mixPublicKey(mixPrivKeys[i]);

            libp2pPubKeys[i] =
                nodes[i]->syncLibp2pPublicKey().data.value<QByteArray>();

            QVERIFY(nodes[i]->syncMixSetNodeInfo(
                        infos[i].addrs[0],
                        mixPrivKeys[i]).ok);

            QVERIFY(nodes[i]->syncMixRegisterDestReadBehavior(
                        "/ipfs/ping/1.0.0",
                        LIBP2P_MIX_READ_EXACTLY,
                        32).ok);
        }

        // --- Populate mix pools ---
        for (int i = 0; i < NUM_NODES; ++i) {
            for (int j = 0; j < NUM_NODES; ++j) {
                if (i == j)
                    continue;

                QVERIFY(nodes[i]->syncMixNodepoolAdd(
                            infos[j].peerId,
                            infos[j].addrs[0],
                            mixPubKeys[j],
                            libp2pPubKeys[j]).ok);
            }
        }

        // --- Dial through mix ---
        Libp2pResult dialResult = nodes[0]->syncMixDialWithReply(
            infos[4].peerId,
            infos[4].addrs[0],
            "/ipfs/ping/1.0.0",
            1,
            0
        );

        QVERIFY(dialResult.ok);
        uint64_t streamId = dialResult.data.value<uint64_t>();

        // generate a random payload
        QByteArray payload(32, 0);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dist(0, 255);
        for (int i = 0; i < payload.size(); ++i)
            payload[i] = static_cast<char>(dist(gen));

        QVERIFY(nodes[0]->syncStreamWrite(streamId, payload).ok);

        Libp2pResult readRes =
            nodes[0]->syncStreamReadExactly(streamId, payload.size());

        QVERIFY(readRes.ok);
        QByteArray received = readRes.data.value<QByteArray>();

        QCOMPARE(received, payload);

        QVERIFY(nodes[0]->syncStreamClose(streamId).ok);
        QVERIFY(nodes[0]->syncStreamRelease(streamId).ok);

        // --- Shutdown ---
        for (int i = 0; i < NUM_NODES; ++i) {
            QVERIFY(nodes[i]->syncLibp2pStop().ok);
        }
    }

    void mixNodepoolRouting()
    {
        // Smaller test focused on pool correctness
        Libp2pModulePlugin nodeA;
        Libp2pModulePlugin nodeB;
        Libp2pModulePlugin nodeC;

        QVERIFY(nodeA.syncLibp2pStart().ok);
        QVERIFY(nodeB.syncLibp2pStart().ok);
        QVERIFY(nodeC.syncLibp2pStart().ok);

        PeerInfo infoA = nodeA.syncPeerInfo().data.value<PeerInfo>();
        PeerInfo infoB = nodeB.syncPeerInfo().data.value<PeerInfo>();
        PeerInfo infoC = nodeC.syncPeerInfo().data.value<PeerInfo>();

        QByteArray privA = nodeA.mixGeneratePrivKey();
        QByteArray privB = nodeB.mixGeneratePrivKey();
        QByteArray privC = nodeC.mixGeneratePrivKey();

        QByteArray pubA = nodeA.mixPublicKey(privA);
        QByteArray pubB = nodeB.mixPublicKey(privB);
        QByteArray pubC = nodeC.mixPublicKey(privC);

        QByteArray libp2pPubA =
            nodeA.syncLibp2pPublicKey().data.value<QByteArray>();
        QByteArray libp2pPubB =
            nodeB.syncLibp2pPublicKey().data.value<QByteArray>();
        QByteArray libp2pPubC =
            nodeC.syncLibp2pPublicKey().data.value<QByteArray>();

        QVERIFY(nodeA.syncMixSetNodeInfo(infoA.addrs[0], privA).ok);
        QVERIFY(nodeB.syncMixSetNodeInfo(infoB.addrs[0], privB).ok);
        QVERIFY(nodeC.syncMixSetNodeInfo(infoC.addrs[0], privC).ok);

        QVERIFY(nodeA.syncMixNodepoolAdd(infoB.peerId, infoB.addrs[0], pubB, libp2pPubB).ok);
        QVERIFY(nodeA.syncMixNodepoolAdd(infoC.peerId, infoC.addrs[0], pubC, libp2pPubC).ok);

        QVERIFY(nodeB.syncMixNodepoolAdd(infoA.peerId, infoA.addrs[0], pubA, libp2pPubA).ok);
        QVERIFY(nodeC.syncMixNodepoolAdd(infoA.peerId, infoA.addrs[0], pubA, libp2pPubA).ok);

        QVERIFY(nodeA.syncLibp2pStop().ok);
        QVERIFY(nodeB.syncLibp2pStop().ok);
        QVERIFY(nodeC.syncLibp2pStop().ok);
    }
};

QTEST_MAIN(TestKadIntegration)
#include "integration.moc"
