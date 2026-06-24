#include <logos_test.h>
#include <plugin.h>
#include "test_helpers.h"

LOGOS_TEST(integration_bootstrap_auto_connect) {
    Libp2pModuleImpl nodeA;
    LOGOS_ASSERT_TRUE(nodeA.start().success);
    auto [peerIdA, addrsA] = getPeerInfoPair(nodeA);

    Libp2pModuleImpl nodeB(Libp2pModuleOptions{
        .bootstrapNodes = { {peerIdA, addrsA} }
    });
    LOGOS_ASSERT_TRUE(nodeB.start().success);

    auto peersRes = nodeB.connectedPeers(Direction_Out);
    LOGOS_ASSERT_TRUE(peersRes.success);
    auto peers = peersRes.value;
    LOGOS_ASSERT_FALSE(peers.empty());
    LOGOS_ASSERT_TRUE(peers[0].get<std::string>() == peerIdA);

    LOGOS_ASSERT_TRUE(nodeA.stop().success);
    LOGOS_ASSERT_TRUE(nodeB.stop().success);
}

LOGOS_TEST(integration_connect_and_disconnect) {
    Libp2pModuleImpl nodeA;
    Libp2pModuleImpl nodeB;

    LOGOS_ASSERT_TRUE(nodeA.start().success);
    LOGOS_ASSERT_TRUE(nodeB.start().success);

    auto [peerIdA, addrsA] = getPeerInfoPair(nodeA);

    LOGOS_ASSERT_FALSE(nodeB.disconnectPeer("fakePeerId").success);
    LOGOS_ASSERT_TRUE(nodeB.connectPeer(peerIdA, addrsA, 500).success);

    auto outPeers = nodeB.connectedPeers(Direction_Out).value;
    LOGOS_ASSERT_EQ(outPeers.size(), size_t(1));
    auto inPeers = nodeA.connectedPeers(Direction_In).value;
    LOGOS_ASSERT_EQ(inPeers.size(), size_t(1));

    LOGOS_ASSERT_TRUE(nodeB.disconnectPeer(peerIdA).success);

    LOGOS_ASSERT_TRUE(nodeA.stop().success);
    LOGOS_ASSERT_TRUE(nodeB.stop().success);
}

LOGOS_TEST(integration_reconnect_after_disconnect) {
    Libp2pModuleImpl nodeA;
    Libp2pModuleImpl nodeB;

    LOGOS_ASSERT_TRUE(nodeA.start().success);
    LOGOS_ASSERT_TRUE(nodeB.start().success);

    auto [peerIdB, addrsB] = getPeerInfoPair(nodeB);

    LOGOS_ASSERT_TRUE(nodeA.connectPeer(peerIdB, addrsB, 500).success);
    LOGOS_ASSERT_EQ(nodeA.connectedPeers(Direction_Out).value.size(), size_t(1));

    LOGOS_ASSERT_TRUE(nodeA.disconnectPeer(peerIdB).success);
    LOGOS_ASSERT_EQ(nodeA.connectedPeers(Direction_Out).value.size(), size_t(0));

    LOGOS_ASSERT_TRUE(nodeA.connectPeer(peerIdB, addrsB, 500).success);
    LOGOS_ASSERT_EQ(nodeA.connectedPeers(Direction_Out).value.size(), size_t(1));

    const int PING_SIZE = 32;
    auto dialResult = nodeA.dial(peerIdB, "/ipfs/ping/1.0.0");
    LOGOS_ASSERT_TRUE(dialResult.success);

    uint64_t streamId = dialResult.value.get<uint64_t>();

    std::string payload(PING_SIZE, '\0');
    for (int i = 0; i < PING_SIZE; ++i)
        payload[i] = static_cast<char>(i);
    LOGOS_ASSERT_TRUE(nodeA.streamWrite(streamId, payload).success);

    auto readResult = nodeA.streamReadExactly(streamId, PING_SIZE);
    LOGOS_ASSERT_TRUE(readResult.success);
    LOGOS_ASSERT_TRUE(base64Decode(readResult.value.get<std::string>()) == payload);

    LOGOS_ASSERT_TRUE(nodeA.streamCloseWithEOF(streamId).success);
    LOGOS_ASSERT_TRUE(nodeA.streamRelease(streamId).success);

    LOGOS_ASSERT_TRUE(nodeA.stop().success);
    LOGOS_ASSERT_TRUE(nodeB.stop().success);
}

LOGOS_TEST(integration_multiple_streams_on_same_connection) {
    const int PING_SIZE = 32;

    Libp2pModuleImpl nodeA;
    Libp2pModuleImpl nodeB;

    LOGOS_ASSERT_TRUE(nodeA.start().success);
    LOGOS_ASSERT_TRUE(nodeB.start().success);

    auto [peerIdB, addrsB] = getPeerInfoPair(nodeB);
    LOGOS_ASSERT_TRUE(nodeA.connectPeer(peerIdB, addrsB, 500).success);

    auto dial1 = nodeA.dial(peerIdB, "/ipfs/ping/1.0.0");
    LOGOS_ASSERT_TRUE(dial1.success);
    uint64_t stream1 = dial1.value.get<uint64_t>();

    auto dial2 = nodeA.dial(peerIdB, "/ipfs/ping/1.0.0");
    LOGOS_ASSERT_TRUE(dial2.success);
    uint64_t stream2 = dial2.value.get<uint64_t>();

    LOGOS_ASSERT_NE(stream1, stream2);

    std::string payload1(PING_SIZE, '\0');
    std::string payload2(PING_SIZE, '\0');
    for (int i = 0; i < PING_SIZE; ++i) {
        payload1[i] = static_cast<char>(i);
        payload2[i] = static_cast<char>(255 - i);
    }

    LOGOS_ASSERT_TRUE(nodeA.streamWrite(stream1, payload1).success);
    LOGOS_ASSERT_TRUE(nodeA.streamWrite(stream2, payload2).success);

    auto read1 = nodeA.streamReadExactly(stream1, PING_SIZE);
    LOGOS_ASSERT_TRUE(read1.success);
    LOGOS_ASSERT_TRUE(base64Decode(read1.value.get<std::string>()) == payload1);

    auto read2 = nodeA.streamReadExactly(stream2, PING_SIZE);
    LOGOS_ASSERT_TRUE(read2.success);
    LOGOS_ASSERT_TRUE(base64Decode(read2.value.get<std::string>()) == payload2);

    LOGOS_ASSERT_TRUE(nodeA.streamCloseWithEOF(stream1).success);
    LOGOS_ASSERT_TRUE(nodeA.streamRelease(stream1).success);
    LOGOS_ASSERT_TRUE(nodeA.streamCloseWithEOF(stream2).success);
    LOGOS_ASSERT_TRUE(nodeA.streamRelease(stream2).success);

    LOGOS_ASSERT_TRUE(nodeA.stop().success);
    LOGOS_ASSERT_TRUE(nodeB.stop().success);
}

LOGOS_TEST(integration_direct_dial_stream_exchange) {
    const int PING_SIZE = 32;

    Libp2pModuleImpl nodeA;
    Libp2pModuleImpl nodeB;

    LOGOS_ASSERT_TRUE(nodeA.start().success);
    LOGOS_ASSERT_TRUE(nodeB.start().success);

    auto [peerIdB, addrsB] = getPeerInfoPair(nodeB);
    LOGOS_ASSERT_TRUE(nodeA.connectPeer(peerIdB, addrsB, 500).success);

    auto dialResult = nodeA.dial(peerIdB, "/ipfs/ping/1.0.0");
    LOGOS_ASSERT_TRUE(dialResult.success);

    uint64_t streamId = dialResult.value.get<uint64_t>();
    LOGOS_ASSERT_NE(streamId, static_cast<uint64_t>(0));

    std::string payload(PING_SIZE, '\0');
    for (int i = 0; i < PING_SIZE; ++i)
        payload[i] = static_cast<char>(i);
    LOGOS_ASSERT_TRUE(nodeA.streamWrite(streamId, payload).success);

    auto readResult = nodeA.streamReadExactly(streamId, PING_SIZE);
    LOGOS_ASSERT_TRUE(readResult.success);
    LOGOS_ASSERT_TRUE(base64Decode(readResult.value.get<std::string>()) == payload);

    LOGOS_ASSERT_TRUE(nodeA.streamCloseWithEOF(streamId).success);
    LOGOS_ASSERT_TRUE(nodeA.streamRelease(streamId).success);

    LOGOS_ASSERT_TRUE(nodeA.stop().success);
    LOGOS_ASSERT_TRUE(nodeB.stop().success);
}

LOGOS_TEST(integration_circuit_relay_routing) {
    const int PING_SIZE = 32;

    Libp2pModuleImpl relay(Libp2pModuleOptions{ .circuitRelay = true });
    LOGOS_ASSERT_TRUE(relay.start().success);
    auto [relayPeerId, relayAddrs] = getPeerInfoPair(relay);

    Libp2pModuleImpl nodeA(Libp2pModuleOptions{ .circuitRelayClient = true });
    LOGOS_ASSERT_TRUE(nodeA.start().success);

    Libp2pModuleImpl nodeB(Libp2pModuleOptions{ .circuitRelayClient = true });
    LOGOS_ASSERT_TRUE(nodeB.start().success);
    auto [peerIdB, addrsB] = getPeerInfoPair(nodeB);

    LOGOS_ASSERT_TRUE(nodeA.connectPeer(relayPeerId, relayAddrs, 500).success);
    LOGOS_ASSERT_TRUE(nodeB.connectPeer(relayPeerId, relayAddrs, 500).success);

    auto rsvp = nodeB.circuitRelayReserve(relayPeerId, relayAddrs);
    LOGOS_ASSERT_TRUE(rsvp.success);
    auto rsvpAddrs = rsvp.value;
    LOGOS_ASSERT_FALSE(rsvpAddrs.empty());

    std::string relayAddr = rsvpAddrs[0].get<std::string>() + "/p2p-circuit";

    auto dialResult = nodeA.dialCircuitRelay(peerIdB, relayAddr, "/ipfs/ping/1.0.0");
    LOGOS_ASSERT_TRUE(dialResult.success);

    uint64_t streamId = dialResult.value.get<uint64_t>();
    LOGOS_ASSERT_NE(streamId, static_cast<uint64_t>(0));

    std::string payload(PING_SIZE, '\0');
    for (int i = 0; i < PING_SIZE; ++i)
        payload[i] = static_cast<char>(i);
    LOGOS_ASSERT_TRUE(nodeA.streamWrite(streamId, payload).success);

    auto readResult = nodeA.streamReadExactly(streamId, PING_SIZE);
    LOGOS_ASSERT_TRUE(readResult.success);
    LOGOS_ASSERT_TRUE(base64Decode(readResult.value.get<std::string>()) == payload);

    LOGOS_ASSERT_TRUE(nodeA.streamCloseWithEOF(streamId).success);
    LOGOS_ASSERT_TRUE(nodeA.streamRelease(streamId).success);

    LOGOS_ASSERT_TRUE(relay.stop().success);
    LOGOS_ASSERT_TRUE(nodeA.stop().success);
    LOGOS_ASSERT_TRUE(nodeB.stop().success);
}

LOGOS_TEST(integration_lp_stream_exchange) {
    const int LP_PAYLOAD_SIZE = 31;

    Libp2pModuleImpl nodeA;
    Libp2pModuleImpl nodeB;

    LOGOS_ASSERT_TRUE(nodeA.start().success);
    LOGOS_ASSERT_TRUE(nodeB.start().success);

    auto [peerIdB, addrsB] = getPeerInfoPair(nodeB);
    LOGOS_ASSERT_TRUE(nodeA.connectPeer(peerIdB, addrsB, 500).success);

    auto dialResult = nodeA.dial(peerIdB, "/ipfs/ping/1.0.0");
    LOGOS_ASSERT_TRUE(dialResult.success);

    uint64_t streamId = dialResult.value.get<uint64_t>();
    LOGOS_ASSERT_NE(streamId, static_cast<uint64_t>(0));

    std::string payload(LP_PAYLOAD_SIZE, '\0');
    for (int i = 0; i < LP_PAYLOAD_SIZE; ++i)
        payload[i] = static_cast<char>(i);

    LOGOS_ASSERT_TRUE(nodeA.streamWriteLp(streamId, payload).success);

    auto readResult = nodeA.streamReadLp(streamId, 4096);
    LOGOS_ASSERT_TRUE(readResult.success);
    LOGOS_ASSERT_TRUE(base64Decode(readResult.value.get<std::string>()) == payload);

    LOGOS_ASSERT_TRUE(nodeA.streamCloseWithEOF(streamId).success);
    LOGOS_ASSERT_TRUE(nodeA.streamRelease(streamId).success);

    LOGOS_ASSERT_TRUE(nodeA.stop().success);
    LOGOS_ASSERT_TRUE(nodeB.stop().success);
}

LOGOS_TEST(integration_quic_ping_round_trip) {
    const int PING_SIZE = 32;

    Libp2pModuleOptions opts{ .transport = LIBP2P_TRANSPORT_QUIC };

    Libp2pModuleImpl nodeA(opts);
    Libp2pModuleImpl nodeB(opts);

    LOGOS_ASSERT_TRUE(nodeA.start().success);
    LOGOS_ASSERT_TRUE(nodeB.start().success);

    auto [peerIdB, addrsB] = getPeerInfoPair(nodeB);
    LOGOS_ASSERT_TRUE(nodeA.connectPeer(peerIdB, addrsB, 500).success);

    auto dialResult = nodeA.dial(peerIdB, "/ipfs/ping/1.0.0");
    LOGOS_ASSERT_TRUE(dialResult.success);

    uint64_t streamId = dialResult.value.get<uint64_t>();
    LOGOS_ASSERT_NE(streamId, static_cast<uint64_t>(0));

    std::string payload(PING_SIZE, '\0');
    for (int i = 0; i < PING_SIZE; ++i)
        payload[i] = static_cast<char>(i);

    LOGOS_ASSERT_TRUE(nodeA.streamWrite(streamId, payload).success);

    auto readResult = nodeA.streamReadExactly(streamId, PING_SIZE);
    LOGOS_ASSERT_TRUE(readResult.success);
    LOGOS_ASSERT_TRUE(base64Decode(readResult.value.get<std::string>()) == payload);

    LOGOS_ASSERT_TRUE(nodeA.streamCloseWithEOF(streamId).success);
    LOGOS_ASSERT_TRUE(nodeA.streamRelease(streamId).success);

    LOGOS_ASSERT_TRUE(nodeA.stop().success);
    LOGOS_ASSERT_TRUE(nodeB.stop().success);
}
