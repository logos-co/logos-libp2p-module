#include "plugin.h"

#include <QtCore/QUuid>
#include <QtCore/QByteArray>
#include <QtCore/QDebug>
#include <cstring>

QString Libp2pModulePlugin::gossipsubPublish(
    const QString &topic,
    const QByteArray &data
)
{
    if (!ctx)
        return {};

    QString uuid = QUuid::createUuid().toString();
    auto *callbackCtx =
        new CallbackContext{ "gossipsubPublish", uuid, this };

    QByteArray topicUtf8 = topic.toUtf8();

    int ret = libp2p_gossipsub_publish(
        ctx,
        topicUtf8.constData(),
        reinterpret_cast<uint8_t*>(const_cast<char*>(data.constData())),
        data.size(),
        &Libp2pModulePlugin::libp2pCallback,
        callbackCtx
    );

    if (ret != RET_OK) {
        delete callbackCtx;
        return {};
    }

    return uuid;
}

QString Libp2pModulePlugin::gossipsubSubscribe(
    const QString &topic
)
{
    if (!ctx)
        return {};

    QString uuid = QUuid::createUuid().toString();
    auto *callbackCtx =
        new CallbackContext{ "gossipsubSubscribe", uuid, this };

    QByteArray topicUtf8 = topic.toUtf8();

    int ret = libp2p_gossipsub_subscribe(
        ctx,
        topicUtf8.constData(),
        &Libp2pModulePlugin::topicHandler,
        &Libp2pModulePlugin::libp2pCallback,
        callbackCtx
    );

    if (ret != RET_OK) {
        delete callbackCtx;
        return {};
    }

    return uuid;
}

QString Libp2pModulePlugin::gossipsubUnsubscribe(
    const QString &topic
)
{
    if (!ctx)
        return {};

    QString uuid = QUuid::createUuid().toString();
    auto *callbackCtx =
        new CallbackContext{ "gossipsubUnsubscribe", uuid, this };

    QByteArray topicUtf8 = topic.toUtf8();

    int ret = libp2p_gossipsub_unsubscribe(
        ctx,
        topicUtf8.constData(),
        &Libp2pModulePlugin::topicHandler,
        &Libp2pModulePlugin::libp2pCallback,
        callbackCtx
    );

    if (ret != RET_OK) {
        delete callbackCtx;
        return {};
    }

    return uuid;
}
