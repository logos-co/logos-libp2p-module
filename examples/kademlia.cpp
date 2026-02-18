#include <QCoreApplication>
#include <QDebug>
#include "libp2p_module_plugin.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    Libp2pModulePlugin nodeA;

    qDebug() << "Starting nodes...";

    if (!nodeA.syncLibp2pStart().ok) {
        qFatal("Node A failed to start");
    }
    auto res = nodeA.syncPeerInfo();
    PeerInfo nodeAPeerInfo = res.data.value<PeerInfo>();

    Libp2pModulePlugin nodeB({ nodeAPeerInfo });

    if (!nodeB.syncLibp2pStart().ok) {
        qFatal("Node B failed to start");
    }

    qDebug() << "Nodes started";

    // Obtain node B peer info from node B
    res = nodeB.syncKadGetRandomRecords();
    QList<ExtendedPeerRecord> peersB = res.data.value<QList<ExtendedPeerRecord>>();

    if (peersB.isEmpty()) {
        qDebug() << "Node B has no known peers yet";
    }

    /* ----------------------------------
       Kademlia Put from A
       ---------------------------------- */

    QByteArray key   = "demo-key";
    QByteArray value = "Hello from node A";

    qDebug() << "Node A putting value into DHT";

    if (!nodeA.syncKadPutValue(key, value).ok) {
        qFatal("PutValue failed");
    }

    /* ----------------------------------
       Node B retrieves value
       ---------------------------------- */

    qDebug() << "Node B fetching value from DHT";

    res = nodeB.syncKadGetValue(key, 1);
    QByteArray received = res.data.value<QByteArray>();

    if (received.isEmpty()) {
        qWarning() << "Node B did not find value";
    } else {
        qDebug() << "Node B received:" << received;
    }

    /* ----------------------------------
       Providers demo
       ---------------------------------- */

    res = nodeA.syncToCid(key);
    QString cid = res.data.value<QString>();

    qDebug() << "CID:" << cid;

    nodeA.syncKadStartProviding(cid);

    res = nodeB.syncKadGetProviders(cid);
    QList<PeerInfo> providers = res.data.value<QList<PeerInfo>>();
    qDebug() << "Providers found by B:" << providers.size();
    for (const auto &p : providers) {
        qDebug() << "Provider:" << p.peerId;
    }

    /* ---------------------------------- */

    nodeA.syncLibp2pStop();
    nodeB.syncLibp2pStop();

    qDebug() << "Done";

    return 0;
}

