#include <logos_test.h>
#include <plugin.h>
#include <QThread>
#include <vector>
#include <memory>

LOGOS_TEST(gossipsub_subscribe_and_publish) {
    Libp2pModulePlugin nodeA;
    Libp2pModulePlugin nodeB;

    LOGOS_ASSERT_TRUE(nodeA.syncLibp2pStart().ok);
    LOGOS_ASSERT_TRUE(nodeB.syncLibp2pStart().ok);

    PeerInfo infoA = nodeA.syncPeerInfo().data.value<PeerInfo>();
    LOGOS_ASSERT_TRUE(nodeB.syncConnectPeer(infoA.peerId, infoA.addrs, 500).ok);

    QString topic = "integration-topic";
    LOGOS_ASSERT_TRUE(nodeB.syncGossipsubSubscribe(topic).ok);
    LOGOS_ASSERT_TRUE(nodeA.syncGossipsubSubscribe(topic).ok);
    QThread::msleep(2000);

    QByteArray payload = "Hello from Node A";
    LOGOS_ASSERT_TRUE(nodeA.syncGossipsubPublish(topic, payload).ok);

    QByteArray received = nodeB.syncGossipsubNextMessage(topic).data.value<QByteArray>();
    LOGOS_ASSERT_TRUE(received == payload);

    LOGOS_ASSERT_TRUE(nodeA.syncLibp2pStop().ok);
    LOGOS_ASSERT_TRUE(nodeB.syncLibp2pStop().ok);
}

LOGOS_TEST(gossipsub_multiple_subscribers) {
    Libp2pModulePlugin nodeA;
    LOGOS_ASSERT_TRUE(nodeA.syncLibp2pStart().ok);

    QString topic = "multi-topic";
    LOGOS_ASSERT_TRUE(nodeA.syncGossipsubSubscribe(topic).ok);

    const int NUM_SUBS = 3;
    std::vector<std::unique_ptr<Libp2pModulePlugin>> subscribers;

    for (int i = 0; i < NUM_SUBS; ++i) {
        subscribers.emplace_back(std::make_unique<Libp2pModulePlugin>());
        LOGOS_ASSERT_TRUE(subscribers.back()->syncLibp2pStart().ok);

        PeerInfo infoA = nodeA.syncPeerInfo().data.value<PeerInfo>();
        LOGOS_ASSERT_TRUE(subscribers.back()->syncConnectPeer(infoA.peerId, infoA.addrs, 500).ok);
        LOGOS_ASSERT_TRUE(subscribers.back()->syncGossipsubSubscribe(topic).ok);
    }

    QThread::msleep(2000);
    QByteArray payload = "Broadcast message";
    LOGOS_ASSERT_TRUE(nodeA.syncGossipsubPublish(topic, payload).ok);

    for (auto &sub : subscribers) {
        QByteArray received = sub->syncGossipsubNextMessage(topic).data.value<QByteArray>();
        LOGOS_ASSERT_TRUE(received == payload);
        LOGOS_ASSERT_TRUE(sub->syncLibp2pStop().ok);
    }

    LOGOS_ASSERT_TRUE(nodeA.syncLibp2pStop().ok);
}

LOGOS_TEST(gossipsub_subscribe_unsubscribe) {
    Libp2pModulePlugin node;
    LOGOS_ASSERT_TRUE(node.syncLibp2pStart().ok);

    QString topic = "temp-topic";
    LOGOS_ASSERT_TRUE(node.syncGossipsubSubscribe(topic).ok);
    LOGOS_ASSERT_TRUE(node.syncGossipsubUnsubscribe(topic).ok);

    QByteArray payload = "Test after unsubscribe";
    LOGOS_ASSERT_TRUE(node.syncGossipsubPublish(topic, payload).ok);

    LOGOS_ASSERT_FALSE(node.syncGossipsubNextMessage(topic).ok);

    LOGOS_ASSERT_TRUE(node.syncLibp2pStop().ok);
}

LOGOS_TEST(gossipsub_binary_payload) {
    Libp2pModulePlugin nodeA;
    Libp2pModulePlugin nodeB;

    LOGOS_ASSERT_TRUE(nodeA.syncLibp2pStart().ok);
    LOGOS_ASSERT_TRUE(nodeB.syncLibp2pStart().ok);

    PeerInfo infoA = nodeA.syncPeerInfo().data.value<PeerInfo>();
    LOGOS_ASSERT_TRUE(nodeB.syncConnectPeer(infoA.peerId, infoA.addrs, 500).ok);

    QString topic = "binary-topic";
    LOGOS_ASSERT_TRUE(nodeB.syncGossipsubSubscribe(topic).ok);
    LOGOS_ASSERT_TRUE(nodeA.syncGossipsubSubscribe(topic).ok);
    QThread::msleep(2000);

    QByteArray payload;
    payload.append('\x01');
    payload.append('\x00');
    payload.append('\x02');
    payload.append('\x00');
    payload.append('\x03');
    LOGOS_ASSERT_EQ(payload.size(), qsizetype(5));

    LOGOS_ASSERT_TRUE(nodeA.syncGossipsubPublish(topic, payload).ok);

    Libp2pResult res = nodeB.syncGossipsubNextMessage(topic);
    LOGOS_ASSERT_TRUE(res.ok);
    QByteArray received = res.data.value<QByteArray>();

    LOGOS_ASSERT_EQ(received.size(), qsizetype(5));
    LOGOS_ASSERT_TRUE(received == payload);

    LOGOS_ASSERT_TRUE(nodeA.syncLibp2pStop().ok);
    LOGOS_ASSERT_TRUE(nodeB.syncLibp2pStop().ok);
}
