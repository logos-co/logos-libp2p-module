#include "libp2p_module_plugin.h"
#include <QEventLoop>
#include <QMetaObject>
#include <QTimer>

WaitResult Libp2pModulePlugin::waitForUuid(
    const QString &uuid,
    const QString &caller
)
{
    WaitResult result{false, QVariant{}};
    if (uuid.isEmpty()) return result;

    QEventLoop loop;
    QMetaObject::Connection conn;

    conn = connect(
        this,
        &Libp2pModulePlugin::libp2pEvent,
        this,
        [&](int ret, const QString &reqId, const QString &eventCaller,
            const QString &message, const QVariant &data) {

            if (reqId == uuid && eventCaller == caller) {
                result.ok = (ret == RET_OK);
                result.data = data;
                if (eventCaller == "toCid")
                    result.data = message;
                disconnect(conn);
                loop.quit();
            }
        });

    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(10000); // 10-second safety timeout

    loop.exec();
    disconnect(conn);
    return result;
}

bool Libp2pModulePlugin::syncLibp2pStart()
{
    QString uuid = libp2pStart();
    return waitForUuid(uuid, "libp2pStart").ok;
}

bool Libp2pModulePlugin::syncLibp2pStop()
{
    QString uuid = libp2pStop();
    return waitForUuid(uuid, "libp2pStop").ok;
}

QList<QString> Libp2pModulePlugin::syncKadFindNode(const QString &peerId)
{
    QString uuid = kadFindNode(peerId);
    WaitResult res = waitForUuid(uuid, "kadFindNode");
    return res.ok ? res.data.value<QList<QString>>() : QList<QString>();
}

bool Libp2pModulePlugin::syncKadPutValue(const QByteArray &key, const QByteArray &value)
{
    QString uuid = kadPutValue(key, value);
    return waitForUuid(uuid, "kadPutValue").ok;
}

QByteArray Libp2pModulePlugin::syncKadGetValue(const QByteArray &key, int quorum)
{
    QString uuid = kadGetValue(key, quorum);
    WaitResult res = waitForUuid(uuid, "kadGetValue");
    return res.ok ? res.data.toByteArray() : QByteArray{};
}

bool Libp2pModulePlugin::syncKadAddProvider(const QString &cid)
{
    QString uuid = kadAddProvider(cid);
    return waitForUuid(uuid, "kadAddProvider").ok;
}

QList<PeerInfo> Libp2pModulePlugin::syncKadGetProviders(const QString &cid)
{
    QString uuid = kadGetProviders(cid);
    WaitResult res = waitForUuid(uuid, "kadGetProviders");
    return res.ok ? res.data.value<QList<PeerInfo>>() : QList<PeerInfo>();
}

bool Libp2pModulePlugin::syncKadStartProviding(const QString &cid)
{
    QString uuid = kadStartProviding(cid);
    return waitForUuid(uuid, "kadStartProviding").ok;
}

bool Libp2pModulePlugin::syncKadStopProviding(const QString &cid)
{
    QString uuid = kadStopProviding(cid);
    return waitForUuid(uuid, "kadStopProviding").ok;
}

QList<ExtendedPeerRecord> Libp2pModulePlugin::syncKadGetRandomRecords()
{
    QString uuid = kadGetRandomRecords();
    WaitResult res = waitForUuid(uuid, "kadGetRandomRecords");
    return res.ok ? res.data.value<QList<ExtendedPeerRecord>>() : QList<ExtendedPeerRecord>();
}

QString Libp2pModulePlugin::syncToCid(const QByteArray &key)
{
    QString uuid = toCid(key);
    WaitResult res = waitForUuid(uuid, "toCid");
    return res.ok ? res.data.toString() : QString{};
}

