#include "plugin.h"

#include <QtCore/QUuid>
#include <QtCore/QDebug>

QString Libp2pModulePlugin::discoStart()
{
    if (!ctx)
        return {};

    QString uuid = QUuid::createUuid().toString();
    auto *callbackCtx = new CallbackContext{ "discoStart", uuid, this };

    int ret = libp2p_service_disco_start(
        ctx,
        &Libp2pModulePlugin::libp2pCallback,
        callbackCtx
    );

    if (ret != RET_OK) {
        delete callbackCtx;
        return {};
    }

    return uuid;
}

QString Libp2pModulePlugin::discoStop()
{
    if (!ctx)
        return {};

    QString uuid = QUuid::createUuid().toString();
    auto *callbackCtx = new CallbackContext{ "discoStop", uuid, this };

    int ret = libp2p_service_disco_stop(
        ctx,
        &Libp2pModulePlugin::libp2pCallback,
        callbackCtx
    );

    if (ret != RET_OK) {
        delete callbackCtx;
        return {};
    }

    return uuid;
}

QString Libp2pModulePlugin::discoStartAdvertising(
    const QString &serviceId,
    const QByteArray &serviceData
)
{
    if (!ctx)
        return {};

    QString uuid = QUuid::createUuid().toString();
    auto *callbackCtx = new CallbackContext{ "discoStartAdvertising", uuid, this };

    QByteArray idUtf8 = serviceId.toUtf8();

    int ret = libp2p_service_disco_start_advertising(
        ctx,
        idUtf8.constData(),
        serviceData.isEmpty()
            ? nullptr
            : reinterpret_cast<const uint8_t *>(serviceData.constData()),
        serviceData.size(),
        &Libp2pModulePlugin::libp2pCallback,
        callbackCtx
    );

    if (ret != RET_OK) {
        delete callbackCtx;
        return {};
    }

    return uuid;
}

QString Libp2pModulePlugin::discoStopAdvertising(
    const QString &serviceId
)
{
    if (!ctx)
        return {};

    QString uuid = QUuid::createUuid().toString();
    auto *callbackCtx = new CallbackContext{ "discoStopAdvertising", uuid, this };

    QByteArray idUtf8 = serviceId.toUtf8();

    int ret = libp2p_service_disco_stop_advertising(
        ctx,
        idUtf8.constData(),
        &Libp2pModulePlugin::libp2pCallback,
        callbackCtx
    );

    if (ret != RET_OK) {
        delete callbackCtx;
        return {};
    }

    return uuid;
}

QString Libp2pModulePlugin::discoStartDiscovering(
    const QString &serviceId
)
{
    if (!ctx)
        return {};

    QString uuid = QUuid::createUuid().toString();
    auto *callbackCtx = new CallbackContext{ "discoStartDiscovering", uuid, this };

    QByteArray idUtf8 = serviceId.toUtf8();

    int ret = libp2p_service_disco_start_discovering(
        ctx,
        idUtf8.constData(),
        &Libp2pModulePlugin::libp2pCallback,
        callbackCtx
    );

    if (ret != RET_OK) {
        delete callbackCtx;
        return {};
    }

    return uuid;
}

QString Libp2pModulePlugin::discoStopDiscovering(
    const QString &serviceId
)
{
    if (!ctx)
        return {};

    QString uuid = QUuid::createUuid().toString();
    auto *callbackCtx = new CallbackContext{ "discoStopDiscovering", uuid, this };

    QByteArray idUtf8 = serviceId.toUtf8();

    int ret = libp2p_service_disco_stop_discovering(
        ctx,
        idUtf8.constData(),
        &Libp2pModulePlugin::libp2pCallback,
        callbackCtx
    );

    if (ret != RET_OK) {
        delete callbackCtx;
        return {};
    }

    return uuid;
}

QString Libp2pModulePlugin::discoLookup(
    const QString &serviceId,
    const QByteArray &serviceData
)
{
    if (!ctx)
        return {};

    QString uuid = QUuid::createUuid().toString();
    auto *callbackCtx = new CallbackContext{ "discoLookup", uuid, this };

    QByteArray idUtf8 = serviceId.toUtf8();

    int ret = libp2p_service_disco_lookup(
        ctx,
        idUtf8.constData(),
        serviceData.isEmpty()
            ? nullptr
            : reinterpret_cast<const uint8_t *>(serviceData.constData()),
        serviceData.size(),
        &Libp2pModulePlugin::randomRecordsCallback,
        callbackCtx
    );

    if (ret != RET_OK) {
        delete callbackCtx;
        return {};
    }

    return uuid;
}

QString Libp2pModulePlugin::discoRandomLookup()
{
    if (!ctx)
        return {};

    QString uuid = QUuid::createUuid().toString();
    auto *callbackCtx = new CallbackContext{ "discoRandomLookup", uuid, this };

    int ret = libp2p_service_disco_random_lookup(
        ctx,
        &Libp2pModulePlugin::randomRecordsCallback,
        callbackCtx
    );

    if (ret != RET_OK) {
        delete callbackCtx;
        return {};
    }

    return uuid;
}
