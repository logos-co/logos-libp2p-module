#include <logos_test.h>
#include <plugin.h>
#include <QThread>

static Libp2pModuleOptions discoOptions() {
    return Libp2pModuleOptions{ .mountServiceDiscovery = true };
}

LOGOS_TEST(disco_start_stop) {
    Libp2pModulePlugin node(discoOptions());
    LOGOS_ASSERT_TRUE(node.syncLibp2pStart().ok);

    LOGOS_ASSERT_TRUE(node.syncDiscoStart().ok);
    LOGOS_ASSERT_TRUE(node.syncDiscoStop().ok);

    LOGOS_ASSERT_TRUE(node.syncLibp2pStop().ok);
}

LOGOS_TEST(disco_advertise_and_lookup) {
    Libp2pModulePlugin nodeC(discoOptions());
    LOGOS_ASSERT_TRUE(nodeC.syncLibp2pStart().ok);
    LOGOS_ASSERT_TRUE(nodeC.syncDiscoStart().ok);
    PeerInfo infoC = nodeC.syncPeerInfo().data.value<PeerInfo>();

    Libp2pModuleOptions optsA = discoOptions();
    optsA.bootstrapNodes = { infoC };
    Libp2pModulePlugin nodeA(optsA);
    LOGOS_ASSERT_TRUE(nodeA.syncLibp2pStart().ok);
    LOGOS_ASSERT_TRUE(nodeA.syncDiscoStart().ok);
    LOGOS_ASSERT_TRUE(nodeA.syncConnectPeer(infoC.peerId, infoC.addrs, 5000).ok);

    Libp2pModuleOptions optsB = discoOptions();
    optsB.bootstrapNodes = { infoC };
    Libp2pModulePlugin nodeB(optsB);
    LOGOS_ASSERT_TRUE(nodeB.syncLibp2pStart().ok);
    LOGOS_ASSERT_TRUE(nodeB.syncDiscoStart().ok);
    LOGOS_ASSERT_TRUE(nodeB.syncConnectPeer(infoC.peerId, infoC.addrs, 5000).ok);

    QString serviceId = "test-service";
    LOGOS_ASSERT_TRUE(nodeA.syncDiscoStartAdvertising(serviceId).ok);
    LOGOS_ASSERT_TRUE(nodeB.syncDiscoStartDiscovering(serviceId).ok);

    QThread::msleep(2000);

    Libp2pResult res = nodeB.syncDiscoLookup(serviceId);
    LOGOS_ASSERT_TRUE(res.ok);

    QList<ExtendedPeerRecord> records =
        res.data.value<QList<ExtendedPeerRecord>>();
    LOGOS_ASSERT_FALSE(records.isEmpty());
    PeerInfo infoA = nodeA.syncPeerInfo().data.value<PeerInfo>();
    LOGOS_ASSERT_TRUE(records[0].peerId == infoA.peerId);

    LOGOS_ASSERT_TRUE(nodeA.syncDiscoStop().ok);
    LOGOS_ASSERT_TRUE(nodeB.syncDiscoStop().ok);
    LOGOS_ASSERT_TRUE(nodeC.syncDiscoStop().ok);

    LOGOS_ASSERT_TRUE(nodeA.syncLibp2pStop().ok);
    LOGOS_ASSERT_TRUE(nodeB.syncLibp2pStop().ok);
    LOGOS_ASSERT_TRUE(nodeC.syncLibp2pStop().ok);
}

LOGOS_TEST(disco_advertise_with_data) {
    Libp2pModulePlugin nodeC(discoOptions());
    LOGOS_ASSERT_TRUE(nodeC.syncLibp2pStart().ok);
    LOGOS_ASSERT_TRUE(nodeC.syncDiscoStart().ok);
    PeerInfo infoC = nodeC.syncPeerInfo().data.value<PeerInfo>();

    Libp2pModuleOptions optsA = discoOptions();
    optsA.bootstrapNodes = { infoC };
    Libp2pModulePlugin nodeA(optsA);
    LOGOS_ASSERT_TRUE(nodeA.syncLibp2pStart().ok);
    LOGOS_ASSERT_TRUE(nodeA.syncDiscoStart().ok);
    LOGOS_ASSERT_TRUE(nodeA.syncConnectPeer(infoC.peerId, infoC.addrs, 5000).ok);

    Libp2pModuleOptions optsB = discoOptions();
    optsB.bootstrapNodes = { infoC };
    Libp2pModulePlugin nodeB(optsB);
    LOGOS_ASSERT_TRUE(nodeB.syncLibp2pStart().ok);
    LOGOS_ASSERT_TRUE(nodeB.syncDiscoStart().ok);
    LOGOS_ASSERT_TRUE(nodeB.syncConnectPeer(infoC.peerId, infoC.addrs, 5000).ok);

    QString serviceId = "data-service";
    QByteArray serviceData = "version=2;proto=test";

    LOGOS_ASSERT_TRUE(nodeA.syncDiscoStartAdvertising(serviceId, serviceData).ok);
    LOGOS_ASSERT_TRUE(nodeB.syncDiscoStartDiscovering(serviceId).ok);

    QThread::msleep(2000);

    Libp2pResult res = nodeB.syncDiscoLookup(serviceId, serviceData);
    LOGOS_ASSERT_TRUE(res.ok);

    QList<ExtendedPeerRecord> records =
        res.data.value<QList<ExtendedPeerRecord>>();
    LOGOS_ASSERT_FALSE(records.isEmpty());

    LOGOS_ASSERT_TRUE(nodeA.syncDiscoStop().ok);
    LOGOS_ASSERT_TRUE(nodeB.syncDiscoStop().ok);
    LOGOS_ASSERT_TRUE(nodeC.syncDiscoStop().ok);

    LOGOS_ASSERT_TRUE(nodeA.syncLibp2pStop().ok);
    LOGOS_ASSERT_TRUE(nodeB.syncLibp2pStop().ok);
    LOGOS_ASSERT_TRUE(nodeC.syncLibp2pStop().ok);
}

LOGOS_TEST(disco_start_stop_advertising) {
    Libp2pModulePlugin nodeA(discoOptions());
    Libp2pModulePlugin nodeB(discoOptions());

    LOGOS_ASSERT_TRUE(nodeA.syncLibp2pStart().ok);
    LOGOS_ASSERT_TRUE(nodeB.syncLibp2pStart().ok);

    LOGOS_ASSERT_TRUE(nodeA.syncDiscoStart().ok);
    LOGOS_ASSERT_TRUE(nodeB.syncDiscoStart().ok);

    PeerInfo infoA = nodeA.syncPeerInfo().data.value<PeerInfo>();
    LOGOS_ASSERT_TRUE(nodeB.syncConnectPeer(infoA.peerId, infoA.addrs, 500).ok);

    QString serviceId = "ephemeral-service";

    LOGOS_ASSERT_TRUE(nodeA.syncDiscoStartAdvertising(serviceId).ok);
    QThread::msleep(500);

    LOGOS_ASSERT_TRUE(nodeA.syncDiscoStopAdvertising(serviceId).ok);
    QThread::msleep(500);

    Libp2pResult res = nodeB.syncDiscoLookup(serviceId);
    LOGOS_ASSERT_TRUE(res.ok);

    QList<ExtendedPeerRecord> records =
        res.data.value<QList<ExtendedPeerRecord>>();
    LOGOS_ASSERT_TRUE(records.isEmpty());

    LOGOS_ASSERT_TRUE(nodeA.syncDiscoStop().ok);
    LOGOS_ASSERT_TRUE(nodeB.syncDiscoStop().ok);

    LOGOS_ASSERT_TRUE(nodeA.syncLibp2pStop().ok);
    LOGOS_ASSERT_TRUE(nodeB.syncLibp2pStop().ok);
}

LOGOS_TEST(disco_random_lookup) {
    Libp2pModulePlugin nodeA(discoOptions());
    Libp2pModulePlugin nodeB(discoOptions());

    LOGOS_ASSERT_TRUE(nodeA.syncLibp2pStart().ok);
    LOGOS_ASSERT_TRUE(nodeB.syncLibp2pStart().ok);

    LOGOS_ASSERT_TRUE(nodeA.syncDiscoStart().ok);
    LOGOS_ASSERT_TRUE(nodeB.syncDiscoStart().ok);

    PeerInfo infoA = nodeA.syncPeerInfo().data.value<PeerInfo>();
    LOGOS_ASSERT_TRUE(nodeB.syncConnectPeer(infoA.peerId, infoA.addrs, 500).ok);

    QThread::msleep(500);

    Libp2pResult res = nodeA.syncDiscoRandomLookup();
    LOGOS_ASSERT_TRUE(res.ok);

    LOGOS_ASSERT_TRUE(nodeA.syncDiscoStop().ok);
    LOGOS_ASSERT_TRUE(nodeB.syncDiscoStop().ok);

    LOGOS_ASSERT_TRUE(nodeA.syncLibp2pStop().ok);
    LOGOS_ASSERT_TRUE(nodeB.syncLibp2pStop().ok);
}
