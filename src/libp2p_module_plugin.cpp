#include "libp2p_module_plugin.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <cstring>
#include <QDebug>

void Libp2pModulePlugin::onLibp2pEventDefault(
    const QString &caller,
    int result,
    const QString &message,
    const QVariant &data
)
{
    qDebug() << "[" << caller << "]"
             << "ret:" << result
             << "msg:" << message;
}


void Libp2pModulePlugin::libp2pCallback(
    int callerRet,
    const char *msg,
    size_t len,
    void *userData
)
{
    auto *self = static_cast<Libp2pModulePlugin *>(userData);
    if (!self) return;

    QString message = QString::fromUtf8(msg, int(len));

    QMetaObject::invokeMethod(
        self,
        [self, callerRet, message]() {
            emit self->libp2pEvent(
                self->lastCaller,
                callerRet,
                message,
                QVariant()
            );
        },
        Qt::QueuedConnection
    );
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
    auto *self = static_cast<Libp2pModulePlugin *>(userData);
    if (!self) return;

    QString message = QString::fromUtf8(msg, int(len));

    // TODO: return peers

    QMetaObject::invokeMethod(
        self,
        [self, callerRet, message]() {
            emit self->libp2pEvent(
                self->lastCaller,
                callerRet,
                message,
                QVariant()
            );
        },
        Qt::QueuedConnection
    );
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
    auto *self = static_cast<Libp2pModulePlugin *>(userData);
    if (!self) return;

    QString message = QString::fromUtf8(msg, int(len));

    // TODO: return buffer

    QMetaObject::invokeMethod(
        self,
        [self, callerRet, message]() {
            emit self->libp2pEvent(
                self->lastCaller,
                callerRet,
                message,
                QVariant()
            );
        },
        Qt::QueuedConnection
    );
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
    auto *self = static_cast<Libp2pModulePlugin *>(userData);
    if (!self) return;

    QString message = QString::fromUtf8(msg, int(len));

    // TODO: return providers

    QMetaObject::invokeMethod(
        self,
        [self, callerRet, message]() {
            emit self->libp2pEvent(
                self->lastCaller,
                callerRet,
                message,
                QVariant()
            );
        },
        Qt::QueuedConnection
    );
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

    lastCaller = "libp2pNew";
    ctx = libp2p_new(&config, &Libp2pModulePlugin::libp2pCallback, this);

    // register default event handler
    connect(this,
        &Libp2pModulePlugin::libp2pEvent,
        this,
        &Libp2pModulePlugin::onLibp2pEventDefault);

}

Libp2pModulePlugin::~Libp2pModulePlugin()
{
    if (ctx) {
        libp2p_destroy(ctx, &Libp2pModulePlugin::libp2pCallback, this);
        ctx = nullptr;
    }

    // Clean up resources
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

    // Create event data with the bar parameter
    QVariantList eventData;
    eventData << bar; // Add the bar parameter to the event data
    eventData << QDateTime::currentDateTime().toString(Qt::ISODate); // Add timestamp

    // Trigger the event using LogosAPI client (like chat module does)
    if (logosAPI) {
        // print triggering signal
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

    lastCaller = "libp2pStart";
    return libp2p_start(ctx, &Libp2pModulePlugin::libp2pCallback, this) == RET_OK;
}

bool Libp2pModulePlugin::libp2pStop()
{
    qDebug() << "Libp2pModulePlugin::libp2pStop called";
    if (!ctx) {
        qDebug() << "libp2pStop called without a context";
        return false;
    }

    lastCaller = "libp2pStop";
    return libp2p_stop(ctx, &Libp2pModulePlugin::libp2pCallback, this) == RET_OK;
}

bool Libp2pModulePlugin::findNode(const QString &peerId)
{
    qDebug() << "Libp2pModulePlugin::findNode called:" << peerId;
    if (!ctx) {
        qDebug() << "findNode called without a context";
        return false;
    }

    lastCaller = "findNode";
    return libp2p_find_node(
        ctx,
        peerId.toUtf8().constData(),
        &Libp2pModulePlugin::peersCallback,
        this
    ) == RET_OK;
}

bool Libp2pModulePlugin::putValue(const QByteArray &key, const QByteArray &value)
{
    qDebug() << "Libp2pModulePlugin::putValue called";
    if (!ctx) {
        qDebug() << "putValue called without a context";
        return false;
    }

    lastCaller = "putValue";
    return libp2p_put_value(
        ctx,
        reinterpret_cast<const uint8_t *>(key.constData()),
        key.size(),
        reinterpret_cast<const uint8_t *>(value.constData()),
        value.size(),
        &Libp2pModulePlugin::libp2pCallback,
        this
    ) == RET_OK;
}

bool Libp2pModulePlugin::getValue(const QByteArray &key, int quorum)
{
    qDebug() << "Libp2pModulePlugin::getValue called";
    if (!ctx) {
        qDebug() << "getValue called without a context";
        return false;
    }

    lastCaller = "getValue";
    return libp2p_get_value(
        ctx,
        reinterpret_cast<const uint8_t *>(key.constData()),
        key.size(),
        quorum,
        &Libp2pModulePlugin::libp2pBufferCallback,
        this
    ) == RET_OK;
}

bool Libp2pModulePlugin::addProvider(const QString &cid)
{
    qDebug() << "Libp2pModulePlugin::addProvider called:" << cid;
    if (!ctx) {
        qDebug() << "addProvider called without a context";
        return false;
    }

    lastCaller = "addProvider";
    return libp2p_add_provider(
        ctx,
        cid.toUtf8().constData(),
        &Libp2pModulePlugin::libp2pCallback,
        this
    ) == RET_OK;
}

bool Libp2pModulePlugin::startProviding(const QString &cid)
{
    qDebug() << "Libp2pModulePlugin::startProviding called:" << cid;
    if (!ctx) {
        qDebug() << "startProviding called without a context";
        return false;
    }

    lastCaller = "startProviding";
    return libp2p_start_providing(
        ctx,
        cid.toUtf8().constData(),
        &Libp2pModulePlugin::libp2pCallback,
        this
    ) == RET_OK;
}

bool Libp2pModulePlugin::stopProviding(const QString &cid)
{
    qDebug() << "Libp2pModulePlugin::stopProviding called:" << cid;
    if (!ctx) {
        qDebug() << "stopProviding called without a context";
        return false;
    }

    lastCaller = "stopProviding";
    return libp2p_stop_providing(
        ctx,
        cid.toUtf8().constData(),
        &Libp2pModulePlugin::libp2pCallback,
        this
    ) == RET_OK;
}

bool Libp2pModulePlugin::getProviders(const QString &cid)
{
    qDebug() << "Libp2pModulePlugin::getProviders called:" << cid;
    if (!ctx) {
        qDebug() << "getProviders called without a context";
        return false;
    }

    lastCaller = "getProviders";
    return libp2p_get_providers(
        ctx,
        cid.toUtf8().constData(),
        &Libp2pModulePlugin::getProvidersCallback,
        this
    ) == RET_OK;
}

bool Libp2pModulePlugin::setEventCallback()
{
    if (!ctx) {
        return false;
    }

    lastCaller = "libp2pSetEventCallback";
    libp2p_set_event_callback(ctx, &Libp2pModulePlugin::libp2pCallback, this);
    return true;
}


