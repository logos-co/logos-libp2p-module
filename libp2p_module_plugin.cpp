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

