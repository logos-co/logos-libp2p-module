#include <logos_test.h>
#include <plugin.h>
#include <vector>
#include <random>
#include <memory>

LOGOS_TEST(mix_dial_and_reply) {
    const int NUM_NODES = 5;

    std::vector<std::unique_ptr<Libp2pModulePlugin>> nodes;
    QVector<PeerInfo> infos(NUM_NODES);
    QVector<QByteArray> mixPrivKeys(NUM_NODES);
    QVector<QByteArray> mixPubKeys(NUM_NODES);
    QVector<QByteArray> libp2pPubKeys(NUM_NODES);

    for (int i = 0; i < NUM_NODES; ++i) {
        nodes.push_back(std::make_unique<Libp2pModulePlugin>(Libp2pModuleOptions{ .mountMix = true }));
        LOGOS_ASSERT_TRUE(nodes[i]->syncLibp2pStart().ok);

        infos[i] = nodes[i]->syncPeerInfo().data.value<PeerInfo>();
        LOGOS_ASSERT_FALSE(infos[i].peerId.isEmpty());
        LOGOS_ASSERT_FALSE(infos[i].addrs.isEmpty());

        mixPrivKeys[i] = nodes[i]->mixGeneratePrivKey();
        mixPubKeys[i] = nodes[i]->mixPublicKey(mixPrivKeys[i]);

        libp2pPubKeys[i] =
            nodes[i]->syncLibp2pPublicKey().data.value<QByteArray>();

        LOGOS_ASSERT_TRUE(nodes[i]->syncMixSetNodeInfo(
                    infos[i].addrs[0],
                    mixPrivKeys[i]).ok);

        LOGOS_ASSERT_TRUE(nodes[i]->syncMixRegisterDestReadBehavior(
                    "/ipfs/ping/1.0.0",
                    LIBP2P_MIX_READ_EXACTLY,
                    32).ok);
    }

    for (int i = 0; i < NUM_NODES; ++i) {
        for (int j = 0; j < NUM_NODES; ++j) {
            if (i == j)
                continue;

            LOGOS_ASSERT_TRUE(nodes[i]->syncMixNodepoolAdd(
                        infos[j].peerId,
                        infos[j].addrs[0],
                        mixPubKeys[j],
                        libp2pPubKeys[j]).ok);
        }
    }

    Libp2pResult dialResult = nodes[0]->syncMixDialWithReply(
        infos[4].peerId,
        infos[4].addrs[0],
        "/ipfs/ping/1.0.0",
        1,
        0
    );

    LOGOS_ASSERT_TRUE(dialResult.ok);
    uint64_t streamId = dialResult.data.value<uint64_t>();

    QByteArray payload(32, 0);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, 255);
    for (int i = 0; i < payload.size(); ++i)
        payload[i] = static_cast<char>(dist(gen));

    LOGOS_ASSERT_TRUE(nodes[0]->syncStreamWrite(streamId, payload).ok);

    Libp2pResult readRes =
        nodes[0]->syncStreamReadExactly(streamId, payload.size());

    LOGOS_ASSERT_TRUE(readRes.ok);
    QByteArray received = readRes.data.value<QByteArray>();
    LOGOS_ASSERT_TRUE(received == payload);

    LOGOS_ASSERT_TRUE(nodes[0]->syncStreamClose(streamId).ok);
    LOGOS_ASSERT_TRUE(nodes[0]->syncStreamRelease(streamId).ok);

    for (int i = 0; i < NUM_NODES; ++i) {
        LOGOS_ASSERT_TRUE(nodes[i]->syncLibp2pStop().ok);
    }
}

LOGOS_TEST(mix_nodepool_routing) {
    Libp2pModulePlugin nodeA(Libp2pModuleOptions{ .mountMix = true });
    Libp2pModulePlugin nodeB(Libp2pModuleOptions{ .mountMix = true });
    Libp2pModulePlugin nodeC(Libp2pModuleOptions{ .mountMix = true });

    LOGOS_ASSERT_TRUE(nodeA.syncLibp2pStart().ok);
    LOGOS_ASSERT_TRUE(nodeB.syncLibp2pStart().ok);
    LOGOS_ASSERT_TRUE(nodeC.syncLibp2pStart().ok);

    PeerInfo infoA = nodeA.syncPeerInfo().data.value<PeerInfo>();
    PeerInfo infoB = nodeB.syncPeerInfo().data.value<PeerInfo>();
    PeerInfo infoC = nodeC.syncPeerInfo().data.value<PeerInfo>();

    QByteArray privA = nodeA.mixGeneratePrivKey();
    QByteArray privB = nodeB.mixGeneratePrivKey();
    QByteArray privC = nodeC.mixGeneratePrivKey();

    QByteArray pubA = nodeA.mixPublicKey(privA);
    QByteArray pubB = nodeB.mixPublicKey(privB);
    QByteArray pubC = nodeC.mixPublicKey(privC);

    QByteArray libp2pPubA = nodeA.syncLibp2pPublicKey().data.value<QByteArray>();
    QByteArray libp2pPubB = nodeB.syncLibp2pPublicKey().data.value<QByteArray>();
    QByteArray libp2pPubC = nodeC.syncLibp2pPublicKey().data.value<QByteArray>();

    LOGOS_ASSERT_TRUE(nodeA.syncMixSetNodeInfo(infoA.addrs[0], privA).ok);
    LOGOS_ASSERT_TRUE(nodeB.syncMixSetNodeInfo(infoB.addrs[0], privB).ok);
    LOGOS_ASSERT_TRUE(nodeC.syncMixSetNodeInfo(infoC.addrs[0], privC).ok);

    LOGOS_ASSERT_TRUE(nodeA.syncMixNodepoolAdd(infoB.peerId, infoB.addrs[0], pubB, libp2pPubB).ok);
    LOGOS_ASSERT_TRUE(nodeA.syncMixNodepoolAdd(infoC.peerId, infoC.addrs[0], pubC, libp2pPubC).ok);

    LOGOS_ASSERT_TRUE(nodeB.syncMixNodepoolAdd(infoA.peerId, infoA.addrs[0], pubA, libp2pPubA).ok);
    LOGOS_ASSERT_TRUE(nodeC.syncMixNodepoolAdd(infoA.peerId, infoA.addrs[0], pubA, libp2pPubA).ok);

    LOGOS_ASSERT_TRUE(nodeA.syncLibp2pStop().ok);
    LOGOS_ASSERT_TRUE(nodeB.syncLibp2pStop().ok);
    LOGOS_ASSERT_TRUE(nodeC.syncLibp2pStop().ok);
}
