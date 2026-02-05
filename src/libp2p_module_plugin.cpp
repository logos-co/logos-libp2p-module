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

    QPointer<Libp2pModulePlugin> safeSelf(self);
    if (caller == "getValue") {
        QMetaObject::invokeMethod(safeSelf, [safeSelf, callerRet, buffer, reqId]() {
            if (!safeSelf) return;
            emit safeSelf->getValueFinished(callerRet, reqId, buffer);
        }, Qt::QueuedConnection);
    } else {
        QMetaObject::invokeMethod(safeSelf, [safeSelf, callerRet, buffer, caller, reqId]() {
            if (!safeSelf) return;
            emit safeSelf->libp2pEvent(callerRet, reqId, caller, QString(), QVariant(buffer));
        }, Qt::QueuedConnection);
    }

    delete callbackCtx;
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

Libp2pModulePlugin::Libp2pModulePlugin()
    : ctx(nullptr)
{
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

bool Libp2pModulePlugin::findNode(const QString &peerId)
{
    qDebug() << "Libp2pModulePlugin::findNode called:" << peerId;
    if (!ctx) {
        qDebug() << "findNode called without a context";
        return false;
    }

    auto *callbackCtx = new CallbackContext{ "findNode", QUuid::createUuid().toString(), this };

    int ret = libp2p_find_node(
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

bool Libp2pModulePlugin::putValue(const QByteArray &key, const QByteArray &value)
{
    qDebug() << "Libp2pModulePlugin::putValue called";
    if (!ctx) {
        qDebug() << "putValue called without a context";
        return false;
    }

    auto *callbackCtx = new CallbackContext{ "putValue", QUuid::createUuid().toString(), this };

    int ret = libp2p_put_value(
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

bool Libp2pModulePlugin::getValue(const QByteArray &key, int quorum)
{
    qDebug() << "Libp2pModulePlugin::getValue called";
    if (!ctx) {
        qDebug() << "getValue called without a context";
        return false;
    }

    auto *callbackCtx = new CallbackContext{ "getValue", QUuid::createUuid().toString(), this };

    int ret = libp2p_get_value(
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

bool Libp2pModulePlugin::addProvider(const QString &cid)
{
    qDebug() << "Libp2pModulePlugin::addProvider called:" << cid;
    if (!ctx) {
        qDebug() << "addProvider called without a context";
        return false;
    }

    auto *callbackCtx = new CallbackContext{ "addProvider", QUuid::createUuid().toString(), this };

    int ret = libp2p_add_provider(
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

bool Libp2pModulePlugin::startProviding(const QString &cid)
{
    qDebug() << "Libp2pModulePlugin::startProviding called:" << cid;
    if (!ctx) {
        qDebug() << "startProviding called without a context";
        return false;
    }

    auto *callbackCtx = new CallbackContext{ "startProviding", QUuid::createUuid().toString(), this };

    int ret = libp2p_start_providing(
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

bool Libp2pModulePlugin::stopProviding(const QString &cid)
{
    qDebug() << "Libp2pModulePlugin::stopProviding called:" << cid;
    if (!ctx) {
        qDebug() << "stopProviding called without a context";
        return false;
    }

    auto *callbackCtx = new CallbackContext{ "stopProviding", QUuid::createUuid().toString(), this };

    int ret = libp2p_stop_providing(
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

bool Libp2pModulePlugin::getProviders(const QString &cid)
{
    qDebug() << "Libp2pModulePlugin::getProviders called:" << cid;
    if (!ctx) {
        qDebug() << "getProviders called without a context";
        return false;
    }

    auto *callbackCtx = new CallbackContext{ "getProviders", QUuid::createUuid().toString(), this };

    int ret = libp2p_get_providers(
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

bool Libp2pModulePlugin::setEventCallback()
{
    if (!ctx) {
        return false;
    }

    libp2p_set_event_callback(ctx, &Libp2pModulePlugin::libp2pCallback, NULL);
    return true;
}

