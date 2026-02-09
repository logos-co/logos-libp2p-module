#include "libp2p_module_plugin.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <cstring>
#include <QDebug>

void Libp2pModulePlugin::onLibp2pEventDefault(
    int result,
    const QString &reqId,
    const QString &caller,
    const QString &message,
    const QVariant &data
)
{
    QByteArray buffer = data.toByteArray();

    qDebug() << "[" << caller << "]"
             << "reqId:" << reqId
             << "ret:" << result
             << "msg:" << message
             << "data size:" << buffer.size();
}


void Libp2pModulePlugin::libp2pCallback(
    int callerRet,
    const char *msg,
    size_t len,
    void *userData
)
{
    auto *callbackCtx = static_cast<CallbackContext *>(userData);
    if (!callbackCtx) return;

    Libp2pModulePlugin *self = callbackCtx->instance;
    if (!self) { delete callbackCtx; return; }

    QString caller = callbackCtx->caller;
    QString reqId = callbackCtx->reqId;

    QString message;
    if (msg && len > 0)
        message = QString::fromUtf8(msg, int(len));

    QPointer<Libp2pModulePlugin> safeSelf(self);
    QMetaObject::invokeMethod(
        safeSelf,
        [safeSelf, callerRet, message, caller, reqId]() {
            if (!safeSelf) return;
            emit safeSelf->libp2pEvent(
                callerRet,
                reqId,
                caller,
                message,
                QVariant()
            );
        },
        Qt::QueuedConnection
    );

    delete callbackCtx;
}

QList<ServiceInfo> copyServiceInfos(
    const Libp2pServiceInfo *services,
    size_t servicesLen
)
{
    QList<ServiceInfo> servicesCopy;

    if (!services || servicesLen == 0) {
        return servicesCopy;
    }

    servicesCopy.reserve(servicesLen);

    for (size_t i = 0; i < servicesLen; ++i) {
        ServiceInfo copy;

        if (services[i].id) {
            copy.id = services[i].id;
        }

        if (services[i].data && services[i].dataLen > 0) {
            copy.data = QByteArray(
                reinterpret_cast<const char *>(services[i].data),
                int(services[i].dataLen)
            );
        }

        servicesCopy.append(std::move(copy));
    }

    return servicesCopy;
}

ExtendedPeerRecord copyExtendedPeerRecord(
    const Libp2pExtendedPeerRecord &record
)
{
    ExtendedPeerRecord copy;

    if (record.peerId) {
        copy.peerId = record.peerId;
    }

    copy.seqNo = record.seqNo;

    /* ---- Addresses ---- */
    if (record.addrs && record.addrsLen > 0) {
        copy.addrs.reserve(record.addrsLen);

        for (size_t i = 0; i < record.addrsLen; ++i) {
            const char *addr = record.addrs[i];
            if (addr) {
                copy.addrs.append(addr);
            }
        }
    }

    copy.services = copyServiceInfos(record.services, record.servicesLen);

    return copy;
}

QList<ExtendedPeerRecord> copyExtendedRecords(
    const Libp2pExtendedPeerRecord *records,
    size_t recordsLen
)
{
    QList<ExtendedPeerRecord> recordsCopy;

    if (!records || recordsLen == 0) {
        return recordsCopy;
    }

    recordsCopy.reserve(recordsLen);

    for (size_t i = 0; i < recordsLen; ++i) {
        recordsCopy.append(copyExtendedPeerRecord(records[i]));
    }

    return recordsCopy;
}



void Libp2pModulePlugin::randomRecordsCallback(
    int callerRet,
    const Libp2pExtendedPeerRecord *records,
    size_t recordsLen,
    const char *msg,
    size_t len,
    void *userData
)
{
    auto *callbackCtx = static_cast<CallbackContext *>(userData);
    if (!callbackCtx) return;

    Libp2pModulePlugin *self = callbackCtx->instance;
    if (!self) { delete callbackCtx; return; }

    QString caller = callbackCtx->caller;
    QString reqId = callbackCtx->reqId;

    QList<ExtendedPeerRecord> recordsCopy = copyExtendedRecords(records, recordsLen);

    QString message;
    if (msg && len > 0)
        message = QString::fromUtf8(msg, int(len));

    QPointer<Libp2pModulePlugin> safeSelf(self);
    QMetaObject::invokeMethod(
        safeSelf,
        [safeSelf, callerRet, reqId, caller, message,
         recordsCopy = std::move(recordsCopy)]() { // avoid copying providers again
            if (!safeSelf) return;
            emit safeSelf->libp2pEvent(
                callerRet,
                reqId,
                caller,
                message,
                QVariant::fromValue(recordsCopy)
            );
        },
        Qt::QueuedConnection
    );

    delete callbackCtx;
}

void Libp2pModulePlugin::peersCallback(
    int callerRet,
    const char **peerIds,
    size_t peerIdsLen,
    const char *msg,
    size_t len,
    void *userData
)
{
    auto *callbackCtx = static_cast<CallbackContext *>(userData);
    if (!callbackCtx) return;

    Libp2pModulePlugin *self = callbackCtx->instance;
    if (!self) { delete callbackCtx; return; }

    QString caller = callbackCtx->caller;
    QString reqId = callbackCtx->reqId;

    QString message;
    if (msg && len > 0)
        message = QString::fromUtf8(msg, int(len));

    QPointer<Libp2pModulePlugin> safeSelf(self);
    QMetaObject::invokeMethod(
        safeSelf,
        [safeSelf, callerRet, message, caller, reqId]() {
            if (!safeSelf) return;
            emit safeSelf->libp2pEvent(
                callerRet,
                reqId,
                caller,
                message,
                QVariant()
            );
        },
        Qt::QueuedConnection
    );

    delete callbackCtx;
}

void Libp2pModulePlugin::libp2pBufferCallback(
    int callerRet,
    const uint8_t *data,
    size_t dataLen,
    const char *msg,
    size_t len,
    void *userData
)
{
    auto *callbackCtx = static_cast<CallbackContext *>(userData);
    if (!callbackCtx) return;

    Libp2pModulePlugin *self = callbackCtx->instance;
    if (!self) { delete callbackCtx; return; }

    QString caller = callbackCtx->caller;
    QString reqId = callbackCtx->reqId;

    QByteArray buffer;
    if (data && dataLen > 0)
        buffer = QByteArray(reinterpret_cast<const char *>(data), int(dataLen));

    QString message;
    if (msg && len > 0)
        message = QString::fromUtf8(msg, int(len));

    QPointer<Libp2pModulePlugin> safeSelf(self);
    QMetaObject::invokeMethod(safeSelf, [safeSelf, callerRet, message, buffer, caller, reqId]() {
        if (!safeSelf) return;
        emit safeSelf->libp2pEvent(callerRet, reqId, caller, message, QVariant(buffer));
    }, Qt::QueuedConnection);

    delete callbackCtx;
}

QList<PeerInfo> copyProviders(
    const Libp2pPeerInfo *providers,
    size_t providersLen
)
{
    QList<PeerInfo> providersCopy;

    if (!providers || providersLen == 0) {
        return providersCopy;
    }

    providersCopy.reserve(providersLen);

    for (size_t i = 0; i < providersLen; ++i) {
        PeerInfo copy;

        if (providers[i].peerId)
            copy.peerId = providers[i].peerId;

        for (size_t j = 0; j < providers[i].addrsLen; ++j) {
            const char* addr = providers[i].addrs[j];
            if (addr)
                copy.addrs.append(addr);
        }

        providersCopy.append(std::move(copy));
    }

    return providersCopy;
}

void Libp2pModulePlugin::getProvidersCallback(
    int callerRet,
    const Libp2pPeerInfo *providers,
    size_t providersLen,
    const char *msg,
    size_t len,
    void *userData
)
{
    auto *callbackCtx = static_cast<CallbackContext *>(userData);
    if (!callbackCtx) return;

    Libp2pModulePlugin *self = callbackCtx->instance;
    if (!self) { delete callbackCtx; return; }

    QString caller = callbackCtx->caller;
    QString reqId = callbackCtx->reqId;

    QList<PeerInfo> providersCopy = copyProviders(providers, providersLen);

    QString message;
    if (msg && len > 0)
        message = QString::fromUtf8(msg, int(len));

    QPointer<Libp2pModulePlugin> safeSelf(self);
    QMetaObject::invokeMethod(
        safeSelf,
        [safeSelf, callerRet, reqId, caller, message,
         providersCopy = std::move(providersCopy)]() { // avoid copying providers again
            if (!safeSelf) return;
            emit safeSelf->libp2pEvent(
                callerRet,
                reqId,
                caller,
                message,
                QVariant::fromValue(providersCopy)
            );
        },
        Qt::QueuedConnection
    );

    delete callbackCtx;
}

/* -------------------- End Callbacks -------------------- */

