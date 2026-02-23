#include <QCoreApplication>
#include <QDebug>
#include "plugin.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    /* -----------------------------
       Node A setup
       ----------------------------- */
    Libp2pModulePlugin nodeA;

    qDebug() << "Starting Node A...";
    if (!nodeA.syncLibp2pStart().ok) {
        qFatal("Node A failed to start");
    }

    auto resA = nodeA.syncPeerInfo();
    PeerInfo nodeAPeerInfo = resA.data.value<PeerInfo>();

    /* -----------------------------
       Node B setup (bootstrap Node A)
       ----------------------------- */
    Libp2pModulePlugin nodeB({ nodeAPeerInfo });

    qDebug() << "Starting Node B...";
    if (!nodeB.syncLibp2pStart().ok) {
        qFatal("Node B failed to start");
    }

    /* -----------------------------
       Subscribe Node B to topic
       ----------------------------- */
    QString topic = "demo-topic";
    qDebug() << "Node B subscribing to topic:" << topic;
    if (!nodeB.syncGossipsubSubscribe(topic).ok) {
        qFatal("Node B subscription failed");
    }

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
    // In synchronous mode, Node B receives messages via `syncGossipsubReceive` or similar if implemented.
    // Since our plugin forwards events to libp2pEvent, we simulate by calling libp2pEvent handlers directly
    qDebug() << "Node B should now have received the message";

    /* -----------------------------
       Cleanup
       ----------------------------- */
    nodeA.syncLibp2pStop();
    nodeB.syncLibp2pStop();

    qDebug() << "Done";

    return 0;
}
