#include "libp2p_module_plugin.h"
#include <QEventLoop>
#include <QMetaObject>
#include <QTimer>

bool Libp2pModulePlugin::waitForUuid(
    const QString &uuid,
    const QString &caller
)
{
    if (uuid.isEmpty())
        return false;

    QEventLoop loop;
    bool success = false;

    QMetaObject::Connection conn;

    conn = connect(
        this,
        &Libp2pModulePlugin::libp2pEvent,
        this,
        [&](int result,
            const QString &reqId,
            const QString &eventCaller,
            const QString &,
            const QVariant &) {

            if (reqId == uuid && eventCaller == caller) {
                success = (result == RET_OK);
                disconnect(conn);
                loop.quit();
            }
        });

    loop.exec();
    return success;
}

bool Libp2pModulePlugin::syncLibp2pStart()
{
    QString uuid = libp2pStart();
    return waitForUuid(uuid, "libp2pStart");
}

bool Libp2pModulePlugin::syncLibp2pStop()
{
    QString uuid = libp2pStop();
    return waitForUuid(uuid, "libp2pStop");
}

/* --------------- Kademlia --------------- */

// bool Libp2pModulePlugin::syncToCid(const QByteArray &key, const QByteArray &value)
// {
//     QString uuid = toCid(key);
//     return waitForUuid(uuid, "toCid");
// }

bool Libp2pModulePlugin::syncKadPutValue(const QByteArray &key, const QByteArray &value)
{
    QString uuid = kadPutValue(key, value);
    return waitForUuid(uuid, "kadPutValue");
}

bool Libp2pModulePlugin::syncKadGetValue(const QByteArray &key, int quorum)
{
    QString uuid = kadGetValue(key, quorum);
    return waitForUuid(uuid, "kadGetValue");
}

// bool Libp2pModulePlugin::syncKadAddProvider(const QString &cid)
// {
//     QString uuid = kadAddProvider(cid);
//     return waitForUuid(uuid, "kadAddProvider");
// }

// bool Libp2pModulePlugin::syncKadGetProviders(const QString &cid)
// {
//     QString uuid = kadGetProviders(cid);
//     return waitForUuid(uuid, "kadGetProviders");
// }

// bool Libp2pModulePlugin::syncKadStartProviding(const QString &cid)
// {
//     QString uuid = kadStartProviding(cid);
//     return waitForUuid(uuid, "kadStartProviding");
// }

// bool Libp2pModulePlugin::syncKadStopProviding(const QString &cid)
// {
//     QString uuid = kadStopProviding(cid);
//     return waitForUuid(uuid, "kadStopProviding");
// }

// bool Libp2pModulePlugin::syncKadGetRandomRecords()
// {
//     QString uuid = kadGetRandomRecords();
//     return waitForUuid(uuid, "kadGetRandomRecords");
// }

