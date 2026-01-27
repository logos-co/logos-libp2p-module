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
    : ctx(nullptr)
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

bool Libp2pModulePlugin::start()
{
    qDebug() << "Libp2pModulePlugin::start called";
    if (!ctx) {
        qDebug() << "start called with no context";
        return false;
    }

    return libp2p_start(ctx, &Libp2pModulePlugin::libp2pCallback, this) == RET_OK;
}

bool Libp2pModulePlugin::stop()
{
    qDebug() << "Libp2pModulePlugin::stop called";
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


