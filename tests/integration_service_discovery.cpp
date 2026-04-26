#include <QtTest>
#include <QThread>
#include <plugin.h>

class TestServiceDiscovery : public QObject
{
    Q_OBJECT

private:
    Libp2pModuleOptions discoOptions()
    {
        return Libp2pModuleOptions{ .serviceDiscovery = true };
    }

private slots:

    void discoStartStop()
    {
        Libp2pModulePlugin node(discoOptions());
        QVERIFY(node.syncLibp2pStart().ok);

        QVERIFY(node.syncDiscoStart().ok);
        QVERIFY(node.syncDiscoStop().ok);

        QVERIFY(node.syncLibp2pStop().ok);
    }

    void advertiseAndLookup()
    {
        // nodeC is the shared routing anchor: A registers with C, B queries C
        Libp2pModulePlugin nodeC(discoOptions());
        QVERIFY(nodeC.syncLibp2pStart().ok);
        QVERIFY(nodeC.syncDiscoStart().ok);
        PeerInfo infoC = nodeC.syncPeerInfo().data.value<PeerInfo>();

        Libp2pModuleOptions optsA = discoOptions();
        optsA.bootstrapNodes = { infoC };
        Libp2pModulePlugin nodeA(optsA);
        QVERIFY(nodeA.syncLibp2pStart().ok);
        QVERIFY(nodeA.syncDiscoStart().ok);
        QVERIFY(nodeA.syncConnectPeer(infoC.peerId, infoC.addrs, 5000).ok);

        Libp2pModuleOptions optsB = discoOptions();
        optsB.bootstrapNodes = { infoC };
        Libp2pModulePlugin nodeB(optsB);
        QVERIFY(nodeB.syncLibp2pStart().ok);
        QVERIFY(nodeB.syncDiscoStart().ok);
        QVERIFY(nodeB.syncConnectPeer(infoC.peerId, infoC.addrs, 5000).ok);

        // A advertises the service; B registers interest to set up routing tables
        QString serviceId = "test-service";
        QVERIFY(nodeA.syncDiscoStartAdvertising(serviceId).ok);
        QVERIFY(nodeB.syncDiscoStartDiscovering(serviceId).ok);

        QThread::msleep(2000);

        // B looks up the service via the shared routing anchor (nodeC)
        Libp2pResult res = nodeB.syncDiscoLookup(serviceId);
        QVERIFY(res.ok);

        QList<ExtendedPeerRecord> records =
            res.data.value<QList<ExtendedPeerRecord>>();
        QVERIFY(!records.isEmpty());
        PeerInfo infoA = nodeA.syncPeerInfo().data.value<PeerInfo>();
        QVERIFY(records[0].peerId == infoA.peerId);

        QVERIFY(nodeA.syncDiscoStop().ok);
        QVERIFY(nodeB.syncDiscoStop().ok);
        QVERIFY(nodeC.syncDiscoStop().ok);

        QVERIFY(nodeA.syncLibp2pStop().ok);
        QVERIFY(nodeB.syncLibp2pStop().ok);
        QVERIFY(nodeC.syncLibp2pStop().ok);
    }

    void advertiseWithData()
    {
        // nodeC is the shared routing anchor: A registers with C, B queries C
        Libp2pModulePlugin nodeC(discoOptions());
        QVERIFY(nodeC.syncLibp2pStart().ok);
        QVERIFY(nodeC.syncDiscoStart().ok);
        PeerInfo infoC = nodeC.syncPeerInfo().data.value<PeerInfo>();

        Libp2pModuleOptions optsA = discoOptions();
        optsA.bootstrapNodes = { infoC };
        Libp2pModulePlugin nodeA(optsA);
        QVERIFY(nodeA.syncLibp2pStart().ok);
        QVERIFY(nodeA.syncDiscoStart().ok);
        QVERIFY(nodeA.syncConnectPeer(infoC.peerId, infoC.addrs, 5000).ok);

        Libp2pModuleOptions optsB = discoOptions();
        optsB.bootstrapNodes = { infoC };
        Libp2pModulePlugin nodeB(optsB);
        QVERIFY(nodeB.syncLibp2pStart().ok);
        QVERIFY(nodeB.syncDiscoStart().ok);
        QVERIFY(nodeB.syncConnectPeer(infoC.peerId, infoC.addrs, 5000).ok);

        QString serviceId = "data-service";
        QByteArray serviceData = "version=2;proto=test";

        // A advertises the service; B registers interest to set up routing tables
        QVERIFY(nodeA.syncDiscoStartAdvertising(serviceId, serviceData).ok);
        QVERIFY(nodeB.syncDiscoStartDiscovering(serviceId).ok);

        QThread::msleep(2000);

        Libp2pResult res = nodeB.syncDiscoLookup(serviceId, serviceData);
        QVERIFY(res.ok);

        QList<ExtendedPeerRecord> records =
            res.data.value<QList<ExtendedPeerRecord>>();
        QVERIFY(!records.isEmpty());

        QVERIFY(nodeA.syncDiscoStop().ok);
        QVERIFY(nodeB.syncDiscoStop().ok);
        QVERIFY(nodeC.syncDiscoStop().ok);

        QVERIFY(nodeA.syncLibp2pStop().ok);
        QVERIFY(nodeB.syncLibp2pStop().ok);
        QVERIFY(nodeC.syncLibp2pStop().ok);
    }

    void startStopAdvertising()
    {
        Libp2pModulePlugin nodeA(discoOptions());
        Libp2pModulePlugin nodeB(discoOptions());

        QVERIFY(nodeA.syncLibp2pStart().ok);
        QVERIFY(nodeB.syncLibp2pStart().ok);

        QVERIFY(nodeA.syncDiscoStart().ok);
        QVERIFY(nodeB.syncDiscoStart().ok);

        PeerInfo infoA = nodeA.syncPeerInfo().data.value<PeerInfo>();
        QVERIFY(nodeB.syncConnectPeer(infoA.peerId, infoA.addrs, 500).ok);

        QString serviceId = "ephemeral-service";

        QVERIFY(nodeA.syncDiscoStartAdvertising(serviceId).ok);
        QThread::msleep(500);

        // A stops advertising
        QVERIFY(nodeA.syncDiscoStopAdvertising(serviceId).ok);
        QThread::msleep(500);

        // B should find no advertisers
        Libp2pResult res = nodeB.syncDiscoLookup(serviceId);
        QVERIFY(res.ok);
        QVERIFY(res.ok);
        QList<ExtendedPeerRecord> records =
            res.data.value<QList<ExtendedPeerRecord>>();
        QVERIFY(records.isEmpty());

        QVERIFY(nodeA.syncDiscoStop().ok);
        QVERIFY(nodeB.syncDiscoStop().ok);

        QVERIFY(nodeA.syncLibp2pStop().ok);
        QVERIFY(nodeB.syncLibp2pStop().ok);
    }

    void randomLookup()
    {
        Libp2pModulePlugin nodeA(discoOptions());
        Libp2pModulePlugin nodeB(discoOptions());

        QVERIFY(nodeA.syncLibp2pStart().ok);
        QVERIFY(nodeB.syncLibp2pStart().ok);

        QVERIFY(nodeA.syncDiscoStart().ok);
        QVERIFY(nodeB.syncDiscoStart().ok);

        PeerInfo infoA = nodeA.syncPeerInfo().data.value<PeerInfo>();
        QVERIFY(nodeB.syncConnectPeer(infoA.peerId, infoA.addrs, 500).ok);

        QThread::msleep(500);

        Libp2pResult res = nodeA.syncDiscoRandomLookup();
        QVERIFY(res.ok);

        QVERIFY(nodeA.syncDiscoStop().ok);
        QVERIFY(nodeB.syncDiscoStop().ok);

        QVERIFY(nodeA.syncLibp2pStop().ok);
        QVERIFY(nodeB.syncLibp2pStop().ok);
    }
};

QTEST_MAIN(TestServiceDiscovery)
#include "integration_service_discovery.moc"
