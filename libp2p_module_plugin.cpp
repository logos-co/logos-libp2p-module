#include "libp2p_module_plugin.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <cstring>

/* =========================
 * Generic callback
 * ========================= */

void Libp2pModulePlugin::genericCallback(
    int callerRet,
    const char *msg,
    size_t len,
    void *userData)
{
    auto *self = static_cast<Libp2pModulePlugin *>(userData);
    if (!self) return;

    QString message;
    if (msg && len > 0) {
        message = QString::fromUtf8(msg, static_cast<int>(len));
    }

    emit self->response(
        QStringLiteral("libp2p"),
        callerRet,
        message,
        QVariant()
    );
}

void Libp2pModulePlugin::bufferCallback(
    int callerRet,
    const uint8_t *data,
    size_t dataLen,
    const char *msg,
    size_t len,
    void *userData)
{
    auto *self = static_cast<Libp2pModulePlugin *>(userData);
    if (!self) return;

    QByteArray payload;
    if (data && dataLen > 0) {
        payload = QByteArray(
            reinterpret_cast<const char *>(data),
            static_cast<int>(dataLen)
        );
    }

    QString message;
    if (msg && len > 0) {
        message = QString::fromUtf8(msg, static_cast<int>(len));
    }

    emit self->response(
        QStringLiteral("libp2p"),
        callerRet,
        message,
        payload
    );
}

void Libp2pModulePlugin::connectionCallback(
    int callerRet,
    libp2p_stream_t *conn,
    const char *msg,
    size_t len,
    void *userData)
{
    auto *self = static_cast<Libp2pModulePlugin *>(userData);
    if (!self) return;

    quintptr handle = reinterpret_cast<quintptr>(conn);

    QString message;
    if (msg && len > 0) {
        message = QString::fromUtf8(msg, static_cast<int>(len));
    }

    emit self->response(
        QStringLiteral("libp2p"),
        callerRet,
        message,
        QVariant::fromValue(handle)
    );

    if (callerRet == RET_OK && conn) {
        emit self->streamOpened(handle);
    }
}

void Libp2pModulePlugin::getProvidersCallback(
    int callerRet,
    const Libp2pPeerInfo *providers,
    size_t providersLen,
    const char *msg,
    size_t len,
    void *userData)
{
    auto *self = static_cast<Libp2pModulePlugin *>(userData);
    if (!self) return;

    QVariantList result;

    for (size_t i = 0; i < providersLen; ++i) {
        QVariantMap peer;
        peer["peerId"] = QString::fromUtf8(providers[i].peerId);

        QStringList addrs;
        for (size_t j = 0; j < providers[i].addrsLen; ++j) {
            addrs << QString::fromUtf8(providers[i].addrs[j]);
        }
        peer["addrs"] = addrs;

        result << peer;
    }

    QString message;
    if (msg && len > 0) {
        message = QString::fromUtf8(msg, static_cast<int>(len));
    }

    emit self->response(
        QStringLiteral("libp2p"),
        callerRet,
        message,
        result
    );

    emit self->providersReceived(result);
}


void Libp2pModulePlugin::peersCallback(
    int callerRet,
    const char **peerIds,
    size_t peerIdsLen,
    const char *msg,
    size_t len,
    void *userData)
{
    auto *self = static_cast<Libp2pModulePlugin *>(userData);
    if (!self) return;

    QStringList peers;
    for (size_t i = 0; i < peerIdsLen; ++i) {
        if (peerIds[i]) {
            peers << QString::fromUtf8(peerIds[i]);
        }
    }

    QString message;
    if (msg && len > 0) {
        message = QString::fromUtf8(msg, static_cast<int>(len));
    }

    emit self->response(
        QStringLiteral("libp2p"),
        callerRet,
        message,
        peers
    );

    emit self->peersReceived(peers);
}



/* =========================
 * Lifecycle
 * ========================= */

Libp2pModulePlugin::Libp2pModulePlugin()
    : QObject(nullptr), ctx(nullptr)
{
}

Libp2pModulePlugin::~Libp2pModulePlugin()
{
    if (ctx) {
        libp2p_destroy(ctx, &Libp2pModulePlugin::genericCallback, this);
        ctx = nullptr;
    }
}

bool Libp2pModulePlugin::newContext(const QVariantMap &config)
{
    if (ctx) {
        return false;
    }

    static libp2p_config_t cfg;
    std::memset(&cfg, 0, sizeof(cfg));

    // ---- Minimal config mapping ----
    if (config.value("gossipsub").toBool()) {
        cfg.flags |= LIBP2P_CFG_GOSSIPSUB;
        cfg.mount_gossipsub = 1;
    }

    if (config.value("gossipsub_trigger_self").toBool()) {
        cfg.flags |= LIBP2P_CFG_GOSSIPSUB_TRIGGER_SELF;
        cfg.gossipsub_trigger_self = 1;
    }

    if (config.value("kad").toBool()) {
        cfg.flags |= LIBP2P_CFG_KAD;
        cfg.mount_kad = 1;
    }

    ctx = libp2p_new(&cfg, &Libp2pModulePlugin::genericCallback, this);
    return ctx != nullptr;
}

bool Libp2pModulePlugin::destroyContext()
{
    if (!ctx) {
        return false;
    }

    libp2p_destroy(ctx, &Libp2pModulePlugin::genericCallback, this);
    ctx = nullptr;
    return true;
}

bool Libp2pModulePlugin::start()
{
    if (!ctx) {
        return false;
    }

    return libp2p_start(
        ctx,
        &Libp2pModulePlugin::genericCallback,
        this
    ) == RET_OK;
}

bool Libp2pModulePlugin::stop()
{
    if (!ctx) {
        return false;
    }

    return libp2p_stop(
        ctx,
        &Libp2pModulePlugin::genericCallback,
        this
    ) == RET_OK;
}

bool Libp2pModulePlugin::setEventCallback()
{
    if (!ctx) {
        return false;
    }

    libp2p_set_event_callback(
        ctx,
        &Libp2pModulePlugin::genericCallback,
        this
    );

    return true;
}

/* =========================
 * Peers
 * ========================= */

bool Libp2pModulePlugin::connectPeer(
    const QString &peerId,
    const QStringList &multiaddrs,
    qint64 timeoutMs)
{
    if (!ctx) return false;

    std::vector<const char *> addrs;
    addrs.reserve(multiaddrs.size());
    for (const auto &a : multiaddrs) {
        addrs.push_back(a.toUtf8().constData());
    }

    return libp2p_connect(
        ctx,
        peerId.toUtf8().constData(),
        addrs.data(),
        addrs.size(),
        timeoutMs,
        &Libp2pModulePlugin::genericCallback,
        this
    ) == RET_OK;
}

bool Libp2pModulePlugin::disconnectPeer(const QString &peerId)
{
    if (!ctx) return false;

    return libp2p_disconnect(
        ctx,
        peerId.toUtf8().constData(),
        &Libp2pModulePlugin::genericCallback,
        this
    ) == RET_OK;
}

bool Libp2pModulePlugin::peerInfo()
{
    return ctx && libp2p_peerinfo(ctx, nullptr, this) == RET_OK;
}

bool Libp2pModulePlugin::connectedPeers(quint32 direction)
{
    return ctx && libp2p_connected_peers(
        ctx,
        static_cast<Direction>(direction),
        nullptr,
        this
    ) == RET_OK;
}

/* =========================
 * Streams
 * ========================= */

bool Libp2pModulePlugin::dial(
    const QString &peerId,
    const QString &protocol)
{
    if (!ctx) return false;

    return libp2p_dial(
        ctx,
        peerId.toUtf8().constData(),
        protocol.toUtf8().constData(),
        nullptr,
        this
    ) == RET_OK;
}

bool Libp2pModulePlugin::streamClose(quintptr streamHandle)
{
    return ctx && libp2p_stream_close(
        ctx,
        reinterpret_cast<libp2p_stream_t *>(streamHandle),
        &Libp2pModulePlugin::genericCallback,
        this
    ) == RET_OK;
}

bool Libp2pModulePlugin::streamCloseWithEOF(quintptr streamHandle)
{
    return ctx && libp2p_stream_closeWithEOF(
        ctx,
        reinterpret_cast<libp2p_stream_t *>(streamHandle),
        &Libp2pModulePlugin::genericCallback,
        this
    ) == RET_OK;
}

bool Libp2pModulePlugin::streamRelease(quintptr streamHandle)
{
    return ctx && libp2p_stream_release(
        ctx,
        reinterpret_cast<libp2p_stream_t *>(streamHandle),
        &Libp2pModulePlugin::genericCallback,
        this
    ) == RET_OK;
}

/* =========================
 * Pubsub / DHT
 * ========================= */

bool Libp2pModulePlugin::gossipsubPublish(
    const QString &topic,
    const QByteArray &data)
{
    return ctx && libp2p_gossipsub_publish(
        ctx,
        topic.toUtf8().constData(),
        reinterpret_cast<uint8_t *>(const_cast<char *>(data.constData())),
        data.size(),
        &Libp2pModulePlugin::genericCallback,
        this
    ) == RET_OK;
}

bool Libp2pModulePlugin::gossipsubSubscribe(const QString &topic)
{
    return ctx && libp2p_gossipsub_subscribe(
        ctx,
        topic.toUtf8().constData(),
        nullptr,
        &Libp2pModulePlugin::genericCallback,
        this
    ) == RET_OK;
}

bool Libp2pModulePlugin::gossipsubUnsubscribe(const QString &topic)
{
    return ctx && libp2p_gossipsub_unsubscribe(
        ctx,
        topic.toUtf8().constData(),
        nullptr,
        &Libp2pModulePlugin::genericCallback,
        this
    ) == RET_OK;
}
bool Libp2pModulePlugin::streamReadExactly(
    quintptr streamHandle,
    quint64 size)
{
    if (!ctx || !streamHandle) return false;

    return libp2p_stream_readExactly(
        ctx,
        reinterpret_cast<libp2p_stream_t *>(streamHandle),
        static_cast<size_t>(size),
        &Libp2pModulePlugin::bufferCallback,
        this
    ) == RET_OK;
}

bool Libp2pModulePlugin::streamReadLp(
    quintptr streamHandle,
    qint64 maxSize)
{
    if (!ctx || !streamHandle) return false;

    return libp2p_stream_readLp(
        ctx,
        reinterpret_cast<libp2p_stream_t *>(streamHandle),
        maxSize,
        &Libp2pModulePlugin::bufferCallback,
        this
    ) == RET_OK;
}

bool Libp2pModulePlugin::streamWrite(
    quintptr streamHandle,
    const QByteArray &data)
{
    if (!ctx || !streamHandle) return false;

    return libp2p_stream_write(
        ctx,
        reinterpret_cast<libp2p_stream_t *>(streamHandle),
        reinterpret_cast<uint8_t *>(const_cast<char *>(data.constData())),
        static_cast<size_t>(data.size()),
        &Libp2pModulePlugin::genericCallback,
        this
    ) == RET_OK;
}

bool Libp2pModulePlugin::streamWriteLp(
    quintptr streamHandle,
    const QByteArray &data)
{
    if (!ctx || !streamHandle) return false;

    return libp2p_stream_writeLp(
        ctx,
        reinterpret_cast<libp2p_stream_t *>(streamHandle),
        reinterpret_cast<uint8_t *>(const_cast<char *>(data.constData())),
        static_cast<size_t>(data.size()),
        &Libp2pModulePlugin::genericCallback,
        this
    ) == RET_OK;
}
bool Libp2pModulePlugin::findNode(const QString &peerId)
{
    if (!ctx) return false;

    return libp2p_find_node(
        ctx,
        peerId.toUtf8().constData(),
        &Libp2pModulePlugin::peersCallback,
        this
    ) == RET_OK;
}

bool Libp2pModulePlugin::putValue(
    const QByteArray &key,
    const QByteArray &value)
{
    if (!ctx) return false;

    return libp2p_put_value(
        ctx,
        reinterpret_cast<const uint8_t *>(key.constData()),
        static_cast<size_t>(key.size()),
        reinterpret_cast<const uint8_t *>(value.constData()),
        static_cast<size_t>(value.size()),
        &Libp2pModulePlugin::genericCallback,
        this
    ) == RET_OK;
}

bool Libp2pModulePlugin::getValue(
    const QByteArray &key,
    int quorumOverride)
{
    if (!ctx) return false;

    return libp2p_get_value(
        ctx,
        reinterpret_cast<const uint8_t *>(key.constData()),
        static_cast<size_t>(key.size()),
        quorumOverride,
        &Libp2pModulePlugin::bufferCallback,
        this
    ) == RET_OK;
}
bool Libp2pModulePlugin::addProvider(const QString &cid)
{
    if (!ctx) return false;

    return libp2p_add_provider(
        ctx,
        cid.toUtf8().constData(),
        &Libp2pModulePlugin::genericCallback,
        this
    ) == RET_OK;
}

bool Libp2pModulePlugin::startProviding(const QString &cid)
{
    if (!ctx) return false;

    return libp2p_start_providing(
        ctx,
        cid.toUtf8().constData(),
        &Libp2pModulePlugin::genericCallback,
        this
    ) == RET_OK;
}

bool Libp2pModulePlugin::stopProviding(const QString &cid)
{
    if (!ctx) return false;

    return libp2p_stop_providing(
        ctx,
        cid.toUtf8().constData(),
        &Libp2pModulePlugin::genericCallback,
        this
    ) == RET_OK;
}

bool Libp2pModulePlugin::getProviders(const QString &cid)
{
    if (!ctx) return false;

    return libp2p_get_providers(
        ctx,
        cid.toUtf8().constData(),
        &Libp2pModulePlugin::getProvidersCallback,
        this
    ) == RET_OK;
}


