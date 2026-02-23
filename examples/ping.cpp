#include <QCoreApplication>
#include <QDebug>
#include "plugin.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    Libp2pModulePlugin nodeA;

    qDebug() << "Starting node A...";
    if (!nodeA.syncLibp2pStart().ok) {
        qFatal("Node A failed to start");
    }

    auto res = nodeA.syncPeerInfo();
    PeerInfo infoA = res.data.value<PeerInfo>();

    Libp2pModulePlugin nodeB({ infoA });

    qDebug() << "Starting node B...";
    if (!nodeB.syncLibp2pStart().ok) {
        qFatal("Node B failed to start");
    }

    qDebug() << "Nodes started";

    // Connect B -> A
    if (!nodeB.syncConnectPeer(infoA.peerId, infoA.addrs, 500).ok) {
        qFatal("Failed to connect peers");
    }

    qDebug() << "Connected";

    // Dial ping protocol
    auto dialRes = nodeB.syncDial(infoA.peerId, "/ipfs/ping/1.0.0");
    if (!dialRes.ok) {
        qFatal("Dial failed");
    }

    uint64_t streamId = dialRes.data.value<uint64_t>();

    QByteArray payload(32, 0);
    for (int i = 0; i < payload.size(); ++i)
        payload[i] = char(i);

    qDebug() << "Sending ping...";
    if (!nodeB.syncStreamWrite(streamId, payload).ok) {
        qFatal("Write failed");
    }

    auto readRes = nodeB.syncStreamReadExactly(streamId, payload.size());
    if (!readRes.ok) {
        qFatal("Read failed");
    }

    QByteArray reply = readRes.data.value<QByteArray>();

    if (reply.size() != payload.size()) {
        qFatal("Ping response size mismatch");
    }

    if (reply != payload) {
        qFatal("Ping payload mismatch");
    }

    qDebug() << "Ping successful — payload verified";

    nodeB.syncStreamClose(streamId);
    nodeB.syncStreamRelease(streamId);

    nodeA.syncLibp2pStop();
    nodeB.syncLibp2pStop();

    qDebug() << "Done";

    return 0;
}
