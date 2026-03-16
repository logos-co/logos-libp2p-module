#include <QtTest>
#include <plugin.h>

class TestIntegration : public QObject
{
    Q_OBJECT

private slots:

    void bootstrapNodeAutoConnect()
    {
        // setup node A
        Libp2pModulePlugin nodeA;
        QVERIFY(nodeA.syncLibp2pStart().ok);
        PeerInfo nodeAPeerInfo = nodeA.syncPeerInfo().data.value<PeerInfo>();

        // setup node B with node A as bootstrap
        Libp2pModulePlugin nodeB(Libp2pModuleOptions{ .bootstrapNodes = { nodeAPeerInfo } });
        QVERIFY(nodeB.syncLibp2pStart().ok);

        auto connectedPeers = nodeB.syncConnectedPeers(Direction_Out).data.value<QList<QString>>();
        QVERIFY(!connectedPeers.isEmpty());
        QCOMPARE(connectedPeers[0], nodeAPeerInfo.peerId);

        QVERIFY(nodeA.syncLibp2pStop().ok);
        QVERIFY(nodeB.syncLibp2pStop().ok);
    }

    void connectAndDisconnect()
    {
        Libp2pModulePlugin nodeA;
        Libp2pModulePlugin nodeB;

        QVERIFY(nodeA.syncLibp2pStart().ok);
        QVERIFY(nodeB.syncLibp2pStart().ok);

        PeerInfo nodeAPeerInfo = nodeA.syncPeerInfo().data.value<PeerInfo>();

        // cannot disconnect, invalid peerId
        QVERIFY(!nodeB.syncDisconnectPeer("fakePeerId").ok);

        QVERIFY(nodeB.syncConnectPeer(nodeAPeerInfo.peerId, nodeAPeerInfo.addrs, 500).ok);

        QCOMPARE(nodeB.syncConnectedPeers(Direction_Out).data.value<QList<QString>>().size(), 1);

        QCOMPARE(nodeA.syncConnectedPeers(Direction_In).data.value<QList<QString>>().size(), 1);

        QVERIFY(nodeB.syncDisconnectPeer(nodeAPeerInfo.peerId).ok);

        QVERIFY(nodeA.syncLibp2pStop().ok);
        QVERIFY(nodeB.syncLibp2pStop().ok);
    }

    void reconnectAfterDisconnect()
    {
        Libp2pModulePlugin nodeA;
        Libp2pModulePlugin nodeB;

        QVERIFY(nodeA.syncLibp2pStart().ok);
        QVERIFY(nodeB.syncLibp2pStart().ok);

        PeerInfo nodeBPeerInfo = nodeB.syncPeerInfo().data.value<PeerInfo>();

        // connect
        QVERIFY(nodeA.syncConnectPeer(nodeBPeerInfo.peerId, nodeBPeerInfo.addrs, 500).ok);
        QCOMPARE(nodeA.syncConnectedPeers(Direction_Out).data.value<QList<QString>>().size(), 1);

        // disconnect
        QVERIFY(nodeA.syncDisconnectPeer(nodeBPeerInfo.peerId).ok);
        QCOMPARE(nodeA.syncConnectedPeers(Direction_Out).data.value<QList<QString>>().size(), 0);

        // reconnect
        QVERIFY(nodeA.syncConnectPeer(nodeBPeerInfo.peerId, nodeBPeerInfo.addrs, 500).ok);
        QCOMPARE(nodeA.syncConnectedPeers(Direction_Out).data.value<QList<QString>>().size(), 1);

        // verify the connection works by dialing ping
        const int PING_SIZE = 32;
        Libp2pResult dialResult = nodeA.syncDial(nodeBPeerInfo.peerId, "/ipfs/ping/1.0.0");
        QVERIFY(dialResult.ok);

        uint64_t streamId = dialResult.data.value<uint64_t>();

        QByteArray payload(PING_SIZE, 0);
        for (int i = 0; i < PING_SIZE; ++i)
            payload[i] = static_cast<char>(i);
        QVERIFY(nodeA.syncStreamWrite(streamId, payload).ok);

        Libp2pResult readResult = nodeA.syncStreamReadExactly(streamId, PING_SIZE);
        QVERIFY(readResult.ok);
        QCOMPARE(readResult.data.value<QByteArray>(), payload);

        QVERIFY(nodeA.syncStreamCloseWithEOF(streamId).ok);
        QVERIFY(nodeA.syncStreamRelease(streamId).ok);

        QVERIFY(nodeA.syncLibp2pStop().ok);
        QVERIFY(nodeB.syncLibp2pStop().ok);
    }

    void multipleStreamsOnSameConnection()
    {
        const int PING_SIZE = 32;

        Libp2pModulePlugin nodeA;
        Libp2pModulePlugin nodeB;

        QVERIFY(nodeA.syncLibp2pStart().ok);
        QVERIFY(nodeB.syncLibp2pStart().ok);

        PeerInfo nodeBPeerInfo = nodeB.syncPeerInfo().data.value<PeerInfo>();
        QVERIFY(nodeA.syncConnectPeer(nodeBPeerInfo.peerId, nodeBPeerInfo.addrs, 500).ok);

        // open two streams on the same connection
        Libp2pResult dial1 = nodeA.syncDial(nodeBPeerInfo.peerId, "/ipfs/ping/1.0.0");
        QVERIFY(dial1.ok);
        uint64_t stream1 = dial1.data.value<uint64_t>();

        Libp2pResult dial2 = nodeA.syncDial(nodeBPeerInfo.peerId, "/ipfs/ping/1.0.0");
        QVERIFY(dial2.ok);
        uint64_t stream2 = dial2.data.value<uint64_t>();

        QVERIFY(stream1 != stream2);

        // write different payloads to each stream
        QByteArray payload1(PING_SIZE, 0);
        QByteArray payload2(PING_SIZE, 0);
        for (int i = 0; i < PING_SIZE; ++i) {
            payload1[i] = static_cast<char>(i);
            payload2[i] = static_cast<char>(255 - i);
        }

        QVERIFY(nodeA.syncStreamWrite(stream1, payload1).ok);
        QVERIFY(nodeA.syncStreamWrite(stream2, payload2).ok);

        // read back from each stream and verify no cross-contamination
        Libp2pResult read1 = nodeA.syncStreamReadExactly(stream1, PING_SIZE);
        QVERIFY(read1.ok);
        QCOMPARE(read1.data.value<QByteArray>(), payload1);

        Libp2pResult read2 = nodeA.syncStreamReadExactly(stream2, PING_SIZE);
        QVERIFY(read2.ok);
        QCOMPARE(read2.data.value<QByteArray>(), payload2);

        // cleanup both streams
        QVERIFY(nodeA.syncStreamCloseWithEOF(stream1).ok);
        QVERIFY(nodeA.syncStreamRelease(stream1).ok);
        QVERIFY(nodeA.syncStreamCloseWithEOF(stream2).ok);
        QVERIFY(nodeA.syncStreamRelease(stream2).ok);

        QVERIFY(nodeA.syncLibp2pStop().ok);
        QVERIFY(nodeB.syncLibp2pStop().ok);
    }

    void directDialStreamExchange()
    {
        const int PING_SIZE = 32;

        Libp2pModulePlugin nodeA;
        Libp2pModulePlugin nodeB;

        QVERIFY(nodeA.syncLibp2pStart().ok);
        QVERIFY(nodeB.syncLibp2pStart().ok);

        PeerInfo nodeBPeerInfo = nodeB.syncPeerInfo().data.value<PeerInfo>();

        // connect A to B
        QVERIFY(nodeA.syncConnectPeer(nodeBPeerInfo.peerId, nodeBPeerInfo.addrs, 500).ok);

        // A dials B directly on /ipfs/ping/1.0.0
        Libp2pResult dialResult = nodeA.syncDial(nodeBPeerInfo.peerId, "/ipfs/ping/1.0.0");
        QVERIFY(dialResult.ok);

        uint64_t streamId = dialResult.data.value<uint64_t>();
        QVERIFY(streamId != 0);

        // send ping payload
        QByteArray payload(PING_SIZE, 0);
        for (int i = 0; i < PING_SIZE; ++i)
            payload[i] = static_cast<char>(i);
        QVERIFY(nodeA.syncStreamWrite(streamId, payload).ok);

        // read ping echo
        Libp2pResult readResult = nodeA.syncStreamReadExactly(streamId, PING_SIZE);
        QVERIFY(readResult.ok);
        QCOMPARE(readResult.data.value<QByteArray>(), payload);

        // cleanup
        QVERIFY(nodeA.syncStreamCloseWithEOF(streamId).ok);
        QVERIFY(nodeA.syncStreamRelease(streamId).ok);

        QVERIFY(nodeA.syncLibp2pStop().ok);
        QVERIFY(nodeB.syncLibp2pStop().ok);
    }

    void circuitRelayRouting()
    {
        const int PING_SIZE = 32;

        // setup relay server node
        Libp2pModulePlugin relay(Libp2pModuleOptions{ .circuitRelay = true });
        QVERIFY(relay.syncLibp2pStart().ok);
        PeerInfo relayPeerInfo = relay.syncPeerInfo().data.value<PeerInfo>();

        // setup node A (relay client)
        Libp2pModulePlugin nodeA(Libp2pModuleOptions{ .circuitRelayClient = true });
        QVERIFY(nodeA.syncLibp2pStart().ok);

        // setup node B (relay client)
        Libp2pModulePlugin nodeB(Libp2pModuleOptions{ .circuitRelayClient = true });
        QVERIFY(nodeB.syncLibp2pStart().ok);
        PeerInfo nodeBPeerInfo = nodeB.syncPeerInfo().data.value<PeerInfo>();

        // connect both clients to server
        QVERIFY(nodeA.syncConnectPeer(relayPeerInfo.peerId, relayPeerInfo.addrs, 500).ok);
        QVERIFY(nodeB.syncConnectPeer(relayPeerInfo.peerId, relayPeerInfo.addrs, 500).ok);

        // node B reserves a slot on the relay and gets relay circuit addresses
        Libp2pResult rsvp = nodeB.syncCircuitRelayReserve(relayPeerInfo.peerId, relayPeerInfo.addrs);
        QVERIFY(rsvp.ok);
        QList<QString> rsvpAddrs = rsvp.data.value<QList<QString>>();
        QVERIFY(!rsvpAddrs.isEmpty());

        QString relayAddr = rsvpAddrs[0] + "/p2p-circuit";

        // node A dials node B through the relay using ping
        Libp2pResult dialResult = nodeA.syncDialCircuitRelay(nodeBPeerInfo.peerId, relayAddr, "/ipfs/ping/1.0.0");
        QVERIFY(dialResult.ok);
        
        uint64_t streamId = dialResult.data.value<uint64_t>();
        QVERIFY(streamId != 0);

        // send ping payload
        QByteArray payload(PING_SIZE, 0);
        for (int i = 0; i < PING_SIZE; ++i)
            payload[i] = static_cast<char>(i);
        QVERIFY(nodeA.syncStreamWrite(streamId, payload).ok);

        // read ping echo
        Libp2pResult readResult = nodeA.syncStreamReadExactly(streamId, PING_SIZE);
        QVERIFY(readResult.ok);
        QCOMPARE(readResult.data.value<QByteArray>(), payload);

        // cleanup
        QVERIFY(nodeA.syncStreamCloseWithEOF(streamId).ok);
        QVERIFY(nodeA.syncStreamRelease(streamId).ok);

        QVERIFY(relay.syncLibp2pStop().ok);
        QVERIFY(nodeA.syncLibp2pStop().ok);
        QVERIFY(nodeB.syncLibp2pStop().ok);
    }

    void lpStreamExchange()
    {
        // Use a 31-byte payload so that the LP frame (1-byte varint + 31 bytes)
        // gives exactly 32 bytes — the ping echo size. The echoed bytes form
        // a valid LP frame that readLp can decode back into the original payload.
        const int LP_PAYLOAD_SIZE = 31;

        Libp2pModulePlugin nodeA;
        Libp2pModulePlugin nodeB;

        QVERIFY(nodeA.syncLibp2pStart().ok);
        QVERIFY(nodeB.syncLibp2pStart().ok);

        PeerInfo nodeBPeerInfo = nodeB.syncPeerInfo().data.value<PeerInfo>();
        QVERIFY(nodeA.syncConnectPeer(nodeBPeerInfo.peerId, nodeBPeerInfo.addrs, 500).ok);

        Libp2pResult dialResult = nodeA.syncDial(nodeBPeerInfo.peerId, "/ipfs/ping/1.0.0");
        QVERIFY(dialResult.ok);

        uint64_t streamId = dialResult.data.value<uint64_t>();
        QVERIFY(streamId != 0);

        QByteArray payload(LP_PAYLOAD_SIZE, 0);
        for (int i = 0; i < LP_PAYLOAD_SIZE; ++i)
            payload[i] = static_cast<char>(i);

        QVERIFY(nodeA.syncStreamWriteLp(streamId, payload).ok);

        Libp2pResult readResult = nodeA.syncStreamReadLp(streamId, 4096);
        QVERIFY(readResult.ok);
        QCOMPARE(readResult.data.value<QByteArray>(), payload);

        QVERIFY(nodeA.syncStreamCloseWithEOF(streamId).ok);
        QVERIFY(nodeA.syncStreamRelease(streamId).ok);

        QVERIFY(nodeA.syncLibp2pStop().ok);
        QVERIFY(nodeB.syncLibp2pStop().ok);
    }

    void quicPingRoundTrip()
    {
        const int PING_SIZE = 32;

        Libp2pModuleOptions opts{ .transport = LIBP2P_TRANSPORT_QUIC };

        Libp2pModulePlugin nodeA(opts);
        Libp2pModulePlugin nodeB(opts);

        QVERIFY(nodeA.syncLibp2pStart().ok);
        QVERIFY(nodeB.syncLibp2pStart().ok);

        PeerInfo nodeBPeerInfo = nodeB.syncPeerInfo().data.value<PeerInfo>();
        QVERIFY(nodeA.syncConnectPeer(nodeBPeerInfo.peerId, nodeBPeerInfo.addrs, 500).ok);

        Libp2pResult dialResult = nodeA.syncDial(nodeBPeerInfo.peerId, "/ipfs/ping/1.0.0");
        QVERIFY(dialResult.ok);

        uint64_t streamId = dialResult.data.value<uint64_t>();
        QVERIFY(streamId != 0);

        QByteArray payload(PING_SIZE, 0);
        for (int i = 0; i < PING_SIZE; ++i)
            payload[i] = static_cast<char>(i);

        QVERIFY(nodeA.syncStreamWrite(streamId, payload).ok);

        Libp2pResult readResult = nodeA.syncStreamReadExactly(streamId, PING_SIZE);
        QVERIFY(readResult.ok);
        QCOMPARE(readResult.data.value<QByteArray>(), payload);

        QVERIFY(nodeA.syncStreamCloseWithEOF(streamId).ok);
        QVERIFY(nodeA.syncStreamRelease(streamId).ok);

        QVERIFY(nodeA.syncLibp2pStop().ok);
        QVERIFY(nodeB.syncLibp2pStop().ok);
    }
};

QTEST_MAIN(TestIntegration)
#include "integration.moc"
