#include <logos_test.h>
#include <plugin.h>
#include <vector>
#include <random>
#include <memory>
#include "test_helpers.h"

LOGOS_TEST(mix_dial_and_reply) {
    const int NUM_NODES = 5;

    std::vector<std::unique_ptr<Libp2pModuleImpl>> nodes;
    std::vector<std::pair<std::string, std::vector<std::string>>> infos(NUM_NODES);
    std::vector<std::string> mixPrivKeys(NUM_NODES);
    std::vector<std::string> mixPubKeys(NUM_NODES);
    std::vector<std::string> libp2pPubKeys(NUM_NODES);

    for (int i = 0; i < NUM_NODES; ++i) {
        nodes.push_back(std::make_unique<Libp2pModuleImpl>(
            Libp2pModuleOptions{ .mountMix = true }));
        LOGOS_ASSERT_TRUE(nodes[i]->start().success);

        infos[i] = getPeerInfoPair(*nodes[i]);
        LOGOS_ASSERT_FALSE(infos[i].first.empty());
        LOGOS_ASSERT_FALSE(infos[i].second.empty());

        auto privRes = nodes[i]->mixGeneratePrivKey();
        LOGOS_ASSERT_TRUE(privRes.success);
        mixPrivKeys[i] = privRes.value.get<std::string>();

        auto pubRes = nodes[i]->mixPublicKey(mixPrivKeys[i]);
        LOGOS_ASSERT_TRUE(pubRes.success);
        mixPubKeys[i] = pubRes.value.get<std::string>();

        auto lpPubRes = nodes[i]->publicKey();
        LOGOS_ASSERT_TRUE(lpPubRes.success);
        libp2pPubKeys[i] = lpPubRes.value.get<std::string>();

        LOGOS_ASSERT_TRUE(nodes[i]->mixSetNodeInfo(
            infos[i].second[0], mixPrivKeys[i]).success);

        LOGOS_ASSERT_TRUE(nodes[i]->mixRegisterDestReadBehavior(
            "/ipfs/ping/1.0.0", LIBP2P_MIX_READ_EXACTLY, 32).success);
    }

    for (int i = 0; i < NUM_NODES; ++i) {
        for (int j = 0; j < NUM_NODES; ++j) {
            if (i == j) continue;

            LOGOS_ASSERT_TRUE(nodes[i]->mixNodepoolAdd(
                infos[j].first, infos[j].second[0],
                mixPubKeys[j], libp2pPubKeys[j]).success);
        }
    }

    auto dialResult = nodes[0]->mixDialWithReply(
        infos[4].first, infos[4].second[0],
        "/ipfs/ping/1.0.0", 1, 0);

    LOGOS_ASSERT_TRUE(dialResult.success);
    uint64_t streamId = dialResult.value.get<uint64_t>();

    std::string payload(32, '\0');
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, 255);
    for (size_t i = 0; i < payload.size(); ++i)
        payload[i] = static_cast<char>(dist(gen));

    LOGOS_ASSERT_TRUE(nodes[0]->streamWrite(streamId, payload).success);

    auto readRes = nodes[0]->streamReadExactly(streamId, payload.size());
    LOGOS_ASSERT_TRUE(readRes.success);
    std::string received = readRes.value.get<std::string>();
    LOGOS_ASSERT_TRUE(received == payload);

    LOGOS_ASSERT_TRUE(nodes[0]->streamClose(streamId).success);
    LOGOS_ASSERT_TRUE(nodes[0]->streamRelease(streamId).success);

    for (int i = 0; i < NUM_NODES; ++i) {
        LOGOS_ASSERT_TRUE(nodes[i]->stop().success);
    }
}

LOGOS_TEST(mix_nodepool_routing) {
    Libp2pModuleImpl nodeA(Libp2pModuleOptions{ .mountMix = true });
    Libp2pModuleImpl nodeB(Libp2pModuleOptions{ .mountMix = true });
    Libp2pModuleImpl nodeC(Libp2pModuleOptions{ .mountMix = true });

    LOGOS_ASSERT_TRUE(nodeA.start().success);
    LOGOS_ASSERT_TRUE(nodeB.start().success);
    LOGOS_ASSERT_TRUE(nodeC.start().success);

    auto [peerIdA, addrsA] = getPeerInfoPair(nodeA);
    auto [peerIdB, addrsB] = getPeerInfoPair(nodeB);
    auto [peerIdC, addrsC] = getPeerInfoPair(nodeC);

    auto privARes = nodeA.mixGeneratePrivKey();
    auto privBRes = nodeB.mixGeneratePrivKey();
    auto privCRes = nodeC.mixGeneratePrivKey();
    LOGOS_ASSERT_TRUE(privARes.success);
    LOGOS_ASSERT_TRUE(privBRes.success);
    LOGOS_ASSERT_TRUE(privCRes.success);

    std::string privA = privARes.value.get<std::string>();
    std::string privB = privBRes.value.get<std::string>();
    std::string privC = privCRes.value.get<std::string>();

    auto pubARes = nodeA.mixPublicKey(privA);
    auto pubBRes = nodeB.mixPublicKey(privB);
    auto pubCRes = nodeC.mixPublicKey(privC);
    LOGOS_ASSERT_TRUE(pubARes.success);
    LOGOS_ASSERT_TRUE(pubBRes.success);
    LOGOS_ASSERT_TRUE(pubCRes.success);

    std::string pubA = pubARes.value.get<std::string>();
    std::string pubB = pubBRes.value.get<std::string>();
    std::string pubC = pubCRes.value.get<std::string>();

    auto lpPubARes = nodeA.publicKey();
    auto lpPubBRes = nodeB.publicKey();
    auto lpPubCRes = nodeC.publicKey();
    LOGOS_ASSERT_TRUE(lpPubARes.success);
    LOGOS_ASSERT_TRUE(lpPubBRes.success);
    LOGOS_ASSERT_TRUE(lpPubCRes.success);

    std::string libp2pPubA = lpPubARes.value.get<std::string>();
    std::string libp2pPubB = lpPubBRes.value.get<std::string>();
    std::string libp2pPubC = lpPubCRes.value.get<std::string>();

    LOGOS_ASSERT_TRUE(nodeA.mixSetNodeInfo(addrsA[0], privA).success);
    LOGOS_ASSERT_TRUE(nodeB.mixSetNodeInfo(addrsB[0], privB).success);
    LOGOS_ASSERT_TRUE(nodeC.mixSetNodeInfo(addrsC[0], privC).success);

    LOGOS_ASSERT_TRUE(nodeA.mixNodepoolAdd(peerIdB, addrsB[0], pubB, libp2pPubB).success);
    LOGOS_ASSERT_TRUE(nodeA.mixNodepoolAdd(peerIdC, addrsC[0], pubC, libp2pPubC).success);

    LOGOS_ASSERT_TRUE(nodeB.mixNodepoolAdd(peerIdA, addrsA[0], pubA, libp2pPubA).success);
    LOGOS_ASSERT_TRUE(nodeC.mixNodepoolAdd(peerIdA, addrsA[0], pubA, libp2pPubA).success);

    LOGOS_ASSERT_TRUE(nodeA.stop().success);
    LOGOS_ASSERT_TRUE(nodeB.stop().success);
    LOGOS_ASSERT_TRUE(nodeC.stop().success);
}
