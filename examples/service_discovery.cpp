#include <QCoreApplication>
#include <QDebug>
#include <QThread>
#include "plugin.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    /* -----------------------------
       Node A: advertiser
       ----------------------------- */
    Libp2pModulePlugin nodeA(Libp2pModuleOptions{
        .mountServiceDiscovery = true,
    });

    /* -----------------------------
       Node B: discoverer
       ----------------------------- */
    Libp2pModulePlugin nodeB(Libp2pModuleOptions{
        .mountServiceDiscovery = true,
    });

    qDebug() << "Starting nodes...";

    if (!nodeA.syncLibp2pStart().ok)
        qFatal("Node A failed to start");

    if (!nodeB.syncLibp2pStart().ok)
        qFatal("Node B failed to start");

    /* -----------------------------
       Start service discovery
       ----------------------------- */
    if (!nodeA.syncDiscoStart().ok)
        qFatal("Node A: discoStart failed");

    if (!nodeB.syncDiscoStart().ok)
        qFatal("Node B: discoStart failed");

    /* -----------------------------
       Connect B -> A so they share routing
       ----------------------------- */
    PeerInfo infoA = nodeA.syncPeerInfo().data.value<PeerInfo>();
    if (!nodeB.syncConnectPeer(infoA.peerId, infoA.addrs, 500).ok)
        qFatal("Node B failed to connect to Node A");

    /* -----------------------------
       Node A advertises a service
       ----------------------------- */
    QString serviceId = "demo-service";
    QByteArray serviceData = "version=1";

    qDebug() << "Node A: starting advertising" << serviceId;
    if (!nodeA.syncDiscoStartAdvertising(serviceId, serviceData).ok)
        qFatal("Node A: discoStartAdvertising failed");

    /* Node B registers interest to set up routing tables */
    qDebug() << "Node B: starting discovering" << serviceId;
    if (!nodeB.syncDiscoStartDiscovering(serviceId).ok)
        qFatal("Node B: discoStartDiscovering failed");

    /* Give the advertisement time to propagate */
    QThread::msleep(1000);

    /* -----------------------------
       Node B looks up the service
       ----------------------------- */
    qDebug() << "Node B: looking up" << serviceId;
    Libp2pResult res = nodeB.syncDiscoLookup(serviceId, serviceData);

    if (!res.ok) {
        qWarning() << "Lookup returned no results";
    } else {
        QList<ExtendedPeerRecord> records =
            res.data.value<QList<ExtendedPeerRecord>>();
        qDebug() << "Node B found" << records.size() << "peer(s) advertising" << serviceId;
        for (const auto &rec : records) {
            qDebug() << "  peer:" << rec.peerId
                     << "seq:" << rec.seqNo
                     << "addrs:" << rec.addrs.size();
        }
    }

    /* -----------------------------
       Random lookup
       ----------------------------- */
    qDebug() << "Node A: random lookup";
    res = nodeA.syncDiscoRandomLookup();
    if (res.ok) {
        QList<ExtendedPeerRecord> records =
            res.data.value<QList<ExtendedPeerRecord>>();
        qDebug() << "Random lookup returned" << records.size() << "peer(s)";
    }

    /* -----------------------------
       Stop advertising
       ----------------------------- */
    qDebug() << "Node B: stopping discovering" << serviceId;
    nodeB.syncDiscoStopDiscovering(serviceId);

    qDebug() << "Node A: stopping advertising" << serviceId;
    nodeA.syncDiscoStopAdvertising(serviceId);

    /* -----------------------------
       Stop service discovery
       ----------------------------- */
    nodeA.syncDiscoStop();
    nodeB.syncDiscoStop();

    /* -----------------------------
       Cleanup
       ----------------------------- */
    nodeA.syncLibp2pStop();
    nodeB.syncLibp2pStop();

    qDebug() << "Done";

    return 0;
}
