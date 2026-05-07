#include <logos_test.h>
#include <plugin.h>

LOGOS_TEST(integration_bootstrap_auto_connect) {
    Libp2pModulePlugin nodeA;
    LOGOS_ASSERT_TRUE(nodeA.syncLibp2pStart().ok);
    PeerInfo nodeAPeerInfo = nodeA.syncPeerInfo().data.value<PeerInfo>();

    Libp2pModulePlugin nodeB(Libp2pModuleOptions{ .bootstrapNodes = { nodeAPeerInfo } });
    LOGOS_ASSERT_TRUE(nodeB.syncLibp2pStart().ok);

    auto connectedPeers = nodeB.syncConnectedPeers(Direction_Out).data.value<QList<QString>>();
    LOGOS_ASSERT_FALSE(connectedPeers.isEmpty());
    LOGOS_ASSERT_TRUE(connectedPeers[0] == nodeAPeerInfo.peerId);

    LOGOS_ASSERT_TRUE(nodeA.syncLibp2pStop().ok);
    LOGOS_ASSERT_TRUE(nodeB.syncLibp2pStop().ok);
}

LOGOS_TEST(integration_connect_and_disconnect) {
    Libp2pModulePlugin nodeA;
    Libp2pModulePlugin nodeB;

    LOGOS_ASSERT_TRUE(nodeA.syncLibp2pStart().ok);
    LOGOS_ASSERT_TRUE(nodeB.syncLibp2pStart().ok);

    PeerInfo nodeAPeerInfo = nodeA.syncPeerInfo().data.value<PeerInfo>();

    LOGOS_ASSERT_FALSE(nodeB.syncDisconnectPeer("fakePeerId").ok);
    LOGOS_ASSERT_TRUE(nodeB.syncConnectPeer(nodeAPeerInfo.peerId, nodeAPeerInfo.addrs, 500).ok);

    LOGOS_ASSERT_EQ(nodeB.syncConnectedPeers(Direction_Out).data.value<QList<QString>>().size(), qsizetype(1));
    LOGOS_ASSERT_EQ(nodeA.syncConnectedPeers(Direction_In).data.value<QList<QString>>().size(), qsizetype(1));

    LOGOS_ASSERT_TRUE(nodeB.syncDisconnectPeer(nodeAPeerInfo.peerId).ok);

    LOGOS_ASSERT_TRUE(nodeA.syncLibp2pStop().ok);
    LOGOS_ASSERT_TRUE(nodeB.syncLibp2pStop().ok);
}

LOGOS_TEST(integration_reconnect_after_disconnect) {
    Libp2pModulePlugin nodeA;
    Libp2pModulePlugin nodeB;

    LOGOS_ASSERT_TRUE(nodeA.syncLibp2pStart().ok);
    LOGOS_ASSERT_TRUE(nodeB.syncLibp2pStart().ok);

    PeerInfo nodeBPeerInfo = nodeB.syncPeerInfo().data.value<PeerInfo>();

    LOGOS_ASSERT_TRUE(nodeA.syncConnectPeer(nodeBPeerInfo.peerId, nodeBPeerInfo.addrs, 500).ok);
    LOGOS_ASSERT_EQ(nodeA.syncConnectedPeers(Direction_Out).data.value<QList<QString>>().size(), qsizetype(1));

    LOGOS_ASSERT_TRUE(nodeA.syncDisconnectPeer(nodeBPeerInfo.peerId).ok);
    LOGOS_ASSERT_EQ(nodeA.syncConnectedPeers(Direction_Out).data.value<QList<QString>>().size(), qsizetype(0));

    LOGOS_ASSERT_TRUE(nodeA.syncConnectPeer(nodeBPeerInfo.peerId, nodeBPeerInfo.addrs, 500).ok);
    LOGOS_ASSERT_EQ(nodeA.syncConnectedPeers(Direction_Out).data.value<QList<QString>>().size(), qsizetype(1));

    const int PING_SIZE = 32;
    Libp2pResult dialResult = nodeA.syncDial(nodeBPeerInfo.peerId, "/ipfs/ping/1.0.0");
    LOGOS_ASSERT_TRUE(dialResult.ok);

    uint64_t streamId = dialResult.data.value<uint64_t>();

    QByteArray payload(PING_SIZE, 0);
    for (int i = 0; i < PING_SIZE; ++i)
        payload[i] = static_cast<char>(i);
    LOGOS_ASSERT_TRUE(nodeA.syncStreamWrite(streamId, payload).ok);

    Libp2pResult readResult = nodeA.syncStreamReadExactly(streamId, PING_SIZE);
    LOGOS_ASSERT_TRUE(readResult.ok);
    LOGOS_ASSERT_TRUE(readResult.data.value<QByteArray>() == payload);

    LOGOS_ASSERT_TRUE(nodeA.syncStreamCloseWithEOF(streamId).ok);
    LOGOS_ASSERT_TRUE(nodeA.syncStreamRelease(streamId).ok);

    LOGOS_ASSERT_TRUE(nodeA.syncLibp2pStop().ok);
    LOGOS_ASSERT_TRUE(nodeB.syncLibp2pStop().ok);
}

LOGOS_TEST(integration_multiple_streams_on_same_connection) {
    const int PING_SIZE = 32;

    Libp2pModulePlugin nodeA;
    Libp2pModulePlugin nodeB;

    LOGOS_ASSERT_TRUE(nodeA.syncLibp2pStart().ok);
    LOGOS_ASSERT_TRUE(nodeB.syncLibp2pStart().ok);

    PeerInfo nodeBPeerInfo = nodeB.syncPeerInfo().data.value<PeerInfo>();
    LOGOS_ASSERT_TRUE(nodeA.syncConnectPeer(nodeBPeerInfo.peerId, nodeBPeerInfo.addrs, 500).ok);

    Libp2pResult dial1 = nodeA.syncDial(nodeBPeerInfo.peerId, "/ipfs/ping/1.0.0");
    LOGOS_ASSERT_TRUE(dial1.ok);
    uint64_t stream1 = dial1.data.value<uint64_t>();

    Libp2pResult dial2 = nodeA.syncDial(nodeBPeerInfo.peerId, "/ipfs/ping/1.0.0");
    LOGOS_ASSERT_TRUE(dial2.ok);
    uint64_t stream2 = dial2.data.value<uint64_t>();

    LOGOS_ASSERT_NE(stream1, stream2);

    QByteArray payload1(PING_SIZE, 0);
    QByteArray payload2(PING_SIZE, 0);
    for (int i = 0; i < PING_SIZE; ++i) {
        payload1[i] = static_cast<char>(i);
        payload2[i] = static_cast<char>(255 - i);
    }

    LOGOS_ASSERT_TRUE(nodeA.syncStreamWrite(stream1, payload1).ok);
    LOGOS_ASSERT_TRUE(nodeA.syncStreamWrite(stream2, payload2).ok);

    Libp2pResult read1 = nodeA.syncStreamReadExactly(stream1, PING_SIZE);
    LOGOS_ASSERT_TRUE(read1.ok);
    LOGOS_ASSERT_TRUE(read1.data.value<QByteArray>() == payload1);

    Libp2pResult read2 = nodeA.syncStreamReadExactly(stream2, PING_SIZE);
    LOGOS_ASSERT_TRUE(read2.ok);
    LOGOS_ASSERT_TRUE(read2.data.value<QByteArray>() == payload2);

    LOGOS_ASSERT_TRUE(nodeA.syncStreamCloseWithEOF(stream1).ok);
    LOGOS_ASSERT_TRUE(nodeA.syncStreamRelease(stream1).ok);
    LOGOS_ASSERT_TRUE(nodeA.syncStreamCloseWithEOF(stream2).ok);
    LOGOS_ASSERT_TRUE(nodeA.syncStreamRelease(stream2).ok);

    LOGOS_ASSERT_TRUE(nodeA.syncLibp2pStop().ok);
    LOGOS_ASSERT_TRUE(nodeB.syncLibp2pStop().ok);
}

LOGOS_TEST(integration_direct_dial_stream_exchange) {
    const int PING_SIZE = 32;

    Libp2pModulePlugin nodeA;
    Libp2pModulePlugin nodeB;

    LOGOS_ASSERT_TRUE(nodeA.syncLibp2pStart().ok);
    LOGOS_ASSERT_TRUE(nodeB.syncLibp2pStart().ok);

    PeerInfo nodeBPeerInfo = nodeB.syncPeerInfo().data.value<PeerInfo>();
    LOGOS_ASSERT_TRUE(nodeA.syncConnectPeer(nodeBPeerInfo.peerId, nodeBPeerInfo.addrs, 500).ok);

    Libp2pResult dialResult = nodeA.syncDial(nodeBPeerInfo.peerId, "/ipfs/ping/1.0.0");
    LOGOS_ASSERT_TRUE(dialResult.ok);

    uint64_t streamId = dialResult.data.value<uint64_t>();
    LOGOS_ASSERT_NE(streamId, static_cast<uint64_t>(0));

    QByteArray payload(PING_SIZE, 0);
    for (int i = 0; i < PING_SIZE; ++i)
        payload[i] = static_cast<char>(i);
    LOGOS_ASSERT_TRUE(nodeA.syncStreamWrite(streamId, payload).ok);

    Libp2pResult readResult = nodeA.syncStreamReadExactly(streamId, PING_SIZE);
    LOGOS_ASSERT_TRUE(readResult.ok);
    LOGOS_ASSERT_TRUE(readResult.data.value<QByteArray>() == payload);

    LOGOS_ASSERT_TRUE(nodeA.syncStreamCloseWithEOF(streamId).ok);
    LOGOS_ASSERT_TRUE(nodeA.syncStreamRelease(streamId).ok);

    LOGOS_ASSERT_TRUE(nodeA.syncLibp2pStop().ok);
    LOGOS_ASSERT_TRUE(nodeB.syncLibp2pStop().ok);
}

LOGOS_TEST(integration_circuit_relay_routing) {
    const int PING_SIZE = 32;

    Libp2pModulePlugin relay(Libp2pModuleOptions{ .circuitRelay = true });
    LOGOS_ASSERT_TRUE(relay.syncLibp2pStart().ok);
    PeerInfo relayPeerInfo = relay.syncPeerInfo().data.value<PeerInfo>();

    Libp2pModulePlugin nodeA(Libp2pModuleOptions{ .circuitRelayClient = true });
    LOGOS_ASSERT_TRUE(nodeA.syncLibp2pStart().ok);

    Libp2pModulePlugin nodeB(Libp2pModuleOptions{ .circuitRelayClient = true });
    LOGOS_ASSERT_TRUE(nodeB.syncLibp2pStart().ok);
    PeerInfo nodeBPeerInfo = nodeB.syncPeerInfo().data.value<PeerInfo>();

    LOGOS_ASSERT_TRUE(nodeA.syncConnectPeer(relayPeerInfo.peerId, relayPeerInfo.addrs, 500).ok);
    LOGOS_ASSERT_TRUE(nodeB.syncConnectPeer(relayPeerInfo.peerId, relayPeerInfo.addrs, 500).ok);

    Libp2pResult rsvp = nodeB.syncCircuitRelayReserve(relayPeerInfo.peerId, relayPeerInfo.addrs);
    LOGOS_ASSERT_TRUE(rsvp.ok);
    QList<QString> rsvpAddrs = rsvp.data.value<QList<QString>>();
    LOGOS_ASSERT_FALSE(rsvpAddrs.isEmpty());

    QString relayAddr = rsvpAddrs[0] + "/p2p-circuit";

    Libp2pResult dialResult = nodeA.syncDialCircuitRelay(nodeBPeerInfo.peerId, relayAddr, "/ipfs/ping/1.0.0");
    LOGOS_ASSERT_TRUE(dialResult.ok);

    uint64_t streamId = dialResult.data.value<uint64_t>();
    LOGOS_ASSERT_NE(streamId, static_cast<uint64_t>(0));

    QByteArray payload(PING_SIZE, 0);
    for (int i = 0; i < PING_SIZE; ++i)
        payload[i] = static_cast<char>(i);
    LOGOS_ASSERT_TRUE(nodeA.syncStreamWrite(streamId, payload).ok);

    Libp2pResult readResult = nodeA.syncStreamReadExactly(streamId, PING_SIZE);
    LOGOS_ASSERT_TRUE(readResult.ok);
    LOGOS_ASSERT_TRUE(readResult.data.value<QByteArray>() == payload);

    LOGOS_ASSERT_TRUE(nodeA.syncStreamCloseWithEOF(streamId).ok);
    LOGOS_ASSERT_TRUE(nodeA.syncStreamRelease(streamId).ok);

    LOGOS_ASSERT_TRUE(relay.syncLibp2pStop().ok);
    LOGOS_ASSERT_TRUE(nodeA.syncLibp2pStop().ok);
    LOGOS_ASSERT_TRUE(nodeB.syncLibp2pStop().ok);
}

LOGOS_TEST(integration_lp_stream_exchange) {
    const int LP_PAYLOAD_SIZE = 31;

    Libp2pModulePlugin nodeA;
    Libp2pModulePlugin nodeB;

    LOGOS_ASSERT_TRUE(nodeA.syncLibp2pStart().ok);
    LOGOS_ASSERT_TRUE(nodeB.syncLibp2pStart().ok);

    PeerInfo nodeBPeerInfo = nodeB.syncPeerInfo().data.value<PeerInfo>();
    LOGOS_ASSERT_TRUE(nodeA.syncConnectPeer(nodeBPeerInfo.peerId, nodeBPeerInfo.addrs, 500).ok);

    Libp2pResult dialResult = nodeA.syncDial(nodeBPeerInfo.peerId, "/ipfs/ping/1.0.0");
    LOGOS_ASSERT_TRUE(dialResult.ok);

    uint64_t streamId = dialResult.data.value<uint64_t>();
    LOGOS_ASSERT_NE(streamId, static_cast<uint64_t>(0));

    QByteArray payload(LP_PAYLOAD_SIZE, 0);
    for (int i = 0; i < LP_PAYLOAD_SIZE; ++i)
        payload[i] = static_cast<char>(i);

    LOGOS_ASSERT_TRUE(nodeA.syncStreamWriteLp(streamId, payload).ok);

    Libp2pResult readResult = nodeA.syncStreamReadLp(streamId, 4096);
    LOGOS_ASSERT_TRUE(readResult.ok);
    LOGOS_ASSERT_TRUE(readResult.data.value<QByteArray>() == payload);

    LOGOS_ASSERT_TRUE(nodeA.syncStreamCloseWithEOF(streamId).ok);
    LOGOS_ASSERT_TRUE(nodeA.syncStreamRelease(streamId).ok);

    LOGOS_ASSERT_TRUE(nodeA.syncLibp2pStop().ok);
    LOGOS_ASSERT_TRUE(nodeB.syncLibp2pStop().ok);
}

LOGOS_TEST(integration_quic_ping_round_trip) {
    const int PING_SIZE = 32;

    Libp2pModuleOptions opts{ .transport = LIBP2P_TRANSPORT_QUIC };

    Libp2pModulePlugin nodeA(opts);
    Libp2pModulePlugin nodeB(opts);

    LOGOS_ASSERT_TRUE(nodeA.syncLibp2pStart().ok);
    LOGOS_ASSERT_TRUE(nodeB.syncLibp2pStart().ok);

    PeerInfo nodeBPeerInfo = nodeB.syncPeerInfo().data.value<PeerInfo>();
    LOGOS_ASSERT_TRUE(nodeA.syncConnectPeer(nodeBPeerInfo.peerId, nodeBPeerInfo.addrs, 500).ok);

    Libp2pResult dialResult = nodeA.syncDial(nodeBPeerInfo.peerId, "/ipfs/ping/1.0.0");
    LOGOS_ASSERT_TRUE(dialResult.ok);

    uint64_t streamId = dialResult.data.value<uint64_t>();
    LOGOS_ASSERT_NE(streamId, static_cast<uint64_t>(0));

    QByteArray payload(PING_SIZE, 0);
    for (int i = 0; i < PING_SIZE; ++i)
        payload[i] = static_cast<char>(i);

    LOGOS_ASSERT_TRUE(nodeA.syncStreamWrite(streamId, payload).ok);

    Libp2pResult readResult = nodeA.syncStreamReadExactly(streamId, PING_SIZE);
    LOGOS_ASSERT_TRUE(readResult.ok);
    LOGOS_ASSERT_TRUE(readResult.data.value<QByteArray>() == payload);

    LOGOS_ASSERT_TRUE(nodeA.syncStreamCloseWithEOF(streamId).ok);
    LOGOS_ASSERT_TRUE(nodeA.syncStreamRelease(streamId).ok);

    LOGOS_ASSERT_TRUE(nodeA.syncLibp2pStop().ok);
    LOGOS_ASSERT_TRUE(nodeB.syncLibp2pStop().ok);
}
