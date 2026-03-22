#include <QtTest>
#include <vector>
#include <plugin.h>

class TestGossipsub : public QObject
{
    Q_OBJECT

private slots:

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

        Libp2pResult res = nodeB.syncGossipsubNextMessage(topic);
        QVERIFY(res.ok);
        QByteArray received = res.data.value<QByteArray>();

        QCOMPARE(received.size(), 5);
        QCOMPARE(received, payload);

        QVERIFY(nodeA.syncLibp2pStop().ok);
        QVERIFY(nodeB.syncLibp2pStop().ok);
    }
};

QTEST_MAIN(TestGossipsub)
#include "integration_gossipsub.moc"
