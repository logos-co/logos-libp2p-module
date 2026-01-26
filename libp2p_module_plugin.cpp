#include "libp2p_module_plugin.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <cstring>
#include <QDebug>

void Libp2pModulePlugin::libp2pCallback(
    int callerRet,
    const char *msg,
    size_t len,
    void *userData
) {
    if (callerRet != RET_OK) {
        if (strlen(msg) > 0) {
            qDebug() << "Message: " << msg;
        }
        qDebug() << "Return value:" << callerRet;
    }
}

Libp2pModulePlugin::Libp2pModulePlugin()
    : QObject(nullptr), ctx(nullptr)
{
    static libp2p_config_t cfg;
    std::memset(&cfg, 0, sizeof(cfg));

    cfg.flags |= LIBP2P_CFG_GOSSIPSUB;
    cfg.mount_gossipsub = 1;

    cfg.flags |= LIBP2P_CFG_GOSSIPSUB_TRIGGER_SELF;
    cfg.gossipsub_trigger_self = 1;

    cfg.flags |= LIBP2P_CFG_KAD;
    cfg.mount_kad = 1;

    ctx = libp2p_new(&cfg, &Libp2pModulePlugin::libp2pCallback, this);
}

Libp2pModulePlugin::~Libp2pModulePlugin()
{
    if (ctx) {
        libp2p_destroy(ctx, &Libp2pModulePlugin::libp2pCallback, this);
        ctx = nullptr;
    }
}

bool Libp2pModulePlugin::start()
{
    if (!ctx) {
        qDebug() << "start called with no context";
        return false;
    }

    return libp2p_start(ctx, &Libp2pModulePlugin::libp2pCallback, this) == RET_OK;
}

bool Libp2pModulePlugin::stop()
{
    if (!ctx) {
        qDebug() << "stop called with no context";
        return false;
    }

    return libp2p_stop(ctx, &Libp2pModulePlugin::libp2pCallback, this) == RET_OK;
}

bool Libp2pModulePlugin::setEventCallback()
{
    if (!ctx) {
        return false;
    }

    libp2p_set_event_callback(ctx, &Libp2pModulePlugin::libp2pCallback, this);
    return true;
}


