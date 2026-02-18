#include <QCoreApplication>
#include <QDebug>
#include "libp2p_module_plugin.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    Libp2pModulePlugin nodeA;

    qDebug() << "Starting nodes...";

    if (!nodeA.syncLibp2pStart()) {
        qFatal("Node A failed to start");
    }
    PeerInfo nodeAPeerInfo = nodeA.syncPeerInfo();

    Libp2pModulePlugin nodeB({ nodeAPeerInfo });

    if (!nodeB.syncLibp2pStart()) {
        qFatal("Node B failed to start");
    }

    qDebug() << "Nodes started";

    // Obtain node B peer info from node B
    auto peersB = nodeB.syncKadGetRandomRecords();

    if (peersB.isEmpty()) {
        qDebug() << "Node B has no known peers yet";
    }

    /* ----------------------------------
       Kademlia Put from A
       ---------------------------------- */

    QByteArray key   = "demo-key";
    QByteArray value = "Hello from node A";

    qDebug() << "Node A putting value into DHT";

    if (!nodeA.syncKadPutValue(key, value)) {
        qFatal("PutValue failed");
    }

    /* ----------------------------------
       Node B retrieves value
       ---------------------------------- */

    qDebug() << "Node B fetching value from DHT";

    QByteArray received = nodeB.syncKadGetValue(key, 1);

    if (received.isEmpty()) {
        qWarning() << "Node B did not find value";
    } else {
        qDebug() << "Node B received:" << received;
    }

    /* ----------------------------------
       Providers demo
       ---------------------------------- */

    QString cid = nodeA.syncToCid(key);

    qDebug() << "CID:" << cid;

    nodeA.syncKadStartProviding(cid);

    auto providers = nodeB.syncKadGetProviders(cid);

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

