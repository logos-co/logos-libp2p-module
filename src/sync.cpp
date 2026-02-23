#include "plugin.h"

#include <QEventLoop>
#include <QTimer>
#include <QMetaObject>
#include <QVariant>
#include <QElapsedTimer>

template<typename AsyncCall>
static Libp2pResult runSync(Libp2pModulePlugin* self, const char* functionName, AsyncCall asyncCall)
{
    Libp2pResult result{false, QString{}, QVariant{}};

    QEventLoop loop;
    QMetaObject::Connection conn;

    QString requestId;

    conn = QObject::connect(
        self,
        &Libp2pModulePlugin::libp2pEvent,
        &loop,
        [&](int ret,
            const QString &reqId,
            const QString &caller,
            const QString &message,
            const QVariant &data)
        {
            if (requestId.isEmpty())
                return;

            if (reqId != requestId)
                return;

            result.ok = (ret == RET_OK);
            result.error = message;
            result.data = data;

            if (caller == "toCid") {
                result.data = message;
            }

            QObject::disconnect(conn);
            loop.quit();
        }
    );

    requestId = asyncCall();

    if (requestId.isEmpty()) {
        QObject::disconnect(conn);
        qCritical() << functionName << "failed to start async request";
        return result;
    }

    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(10000);

    loop.exec();

    QObject::disconnect(conn);
    return result;
}

/* ---------------------------
 * Start / Stop
 * --------------------------- */

Libp2pResult Libp2pModulePlugin::syncLibp2pStart()
{
    return runSync(this, __func__, [&]() { return libp2pStart(); });
}

Libp2pResult Libp2pModulePlugin::syncLibp2pStop()
{
    return runSync(this, __func__, [&]() { return libp2pStop(); });
}

/* ---------------------------
 * Connectivity
 * --------------------------- */

Libp2pResult Libp2pModulePlugin::syncConnectPeer(const QString &peerId, const QList<QString> multiaddrs, int64_t timeoutMs)
{
    return runSync(this, __func__, [&]() { return connectPeer(peerId, multiaddrs, timeoutMs); });
}

Libp2pResult Libp2pModulePlugin::syncDisconnectPeer(const QString &peerId)
{
    return runSync(this, __func__, [&]() { return disconnectPeer(peerId); });
}

Libp2pResult Libp2pModulePlugin::syncPeerInfo()
{
    return runSync(this, __func__, [&]() { return peerInfo(); });
}

Libp2pResult Libp2pModulePlugin::syncConnectedPeers(int direction)
{
    return runSync(this, __func__, [&]() { return connectedPeers(direction); });
}

Libp2pResult Libp2pModulePlugin::syncDial(const QString &peerId, const QString &proto)
{
    return runSync(this, __func__, [&]() { return dial(peerId, proto); });
}

/* ---------------------------
 * Stream
 * --------------------------- */

Libp2pResult Libp2pModulePlugin::syncStreamReadExactly(uint64_t streamId, size_t len)
{
    return runSync(this, __func__, [&]() { return streamReadExactly(streamId, len); });
}

Libp2pResult Libp2pModulePlugin::syncStreamReadLp(uint64_t streamId, size_t maxSize)
{
    return runSync(this, __func__, [&]() { return streamReadLp(streamId, maxSize); });
}

Libp2pResult Libp2pModulePlugin::syncStreamWrite(uint64_t streamId, const QByteArray &data)
{
    return runSync(this, __func__, [&]() { return streamWrite(streamId, data); });
}

Libp2pResult Libp2pModulePlugin::syncStreamWriteLp(uint64_t streamId, const QByteArray &data)
{
    return runSync(this, __func__, [&]() { return streamWriteLp(streamId, data); });
}

Libp2pResult Libp2pModulePlugin::syncStreamClose(uint64_t streamId)
{
    return runSync(this, __func__, [&]() { return streamClose(streamId); });
}

Libp2pResult Libp2pModulePlugin::syncStreamCloseWithEOF(uint64_t streamId)
{
    return runSync(this, __func__, [&]() { return streamCloseWithEOF(streamId); });
}

Libp2pResult Libp2pModulePlugin::syncStreamRelease(uint64_t streamId)
{
    return runSync(this, __func__, [&]() { return streamRelease(streamId); });
}

/* ---------------------------
 * Kademlia
 * --------------------------- */

Libp2pResult Libp2pModulePlugin::syncKadFindNode(const QString &peerId)
{
    return runSync(this, __func__, [&]() { return kadFindNode(peerId); });
}

Libp2pResult Libp2pModulePlugin::syncKadPutValue(const QByteArray &key, const QByteArray &value)
{
    return runSync(this, __func__, [&]() { return kadPutValue(key, value); });
}

Libp2pResult Libp2pModulePlugin::syncKadGetValue(const QByteArray &key, int quorum)
{
    return runSync(this, __func__, [&]() { return kadGetValue(key, quorum); });
}

Libp2pResult Libp2pModulePlugin::syncKadAddProvider(const QString &cid)
{
    return runSync(this, __func__, [&]() { return kadAddProvider(cid); });
}

Libp2pResult Libp2pModulePlugin::syncKadGetProviders(const QString &cid)
{
    return runSync(this, __func__, [&]() { return kadGetProviders(cid); });
}

Libp2pResult Libp2pModulePlugin::syncKadStartProviding(const QString &cid)
{
    return runSync(this, __func__, [&]() { return kadStartProviding(cid); });
}

Libp2pResult Libp2pModulePlugin::syncKadStopProviding(const QString &cid)
{
    return runSync(this, __func__, [&]() { return kadStopProviding(cid); });
}

Libp2pResult Libp2pModulePlugin::syncKadGetRandomRecords()
{
    return runSync(this, __func__, [&]() { return kadGetRandomRecords(); });
}

Libp2pResult Libp2pModulePlugin::syncToCid(const QByteArray &key)
{
    return runSync(this, __func__, [&]() { return toCid(key); });
}

/* ---------------------------
 * Mix
 * --------------------------- */

Libp2pResult Libp2pModulePlugin::syncMixDial(
    const QString &peerId,
    const QString &multiaddr,
    const QString &proto)
{
    return runSync(this, __func__, [&]() {
        return mixDial(peerId, multiaddr, proto);
    });
}

Libp2pResult Libp2pModulePlugin::syncMixDialWithReply(
    const QString &peerId,
    const QString &multiaddr,
    const QString &proto,
    int expectReply,
    uint8_t numSurbs)
{
    return runSync(this, __func__, [&]() {
        return mixDialWithReply(peerId, multiaddr, proto, expectReply, numSurbs);
    });
}

Libp2pResult Libp2pModulePlugin::syncMixRegisterDestReadBehavior(
    const QString &proto,
    int behavior,
    uint32_t sizeParam)
{
    return runSync(this, __func__, [&]() {
        return mixRegisterDestReadBehavior(proto, behavior, sizeParam);
    });
}

Libp2pResult Libp2pModulePlugin::syncMixSetNodeInfo(
    const QString &multiaddr,
    const QByteArray &mixPrivKey)
{
    return runSync(this, __func__, [&]() {
        return mixSetNodeInfo(multiaddr, mixPrivKey);
    });
}

Libp2pResult Libp2pModulePlugin::syncMixNodepoolAdd(
    const QString &peerId,
    const QString &multiaddr,
    const QByteArray &mixPubKey,
    const QByteArray &libp2pPubKey)
{
    return runSync(this, __func__, [&]() {
        return mixNodepoolAdd(peerId, multiaddr, mixPubKey, libp2pPubKey);
    });
}

