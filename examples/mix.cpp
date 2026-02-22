#include <QCoreApplication>
#include <QDebug>
#include <random>
#include "plugin.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    const int NUM_NODES = 5;

    QList<Libp2pModulePlugin*> nodes;
    QList<PeerInfo> infos;

    QList<QByteArray> mixPrivKeys;
    QList<QByteArray> mixPubKeys;
    QList<QByteArray> libp2pPubKeys;

    qDebug() << "Starting mix nodes...";

    // ----------------------------
    // Start nodes
    // ----------------------------
    for (int i = 0; i < NUM_NODES; ++i) {
        auto *node = new Libp2pModulePlugin();
        nodes.append(node);

        if (!node->syncLibp2pStart().ok)
            qFatal("Node failed to start");

        PeerInfo info = node->syncPeerInfo().data.value<PeerInfo>();
        infos.append(info);

        qDebug() << "Node started:" << info.peerId;
        qDebug() << "Address:" << info.addrs[0];

        QByteArray priv = node->mixGeneratePrivKey();
        QByteArray pub  = node->mixPublicKey(priv);
        QByteArray libp2pPub =
            node->syncLibp2pPublicKey().data.value<QByteArray>();

        mixPrivKeys.append(priv);
        mixPubKeys.append(pub);
        libp2pPubKeys.append(libp2pPub);

        if (!node->syncMixSetNodeInfo(info.addrs[0], priv).ok)
            qFatal("mixSetNodeInfo failed");

        if (!node->syncMixRegisterDestReadBehavior(
                "/ipfs/ping/1.0.0",
                LIBP2P_MIX_READ_EXACTLY,
                32).ok)
        {
            qFatal("mixRegisterDestReadBehavior failed");
        }
    }

    qDebug() << "All nodes started";

    // ----------------------------
    // Populate mix pools
    // ----------------------------
    qDebug() << "Populating mix pools";

    for (int i = 0; i < NUM_NODES; ++i) {
        for (int j = 0; j < NUM_NODES; ++j) {
            if (i == j)
                continue;

            if (!nodes[i]->syncMixNodepoolAdd(
                    infos[j].peerId,
                    infos[j].addrs[0],
                    mixPubKeys[j],
                    libp2pPubKeys[j]).ok)
            {
                qFatal("mixNodepoolAdd failed");
            }
        }
    }

    qDebug() << "Mix pools ready";

    // ----------------------------
    // Dial through mix network
    // ----------------------------
    qDebug() << "Dialing through mix";

    auto dialRes = nodes[0]->syncMixDialWithReply(
        infos[4].peerId,
        infos[4].addrs[0],
        "/ipfs/ping/1.0.0",
        1,
        0
    );

    if (!dialRes.ok)
        qFatal("Mix dial failed");

    uint64_t streamId = dialRes.data.value<uint64_t>();

    qDebug() << "Mix stream opened:" << streamId;

    // ----------------------------
    // Send payload
    // ----------------------------
    QByteArray payload(32, 0);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, 255);

    for (int i = 0; i < payload.size(); ++i)
        payload[i] = static_cast<char>(dist(gen));

    qDebug() << "Sending payload";

    if (!nodes[0]->syncStreamWrite(streamId, payload).ok)
        qFatal("Stream write failed");

    // ----------------------------
    // Read reply
    // ----------------------------
    auto readRes =
        nodes[0]->syncStreamReadExactly(streamId, payload.size());

    if (!readRes.ok)
        qFatal("Stream read failed");

    QByteArray received = readRes.data.value<QByteArray>();

    qDebug() << "Received bytes:" << received.size();

    // ----------------------------
    // Close stream
    // ----------------------------
    nodes[0]->syncStreamClose(streamId);
    nodes[0]->syncStreamRelease(streamId);

    qDebug() << "Stream closed";

    // ----------------------------
    // Shutdown
    // ----------------------------
    for (auto *node : nodes) {
        node->syncLibp2pStop();
        delete node;
    }

    qDebug() << "Done";

    return 0;
}
