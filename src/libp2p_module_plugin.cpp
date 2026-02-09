#include "libp2p_module_plugin.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <cstring>
#include <QDebug>

struct CallbackContext {
    QString caller;
    QString reqId;
    Libp2pModulePlugin *instance;
};

struct PeerInfo {
    QString peerId;
    QList<QString> addrs;
};

struct ServiceInfo{
    QString id;
    QByteArray data;
};

struct ExtendedPeerRecord{
    QString peerId;
    uint64_t seqNo;
    QList<QString> addrs;
    QList<ServiceInfo> services;
};


/* -------------------- Callbacks -------------------- */

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

Libp2pModulePlugin::Libp2pModulePlugin()
    : ctx(nullptr)
{
    qRegisterMetaType<PeerInfo>("PeerInfo");
    qRegisterMetaType<ServiceInfo>("ServiceInfo");
    qRegisterMetaType<ExtendedPeerRecord>("ExtendedPeerRecord");

    /* ---- QList containers ---- */
    qRegisterMetaType<QList<PeerInfo>>("QList<PeerInfo>");
    qRegisterMetaType<QList<ServiceInfo>>("QList<ServiceInfo>");
    qRegisterMetaType<QList<ExtendedPeerRecord>>("QList<ExtendedPeerRecord>");

    std::memset(&config, 0, sizeof(config));

    config.flags |= LIBP2P_CFG_GOSSIPSUB;
    config.mount_gossipsub = 1;

    config.flags |= LIBP2P_CFG_GOSSIPSUB_TRIGGER_SELF;
    config.gossipsub_trigger_self = 1;

    config.flags |= LIBP2P_CFG_KAD;
    config.mount_kad = 1;

    auto *callbackCtx = new CallbackContext{ "libp2pNew", QUuid::createUuid().toString(), this };

    ctx = libp2p_new(&config, &Libp2pModulePlugin::libp2pCallback, callbackCtx);

    connect(this,
        &Libp2pModulePlugin::libp2pEvent,
        this,
        &Libp2pModulePlugin::onLibp2pEventDefault);
}

Libp2pModulePlugin::~Libp2pModulePlugin()
{
    if (ctx) {
        auto *callbackCtx = new CallbackContext{ "libp2pDestroy", QUuid::createUuid().toString(), this };
        libp2p_destroy(ctx, &Libp2pModulePlugin::libp2pCallback, callbackCtx);
        ctx = nullptr;
    }

    if (logosAPI) {
        delete logosAPI;
        logosAPI = nullptr;
    }
}

void Libp2pModulePlugin::initLogos(LogosAPI* logosAPIInstance) {
    if (logosAPI) {
        delete logosAPI;
    }
    logosAPI = logosAPIInstance;
}

/* ---------------- Helper Functions ----------------- */

bool Libp2pModulePlugin::toCid(const QByteArray &key)
{
    if (key.isEmpty())
        return {};

    auto *callbackCtx = new CallbackContext{ "toCid", QUuid::createUuid().toString(), this };

    int ret = libp2p_create_cid(
        1,                      // CIDv1
        "dag-pb",
        "sha2-256",
        reinterpret_cast<const uint8_t *>(key.constData()),
        size_t(key.size()),
        &Libp2pModulePlugin::libp2pCallback,
        callbackCtx
    );

    if (ret != RET_OK) {
        delete callbackCtx;
    }

    return ret == RET_OK;
}

bool Libp2pModulePlugin::foo(const QString &bar)
{
    qDebug() << "Libp2pModulePlugin::foo called with:" << bar;

    QVariantList eventData;
    eventData << bar;
    eventData << QDateTime::currentDateTime().toString(Qt::ISODate);

    if (logosAPI) {
        qDebug() << "Libp2pModulePlugin: Triggering event 'fooTriggered' with data:" << eventData;
        logosAPI->getClient("core_manager")->onEventResponse(this, "fooTriggered", eventData);
        qDebug() << "Libp2pModulePlugin: Event 'fooTriggered' triggered with data:" << eventData;
    } else {
        qWarning() << "Libp2pModulePlugin: LogosAPI not available, cannot trigger event";
    }

    return true;
}

/* --------------- Start/stop --------------- */

bool Libp2pModulePlugin::libp2pStart()
{
    qDebug() << "Libp2pModulePlugin::libp2pStart called";
    if (!ctx) {
        qDebug() << "libp2pStart called without a context";
        return false;
    }

    auto *callbackCtx = new CallbackContext{ "libp2pStart", QUuid::createUuid().toString(), this };

    int ret = libp2p_start(ctx, &Libp2pModulePlugin::libp2pCallback, callbackCtx);

    if (ret != RET_OK) {
        delete callbackCtx;
    }

    return ret == RET_OK;
}

bool Libp2pModulePlugin::libp2pStop()
{
    qDebug() << "Libp2pModulePlugin::libp2pStop called";
    if (!ctx) {
        qDebug() << "libp2pStop called without a context";
        return false;
    }

    auto *callbackCtx = new CallbackContext{ "libp2pStop", QUuid::createUuid().toString(), this };

    int ret = libp2p_stop(ctx, &Libp2pModulePlugin::libp2pCallback, callbackCtx);

    if (ret != RET_OK) {
        delete callbackCtx;
    }

    return ret == RET_OK;
}

/* --------------- Kademlia --------------- */

bool Libp2pModulePlugin::kadFindNode(const QString &peerId)
{
    qDebug() << "Libp2pModulePlugin::kadFindNode called:" << peerId;
    if (!ctx) {
        qDebug() << "kadFindNode called without a context";
        return false;
    }

    auto *callbackCtx = new CallbackContext{ "kadFindNode", QUuid::createUuid().toString(), this };

    int ret = libp2p_kad_find_node(
        ctx,
        peerId.toUtf8().constData(),
        &Libp2pModulePlugin::peersCallback,
        callbackCtx
    );

    if (ret != RET_OK) {
        delete callbackCtx;
    }

    return ret == RET_OK;
}

bool Libp2pModulePlugin::kadPutValue(const QByteArray &key, const QByteArray &value)
{
    qDebug() << "Libp2pModulePlugin::kadPutValue called";
    if (!ctx) {
        qDebug() << "kadPutValue called without a context";
        return false;
    }

    auto *callbackCtx = new CallbackContext{ "kadPutValue", QUuid::createUuid().toString(), this };

    int ret = libp2p_kad_put_value(
        ctx,
        reinterpret_cast<const uint8_t *>(key.constData()),
        key.size(),
        reinterpret_cast<const uint8_t *>(value.constData()),
        value.size(),
        &Libp2pModulePlugin::libp2pCallback,
        callbackCtx
    );

    if (ret != RET_OK) {
        delete callbackCtx;
    }

    return ret == RET_OK;
}

bool Libp2pModulePlugin::kadGetValue(const QByteArray &key, int quorum)
{
    qDebug() << "Libp2pModulePlugin::kadGetValue called";
    if (!ctx) {
        qDebug() << "kadGetValue called without a context";
        return false;
    }

    auto *callbackCtx = new CallbackContext{ "kadGetValue", QUuid::createUuid().toString(), this };

    int ret = libp2p_kad_get_value(
        ctx,
        reinterpret_cast<const uint8_t *>(key.constData()),
        key.size(),
        quorum,
        &Libp2pModulePlugin::libp2pBufferCallback,
        callbackCtx
    );

    if (ret != RET_OK) {
        delete callbackCtx;
    }

    return ret == RET_OK;
}

bool Libp2pModulePlugin::kadAddProvider(const QString &cid)
{
    qDebug() << "Libp2pModulePlugin::kadAddProvider called:" << cid;
    if (!ctx) {
        qDebug() << "kadAddProvider called without a context";
        return false;
    }

    auto *callbackCtx = new CallbackContext{ "kadAddProvider", QUuid::createUuid().toString(), this };

    int ret = libp2p_kad_add_provider(
        ctx,
        cid.toUtf8().constData(),
        &Libp2pModulePlugin::libp2pCallback,
        callbackCtx
    );

    if (ret != RET_OK) {
        delete callbackCtx;
    }

    return ret == RET_OK;
}

bool Libp2pModulePlugin::kadGetProviders(const QString &cid)
{
    qDebug() << "Libp2pModulePlugin::kadGetProviders called:" << cid;
    if (!ctx) {
        qDebug() << "kadGetProviders called without a context";
        return false;
    }

    auto *callbackCtx = new CallbackContext{ "kadGetProviders", QUuid::createUuid().toString(), this };

    int ret = libp2p_kad_get_providers(
        ctx,
        cid.toUtf8().constData(),
        &Libp2pModulePlugin::getProvidersCallback,
        callbackCtx
    );

    if (ret != RET_OK) {
        delete callbackCtx;
    }

    return ret == RET_OK;
}

bool Libp2pModulePlugin::kadStartProviding(const QString &cid)
{
    qDebug() << "Libp2pModulePlugin::kadStartProviding called:" << cid;
    if (!ctx) {
        qDebug() << "kadStartProviding called without a context";
        return false;
    }

    auto *callbackCtx = new CallbackContext{ "kadStartProviding", QUuid::createUuid().toString(), this };

    int ret = libp2p_kad_start_providing(
        ctx,
        cid.toUtf8().constData(),
        &Libp2pModulePlugin::libp2pCallback,
        callbackCtx
    );

    if (ret != RET_OK) {
        delete callbackCtx;
    }

    return ret == RET_OK;
}

bool Libp2pModulePlugin::kadStopProviding(const QString &cid)
{
    qDebug() << "Libp2pModulePlugin::kadStopProviding called:" << cid;
    if (!ctx) {
        qDebug() << "kadStopProviding called without a context";
        return false;
    }

    auto *callbackCtx = new CallbackContext{ "kadStopProviding", QUuid::createUuid().toString(), this };

    int ret = libp2p_kad_stop_providing(
        ctx,
        cid.toUtf8().constData(),
        &Libp2pModulePlugin::libp2pCallback,
        callbackCtx
    );

    if (ret != RET_OK) {
        delete callbackCtx;
    }

    return ret == RET_OK;
}

bool Libp2pModulePlugin::kadGetRandomRecords()
{
    qDebug() << "Libp2pModulePlugin::kadGetRandomRecords called";

    if (!ctx) {
        qDebug() << "kadGetRandomRecords called without a context";
        return false;
    }

    auto *callbackCtx =
        new CallbackContext{ "kadGetRandomRecords", QUuid::createUuid().toString(), this };

    int ret = libp2p_kad_random_records(
        ctx,
        &Libp2pModulePlugin::randomRecordsCallback,
        callbackCtx
    );

    if (ret != RET_OK) {
        delete callbackCtx;
    }

    return ret == RET_OK;
}

bool Libp2pModulePlugin::setEventCallback()
{
    if (!ctx) {
        return false;
    }

    libp2p_set_event_callback(ctx, &Libp2pModulePlugin::libp2pCallback, NULL);
    return true;
}

