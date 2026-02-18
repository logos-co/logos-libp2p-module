#include "libp2p_module_plugin.h"

#include <QEventLoop>
#include <QTimer>
#include <QMetaObject>
#include <QVariant>
#include <QElapsedTimer>

template<typename AsyncCall>
static Libp2pResult runSync(Libp2pModulePlugin* self, AsyncCall asyncCall)
{
    Libp2pResult result{false, QString{}, QVariant{}};

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
            result.error = message;
            result.data = data;

            if (caller == "toCid") {
                result.data = message;
            }

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

Libp2pResult Libp2pModulePlugin::syncLibp2pStart()
{
    return runSync(this, [&]() { return libp2pStart(); });
}

Libp2pResult Libp2pModulePlugin::syncLibp2pStop()
{
    return runSync(this, [&]() { return libp2pStop(); });
}

/* ---------------------------
 * Connectivity
 * --------------------------- */

Libp2pResult Libp2pModulePlugin::syncConnectPeer(const QString &peerId, const QList<QString> multiaddrs, int64_t timeoutMs)
{
    return runSync(this, [&]() { return connectPeer(peerId, multiaddrs, timeoutMs); });
}

Libp2pResult Libp2pModulePlugin::syncDisconnectPeer(const QString &peerId)
{
    return runSync(this, [&]() { return disconnectPeer(peerId); });
}

Libp2pResult Libp2pModulePlugin::syncPeerInfo()
{
    return runSync(this, [&]() { return peerInfo(); });
}

Libp2pResult Libp2pModulePlugin::syncConnectedPeers(int direction)
{
    return runSync(this, [&]() { return connectedPeers(direction); });
}

Libp2pResult Libp2pModulePlugin::syncDial(const QString &peerId, const QString &proto)
{
    return runSync(this, [&]() { return dial(peerId, proto); });
}

/* ---------------------------
 * Stream
 * --------------------------- */

Libp2pResult Libp2pModulePlugin::syncStreamReadExactly(uint64_t streamId, size_t len)
{
    return runSync(this, [&]() { return streamReadExactly(streamId, len); });
}

Libp2pResult Libp2pModulePlugin::syncStreamReadLp(uint64_t streamId, size_t maxSize)
{
    return runSync(this, [&]() { return streamReadLp(streamId, maxSize); });
}

Libp2pResult Libp2pModulePlugin::syncStreamWrite(uint64_t streamId, const QByteArray &data)
{
    return runSync(this, [&]() { return streamWrite(streamId, data); });
}

Libp2pResult Libp2pModulePlugin::syncStreamWriteLp(uint64_t streamId, const QByteArray &data)
{
    return runSync(this, [&]() { return streamWriteLp(streamId, data); });
}

Libp2pResult Libp2pModulePlugin::syncStreamClose(uint64_t streamId)
{
    return runSync(this, [&]() { return streamClose(streamId); });
}

Libp2pResult Libp2pModulePlugin::syncStreamCloseEOF(uint64_t streamId)
{
    return runSync(this, [&]() { return streamCloseEOF(streamId); });
}

Libp2pResult Libp2pModulePlugin::syncStreamRelease(uint64_t streamId)
{
    return runSync(this, [&]() { return streamRelease(streamId); });
}

/* ---------------------------
 * Kademlia
 * --------------------------- */

Libp2pResult Libp2pModulePlugin::syncKadFindNode(const QString &peerId)
{
    return runSync(this, [&]() { return kadFindNode(peerId); });
}

Libp2pResult Libp2pModulePlugin::syncKadPutValue(const QByteArray &key, const QByteArray &value)
{
    return runSync(this, [&]() { return kadPutValue(key, value); });
}

Libp2pResult Libp2pModulePlugin::syncKadGetValue(const QByteArray &key, int quorum)
{
    return runSync(this, [&]() { return kadGetValue(key, quorum); });
}

Libp2pResult Libp2pModulePlugin::syncKadAddProvider(const QString &cid)
{
    return runSync(this, [&]() { return kadAddProvider(cid); });
}

Libp2pResult Libp2pModulePlugin::syncKadGetProviders(const QString &cid)
{
    return runSync(this, [&]() { return kadGetProviders(cid); });
}

Libp2pResult Libp2pModulePlugin::syncKadStartProviding(const QString &cid)
{
    return runSync(this, [&]() { return kadStartProviding(cid); });
}

Libp2pResult Libp2pModulePlugin::syncKadStopProviding(const QString &cid)
{
    return runSync(this, [&]() { return kadStopProviding(cid); });
}

Libp2pResult Libp2pModulePlugin::syncKadGetRandomRecords()
{
    return runSync(this, [&]() { return kadGetRandomRecords(); });
}

Libp2pResult Libp2pModulePlugin::syncToCid(const QByteArray &key)
{
    return runSync(this, [&]() { return toCid(key); });
}
