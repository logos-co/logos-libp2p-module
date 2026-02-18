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

QList<QString> copyPeerIds(
    const char **peerIds,
    size_t peerIdsLen
)
{
    QList<QString> peerIdsCopy;

    if (!peerIds || peerIdsLen == 0) {
        return peerIdsCopy;
    }

    peerIdsCopy.reserve(peerIdsLen);

    for (size_t i = 0; i < peerIdsLen; ++i) {
        peerIdsCopy.append(QString::fromUtf8(peerIds[i]));
    }

    return peerIdsCopy;
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

    QList<QString> peerIdsCopy = copyPeerIds(peerIds, peerIdsLen);

    QString message;
    if (msg && len > 0)
        message = QString::fromUtf8(msg, int(len));

    QPointer<Libp2pModulePlugin> safeSelf(self);
    QMetaObject::invokeMethod(
        safeSelf,
        [safeSelf, callerRet, message, caller, reqId,
        peerIdsCopy = std::move(peerIdsCopy)]() { // avoid copying again
            if (!safeSelf) return;
            emit safeSelf->libp2pEvent(
                callerRet,
                reqId,
                caller,
                message,
                QVariant::fromValue(peerIdsCopy)
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

QList<PeerInfo> copyPeerInfos(
    const Libp2pPeerInfo *peerInfos,
    size_t peerInfosLen
)
{
    QList<PeerInfo> peerInfosCopy;

    if (!peerInfos || peerInfosLen == 0) {
        return peerInfosCopy;
    }

    peerInfosCopy.reserve(peerInfosLen);

    for (size_t i = 0; i < peerInfosLen; ++i) {
        PeerInfo copy;

        if (peerInfos[i].peerId)
            copy.peerId = peerInfos[i].peerId;

        if (!peerInfos[i].addrs){
            continue;
        }
        for (size_t j = 0; j < peerInfos[i].addrsLen; ++j) {
            const char* addr = peerInfos[i].addrs[j];
            if (addr)
                copy.addrs.append(addr);
        }

        peerInfosCopy.append(std::move(copy));
    }

    return peerInfosCopy;
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

    QList<PeerInfo> providersCopy = copyPeerInfos(providers, providersLen);

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

void Libp2pModulePlugin::peerInfoCallback(
    int callerRet,
    const Libp2pPeerInfo *info,
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

    PeerInfo copy;
    if (info) {
        auto copies = copyPeerInfos(info, 1);
        if (!copies.empty()) {
            copy = std::move(copies[0]);
        }
    }

    QString message;
    if (msg && len > 0)
        message = QString::fromUtf8(msg, int(len));

    QPointer<Libp2pModulePlugin> safeSelf(self);
    QMetaObject::invokeMethod(
        safeSelf,
        [safeSelf, callerRet, reqId, caller, message,
         copy = std::move(copy)]() {
            if (!safeSelf) return;

            emit safeSelf->libp2pEvent(
                callerRet,
                reqId,
                caller,
                message,
                QVariant::fromValue(copy)
            );
        },
        Qt::QueuedConnection
    );

    delete callbackCtx;
}

void Libp2pModulePlugin::connectionCallback(
    int callerRet,
    libp2p_stream_t *stream,
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

    QVariant streamIdVariant;
    if (callerRet == RET_OK && stream) {
        uint64_t streamId = self->addStream(stream);
        streamIdVariant = QVariant::fromValue<qulonglong>(streamId);
    }

    QString message;
    if (msg && len > 0)
        message = QString::fromUtf8(msg, int(len));

    QPointer<Libp2pModulePlugin> safeSelf(self);
    QMetaObject::invokeMethod(
        safeSelf,
        [safeSelf, callerRet, reqId, caller, message, streamIdVariant]() {
            if (!safeSelf) return;

            emit safeSelf->libp2pEvent(
                callerRet,
                reqId,
                caller,
                message,
                streamIdVariant
            );
        },
        Qt::QueuedConnection
    );

    delete callbackCtx;
}




