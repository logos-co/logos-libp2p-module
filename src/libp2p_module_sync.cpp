#include "libp2p_module_plugin.h"

#include <QEventLoop>
#include <QTimer>
#include <QMetaObject>
#include <QVariant>
#include <QElapsedTimer>

template<typename AsyncCall>
static WaitResult runSync(Libp2pModulePlugin* self, AsyncCall asyncCall)
{
    WaitResult result{false, QVariant{}};

    QEventLoop loop;
    QMetaObject::Connection conn;

    conn = QObject::connect(
        self,
        &Libp2pModulePlugin::libp2pEvent,
        self,
        [&](int ret,
            const QString &reqId,
            const QString &caller,
            const QString &message,
            const QVariant &data)
        {
            if (reqId != result.data.toString())
                return;

            result.ok = (ret == RET_OK);
            if (caller == "toCid")
                result.data = message;
            else
                result.data = data;

            QObject::disconnect(conn);
            loop.quit();
        });

    QString uuid = asyncCall();

    if (uuid.isEmpty()) {
        QObject::disconnect(conn);
        return result;
    }

    // temporarily store UUID in result.data for matching
    result.data = uuid;

    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(10000);

    loop.exec();

    // if timeout, UUID is still stored
    if (result.data.typeId() == QMetaType::QString && result.data.toString() == uuid)
        result.data = QVariant{};

    QObject::disconnect(conn);
    return result;
}

/* ---------------------------
 * Start / Stop
 * --------------------------- */

bool Libp2pModulePlugin::syncLibp2pStart()
{
    return runSync(this, [&]() { return libp2pStart(); }).ok;
}

bool Libp2pModulePlugin::syncLibp2pStop()
{
    return runSync(this, [&]() { return libp2pStop(); }).ok;
}

/* ---------------------------
 * Connectivity
 * --------------------------- */

bool Libp2pModulePlugin::syncConnectPeer(const QString &peerId, const QList<QString> multiaddrs, int64_t timeoutMs)
{
    return runSync(this, [&]() { return connectPeer(peerId, multiaddrs, timeoutMs); }).ok;
}

bool Libp2pModulePlugin::syncDisconnectPeer(const QString &peerId)
{
    return runSync(this, [&]() { return disconnectPeer(peerId); }).ok;
}

PeerInfo Libp2pModulePlugin::syncPeerInfo()
{
    auto res = runSync(this, [&]() { return peerInfo(); });
    return res.ok ? res.data.value<PeerInfo>() : PeerInfo();
}

QList<QString> Libp2pModulePlugin::syncConnectedPeers(int direction)
{
    auto res = runSync(this, [&]() { return connectedPeers(direction); });
    return res.ok ? res.data.value<QList<QString>>() : QList<QString>();
}

uint64_t Libp2pModulePlugin::syncDial(const QString &peerId, const QString &proto)
{
    auto res = runSync(this, [&]() { return dial(peerId, proto); });
    return res.ok ? res.data.value<uint64_t>() : 0;
}

/* ---------------------------
 * Stream
 * --------------------------- */

QByteArray Libp2pModulePlugin::syncStreamReadExactly(uint64_t streamId, size_t len)
{
    auto res = runSync(this, [&]() { return streamReadExactly(streamId, len); });
    return res.ok ? res.data.value<QByteArray>() : QByteArray();
}

QByteArray Libp2pModulePlugin::syncStreamReadLp(uint64_t streamId, size_t maxSize)
{
    auto res = runSync(this, [&]() { return streamReadLp(streamId, maxSize); });
    return res.ok ? res.data.value<QByteArray>() : QByteArray();
}

bool Libp2pModulePlugin::syncStreamWrite(uint64_t streamId, const QByteArray &data)
{
    return runSync(this, [&]() { return streamWrite(streamId, data); }).ok;
}

bool Libp2pModulePlugin::syncStreamWriteLp(uint64_t streamId, const QByteArray &data)
{
    return runSync(this, [&]() { return streamWriteLp(streamId, data); }).ok;
}

bool Libp2pModulePlugin::syncStreamClose(uint64_t streamId)
{
    return runSync(this, [&]() { return streamClose(streamId); }).ok;
}

bool Libp2pModulePlugin::syncStreamCloseEOF(uint64_t streamId)
{
    return runSync(this, [&]() { return streamCloseEOF(streamId); }).ok;
}

bool Libp2pModulePlugin::syncStreamRelease(uint64_t streamId)
{
    return runSync(this, [&]() { return streamRelease(streamId); }).ok;
}

/* ---------------------------
 * Kademlia
 * --------------------------- */

QList<QString> Libp2pModulePlugin::syncKadFindNode(const QString &peerId)
{
    auto res = runSync(this, [&]() { return kadFindNode(peerId); });
    return res.ok ? res.data.value<QList<QString>>() : QList<QString>();
}

bool Libp2pModulePlugin::syncKadPutValue(const QByteArray &key, const QByteArray &value)
{
    return runSync(this, [&]() { return kadPutValue(key, value); }).ok;
}

QByteArray Libp2pModulePlugin::syncKadGetValue(const QByteArray &key, int quorum)
{
    auto res = runSync(this, [&]() { return kadGetValue(key, quorum); });
    return res.ok ? res.data.toByteArray() : QByteArray{};
}

bool Libp2pModulePlugin::syncKadAddProvider(const QString &cid)
{
    return runSync(this, [&]() { return kadAddProvider(cid); }).ok;
}

QList<PeerInfo> Libp2pModulePlugin::syncKadGetProviders(const QString &cid)
{
    auto res = runSync(this, [&]() { return kadGetProviders(cid); });
    return res.ok ? res.data.value<QList<PeerInfo>>() : QList<PeerInfo>();
}

bool Libp2pModulePlugin::syncKadStartProviding(const QString &cid)
{
    return runSync(this, [&]() { return kadStartProviding(cid); }).ok;
}

bool Libp2pModulePlugin::syncKadStopProviding(const QString &cid)
{
    return runSync(this, [&]() { return kadStopProviding(cid); }).ok;
}

QList<ExtendedPeerRecord> Libp2pModulePlugin::syncKadGetRandomRecords()
{
    auto res = runSync(this, [&]() { return kadGetRandomRecords(); });
    return res.ok ? res.data.value<QList<ExtendedPeerRecord>>() : QList<ExtendedPeerRecord>();
}

QString Libp2pModulePlugin::syncToCid(const QByteArray &key)
{
    auto res = runSync(this, [&]() { return toCid(key); });
    return res.ok ? res.data.toString() : QString{};
}
