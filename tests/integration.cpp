#include <QtTest>
#include <vector>
#include <random>
#include <plugin.h>

class TestIntegration : public QObject
{
    Q_OBJECT

private slots:

    void bootstrapNodes()
    {
        // setup node A
        Libp2pModulePlugin nodeA;
        QVERIFY(nodeA.syncLibp2pStart().ok);
        PeerInfo nodeAPeerInfo = nodeA.syncPeerInfo().data.value<PeerInfo>();

        // setup node B with node A as bootstrap
        Libp2pModulePlugin nodeB(Libp2pModuleOptions{ .bootstrapNodes = { nodeAPeerInfo } });
        QVERIFY(nodeB.syncLibp2pStart().ok);

        auto connectedPeers = nodeB.syncConnectedPeers(Direction_Out).data.value<QList<QString>>();
        QVERIFY(!connectedPeers.isEmpty());
        QCOMPARE(connectedPeers[0], nodeAPeerInfo.peerId);

        QVERIFY(nodeA.syncLibp2pStop().ok);
        QVERIFY(nodeB.syncLibp2pStop().ok);
    }

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

        QCOMPARE(nodeB.syncConnectedPeers(Direction_Out).data.value<QList<QString>>().size(), 1);

        QCOMPARE(nodeA.syncConnectedPeers(Direction_In).data.value<QList<QString>>().size(), 1);

        QVERIFY(nodeB.syncDisconnectPeer(nodeAPeerInfo.peerId).ok);

        QVERIFY(nodeA.syncLibp2pStop().ok);
        QVERIFY(nodeB.syncLibp2pStop().ok);
    }

    void reconnectAfterDisconnect()
    {
        Libp2pModulePlugin nodeA;
        Libp2pModulePlugin nodeB;

        QVERIFY(nodeA.syncLibp2pStart().ok);
        QVERIFY(nodeB.syncLibp2pStart().ok);

        PeerInfo infoBPeerInfo = nodeB.syncPeerInfo().data.value<PeerInfo>();

        // connect
        QVERIFY(nodeA.syncConnectPeer(infoBPeerInfo.peerId, infoBPeerInfo.addrs, 500).ok);
        QCOMPARE(nodeA.syncConnectedPeers(Direction_Out).data.value<QList<QString>>().size(), 1);

        // disconnect
        QVERIFY(nodeA.syncDisconnectPeer(infoBPeerInfo.peerId).ok);
        QCOMPARE(nodeA.syncConnectedPeers(Direction_Out).data.value<QList<QString>>().size(), 0);

        // reconnect
        QVERIFY(nodeA.syncConnectPeer(infoBPeerInfo.peerId, infoBPeerInfo.addrs, 500).ok);
        QCOMPARE(nodeA.syncConnectedPeers(Direction_Out).data.value<QList<QString>>().size(), 1);

        // verify the connection works by dialing ping
        const int PING_SIZE = 32;
        Libp2pResult dialResult = nodeA.syncDial(infoBPeerInfo.peerId, "/ipfs/ping/1.0.0");
        QVERIFY(dialResult.ok);

        uint64_t streamId = dialResult.data.value<uint64_t>();

        QByteArray payload(PING_SIZE, 0);
        for (int i = 0; i < PING_SIZE; ++i)
            payload[i] = static_cast<char>(i);
        QVERIFY(nodeA.syncStreamWrite(streamId, payload).ok);

        Libp2pResult readResult = nodeA.syncStreamReadExactly(streamId, PING_SIZE);
        QVERIFY(readResult.ok);
        QCOMPARE(readResult.data.value<QByteArray>(), payload);

        QVERIFY(nodeA.syncStreamCloseWithEOF(streamId).ok);
        QVERIFY(nodeA.syncStreamRelease(streamId).ok);

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

    void subscribeAndPublish()
    {
        Libp2pModulePlugin nodeA;
        Libp2pModulePlugin nodeB;

        QVERIFY(nodeA.syncLibp2pStart().ok);
        QVERIFY(nodeB.syncLibp2pStart().ok);

        // Connect nodeB to nodeA
        PeerInfo infoA = nodeA.syncPeerInfo().data.value<PeerInfo>();
        PeerInfo infoB = nodeB.syncPeerInfo().data.value<PeerInfo>();
        QVERIFY(nodeB.syncConnectPeer(infoA.peerId, infoA.addrs, 500).ok);


        // gossipsub requires both peers to be subscribed to the topic
        // and subscriptions to be broadcasted (takes 2 heartbeats)
        QString topic = "integration-topic";
        QVERIFY(nodeB.syncGossipsubSubscribe(topic).ok);
        QVERIFY(nodeA.syncGossipsubSubscribe(topic).ok);
        QThread::msleep(2000);

        QByteArray payload = "Hello from Node A";
        QVERIFY(nodeA.syncGossipsubPublish(topic, payload).ok);

        QByteArray received = nodeB.syncGossipsubNextMessage(topic).data.value<QByteArray>();

        QCOMPARE(received, payload);

        QVERIFY(nodeA.syncLibp2pStop().ok);
        QVERIFY(nodeB.syncLibp2pStop().ok);
    }

    void multipleSubscribers()
    {
        Libp2pModulePlugin nodeA;
        QVERIFY(nodeA.syncLibp2pStart().ok);

        QString topic = "multi-topic";

        QVERIFY(nodeA.syncGossipsubSubscribe(topic).ok);

        const int NUM_SUBS = 3;
        std::vector<std::unique_ptr<Libp2pModulePlugin>> subscribers;

        // Start subscribers
        for (int i = 0; i < NUM_SUBS; ++i) {
            subscribers.emplace_back(std::make_unique<Libp2pModulePlugin>());
            QVERIFY(subscribers.back()->syncLibp2pStart().ok);

            // Connect each subscriber to nodeA
            PeerInfo infoA = nodeA.syncPeerInfo().data.value<PeerInfo>();
            QVERIFY(subscribers.back()->syncConnectPeer(infoA.peerId, infoA.addrs, 500).ok);

            QVERIFY(subscribers.back()->syncGossipsubSubscribe(topic).ok);
        }

        QThread::msleep(2000);
        QByteArray payload = "Broadcast message";
        QVERIFY(nodeA.syncGossipsubPublish(topic, payload).ok);

        for (auto &sub : subscribers) {
            QByteArray received = sub->syncGossipsubNextMessage(topic).data.value<QByteArray>();
            QCOMPARE(received, payload);
            QVERIFY(sub->syncLibp2pStop().ok);
        }

        QVERIFY(nodeA.syncLibp2pStop().ok);
    }

    void subscribeUnsubscribe()
    {
        Libp2pModulePlugin node;
        QVERIFY(node.syncLibp2pStart().ok);

        QString topic = "temp-topic";
        QVERIFY(node.syncGossipsubSubscribe(topic).ok);
        QVERIFY(node.syncGossipsubUnsubscribe(topic).ok);

        QByteArray payload = "Test after unsubscribe";
        QVERIFY(node.syncGossipsubPublish(topic, payload).ok);

        // Should not receive messages after unsubscribe
        QVERIFY(!node.syncGossipsubNextMessage(topic).ok);

        QVERIFY(node.syncLibp2pStop().ok);
    }

    void gossipsubBinaryPayload()
    {
        Libp2pModulePlugin nodeA;
        Libp2pModulePlugin nodeB;

        QVERIFY(nodeA.syncLibp2pStart().ok);
        QVERIFY(nodeB.syncLibp2pStart().ok);

        PeerInfo infoA = nodeA.syncPeerInfo().data.value<PeerInfo>();
        QVERIFY(nodeB.syncConnectPeer(infoA.peerId, infoA.addrs, 500).ok);

        QString topic = "binary-topic";
        QVERIFY(nodeB.syncGossipsubSubscribe(topic).ok);
        QVERIFY(nodeA.syncGossipsubSubscribe(topic).ok);
        QThread::msleep(2000);

        // payload with embedded null bytes — would be truncated
        // if any part of the callback chain treats data as C string
        QByteArray payload;
        payload.append('\x01');
        payload.append('\x00');
        payload.append('\x02');
        payload.append('\x00');
        payload.append('\x03');
        QCOMPARE(payload.size(), 5);

        QVERIFY(nodeA.syncGossipsubPublish(topic, payload).ok);

        QByteArray received = nodeB.syncGossipsubNextMessage(topic).data.value<QByteArray>();
        QCOMPARE(received.size(), 5);
        QCOMPARE(received, payload);

        QVERIFY(nodeA.syncLibp2pStop().ok);
        QVERIFY(nodeB.syncLibp2pStop().ok);
    }

    void multipleStreamsOnSameConnection()
    {
        const int PING_SIZE = 32;

        Libp2pModulePlugin nodeA;
        Libp2pModulePlugin nodeB;

        QVERIFY(nodeA.syncLibp2pStart().ok);
        QVERIFY(nodeB.syncLibp2pStart().ok);

        PeerInfo infoBPeerInfo = nodeB.syncPeerInfo().data.value<PeerInfo>();
        QVERIFY(nodeA.syncConnectPeer(infoBPeerInfo.peerId, infoBPeerInfo.addrs, 500).ok);

        // open two streams on the same connection
        Libp2pResult dial1 = nodeA.syncDial(infoBPeerInfo.peerId, "/ipfs/ping/1.0.0");
        QVERIFY(dial1.ok);
        uint64_t stream1 = dial1.data.value<uint64_t>();

        Libp2pResult dial2 = nodeA.syncDial(infoBPeerInfo.peerId, "/ipfs/ping/1.0.0");
        QVERIFY(dial2.ok);
        uint64_t stream2 = dial2.data.value<uint64_t>();

        QVERIFY(stream1 != stream2);

        // write different payloads to each stream
        QByteArray payload1(PING_SIZE, 0);
        QByteArray payload2(PING_SIZE, 0);
        for (int i = 0; i < PING_SIZE; ++i) {
            payload1[i] = static_cast<char>(i);
            payload2[i] = static_cast<char>(255 - i);
        }

        QVERIFY(nodeA.syncStreamWrite(stream1, payload1).ok);
        QVERIFY(nodeA.syncStreamWrite(stream2, payload2).ok);

        // read back from each stream and verify no cross-contamination
        Libp2pResult read1 = nodeA.syncStreamReadExactly(stream1, PING_SIZE);
        QVERIFY(read1.ok);
        QCOMPARE(read1.data.value<QByteArray>(), payload1);

        Libp2pResult read2 = nodeA.syncStreamReadExactly(stream2, PING_SIZE);
        QVERIFY(read2.ok);
        QCOMPARE(read2.data.value<QByteArray>(), payload2);

        // cleanup both streams
        QVERIFY(nodeA.syncStreamCloseWithEOF(stream1).ok);
        QVERIFY(nodeA.syncStreamRelease(stream1).ok);
        QVERIFY(nodeA.syncStreamCloseWithEOF(stream2).ok);
        QVERIFY(nodeA.syncStreamRelease(stream2).ok);

        QVERIFY(nodeA.syncLibp2pStop().ok);
        QVERIFY(nodeB.syncLibp2pStop().ok);
    }

    void directDialStreamExchange()
    {
        const int PING_SIZE = 32;

        Libp2pModulePlugin nodeA;
        Libp2pModulePlugin nodeB;

        QVERIFY(nodeA.syncLibp2pStart().ok);
        QVERIFY(nodeB.syncLibp2pStart().ok);

        PeerInfo infoBPeerInfo = nodeB.syncPeerInfo().data.value<PeerInfo>();

        // connect A to B
        QVERIFY(nodeA.syncConnectPeer(infoBPeerInfo.peerId, infoBPeerInfo.addrs, 500).ok);

        // A dials B directly on /ipfs/ping/1.0.0
        Libp2pResult dialResult = nodeA.syncDial(infoBPeerInfo.peerId, "/ipfs/ping/1.0.0");
        QVERIFY(dialResult.ok);

        uint64_t streamId = dialResult.data.value<uint64_t>();
        QVERIFY(streamId != 0);

        // send ping payload
        QByteArray payload(PING_SIZE, 0);
        for (int i = 0; i < PING_SIZE; ++i)
            payload[i] = static_cast<char>(i);
        QVERIFY(nodeA.syncStreamWrite(streamId, payload).ok);

        // read ping echo
        Libp2pResult readResult = nodeA.syncStreamReadExactly(streamId, PING_SIZE);
        QVERIFY(readResult.ok);
        QCOMPARE(readResult.data.value<QByteArray>(), payload);

        // cleanup
        QVERIFY(nodeA.syncStreamCloseWithEOF(streamId).ok);
        QVERIFY(nodeA.syncStreamRelease(streamId).ok);

        QVERIFY(nodeA.syncLibp2pStop().ok);
        QVERIFY(nodeB.syncLibp2pStop().ok);
    }
};

QTEST_MAIN(TestIntegration)
#include "integration.moc"
