#include <QCoreApplication>
#include <QDebug>
#include <QThread>
#include "plugin.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    /* -----------------------------
       Node A setup
       ----------------------------- */
    Libp2pModulePlugin nodeA;
    Libp2pModulePlugin nodeB;

    qDebug() << "Starting Nodes...";
    if (!nodeA.syncLibp2pStart().ok) {
        qFatal("Node A failed to start");
    }
    if (!nodeB.syncLibp2pStart().ok) {
        qFatal("Node B failed to start");
    }

    PeerInfo nodeAPeerInfo = nodeA.syncPeerInfo().data.value<PeerInfo>();

    if (!nodeB.syncConnectPeer(nodeAPeerInfo.peerId, nodeAPeerInfo.addrs, 500).ok) {
        qFatal("Node B failed to connect to Node A");
    }

    /* -----------------------------
       Subscribe nodes to topic
       ----------------------------- */
    QString topic = "demo-topic";
    qDebug() << "Node B subscribing to topic:" << topic;
    if (!nodeB.syncGossipsubSubscribe(topic).ok) {
        qFatal("Node B subscription failed");
    }
    qDebug() << "Node A subscribing to topic:" << topic;
    if (!nodeA.syncGossipsubSubscribe(topic).ok) {
        qFatal("Node A subscription failed");
    }

    /* -----------------------------
       Give the mesh time to form
       ----------------------------- */
    qDebug() << "Waiting for mesh to form";
    QThread::msleep(2000);

    /* -----------------------------
       Publish message from Node A
       ----------------------------- */
    QByteArray payload = "Hello from Node A via gossipsub!";
    qDebug() << "Node A publishing message to topic:" << topic;
    if (!nodeA.syncGossipsubPublish(topic, payload).ok) {
        qFatal("Node A publish failed");
    }

    /* -----------------------------
       Fetch messages for Node B
       ----------------------------- */
    auto res = nodeB.syncGossipsubNextMessage(topic);
    if (!res.ok) {
        qFatal("Node B did not receive any messages");
    }
    QByteArray message = res.data.value<QByteArray>();
    qDebug() << "Node B received:" << message;

    /* -----------------------------
       Cleanup
       ----------------------------- */
    nodeA.syncLibp2pStop();
    nodeB.syncLibp2pStop();

    qDebug() << "Done";

    return 0;
}
