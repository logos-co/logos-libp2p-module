#include "libp2p_module_plugin.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <cstring>

// ---------------- Generic callback ----------------
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

// ---------------- Constructor / Destructor ----------------
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

// ---------------- Context management ----------------
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

// ---------------- Start / Stop ----------------
bool Libp2pModulePlugin::start()
{
    if (!ctx) {
        return false;
    }

    return libp2p_start(ctx, &Libp2pModulePlugin::genericCallback, this) == RET_OK;
}

bool Libp2pModulePlugin::stop()
{
    if (!ctx) {
        return false;
    }

    return libp2p_stop(ctx, &Libp2pModulePlugin::genericCallback, this) == RET_OK;
}

// ---------------- Event callback ----------------
bool Libp2pModulePlugin::setEventCallback()
{
    if (!ctx) {
        return false;
    }

    libp2p_set_event_callback(ctx, &Libp2pModulePlugin::genericCallback, this);
    return true;
}

