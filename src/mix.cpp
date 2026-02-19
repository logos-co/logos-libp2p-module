#include "plugin.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <cstring>

QByteArray Libp2pModulePlugin::mixGeneratePrivKey()
{
    if (!ctx)
        return {};

    QString uuid = QUuid::createUuid().toString();

    libp2p_curve25519_key_t key {};
    libp2p_mix_generate_priv_key(&key);

    return QByteArray(
        reinterpret_cast<const char*>(&key),
        sizeof(key)
    );
}

QByteArray Libp2pModulePlugin::mixPublicKey(const QByteArray &privKey)
{
    if (!ctx)
        return {};

    if (privKey.size() != sizeof(libp2p_curve25519_key_t)) {
        qCritical() << "mixPublicKey invalid private key size:" << privKey.size();
        return {};
    }

    QString uuid = QUuid::createUuid().toString();

    libp2p_curve25519_key_t inKey {};
    libp2p_curve25519_key_t outKey {};

    memcpy(&inKey, privKey.constData(), sizeof(inKey));

    libp2p_mix_public_key(inKey, &outKey);

    return QByteArray(
        reinterpret_cast<const char*>(&outKey),
        sizeof(outKey)
    );
}

QString Libp2pModulePlugin::mixDial(
    const QString &peerId,
    const QString &multiaddr,
    const QString &proto
)
{
    if (!ctx)
        return {};

    QString uuid = QUuid::createUuid().toString();
    auto *callbackCtx =
        new CallbackContext{ "mixDial", uuid, this };

    QByteArray peerIdUtf8 = peerId.toUtf8();
    QByteArray addrUtf8 = multiaddr.toUtf8();
    QByteArray protoUtf8 = proto.toUtf8();

    int ret = libp2p_mix_dial(
        ctx,
        peerIdUtf8.constData(),
        addrUtf8.constData(),
        protoUtf8.constData(),
        &Libp2pModulePlugin::connectionCallback,
        callbackCtx
    );

    if (ret != RET_OK) {
        delete callbackCtx;
        return {};
    }

    return uuid;
}

QString Libp2pModulePlugin::mixDialWithReply(
    const QString &peerId,
    const QString &multiaddr,
    const QString &proto,
    int expectReply,
    uint8_t numSurbs
)
{
    if (!ctx)
        return {};

    QString uuid = QUuid::createUuid().toString();
    auto *callbackCtx =
        new CallbackContext{ "mixDialWithReply", uuid, this };

    QByteArray peerIdUtf8 = peerId.toUtf8();
    QByteArray addrUtf8 = multiaddr.toUtf8();
    QByteArray protoUtf8 = proto.toUtf8();

    int ret = libp2p_mix_dial_with_reply(
        ctx,
        peerIdUtf8.constData(),
        addrUtf8.constData(),
        protoUtf8.constData(),
        expectReply,
        numSurbs,
        &Libp2pModulePlugin::connectionCallback,
        callbackCtx
    );

    if (ret != RET_OK) {
        delete callbackCtx;
        return {};
    }

    return uuid;
}

QString Libp2pModulePlugin::mixRegisterDestReadBehavior(
    const QString &proto,
    int behavior,
    uint32_t sizeParam
)
{
    if (!ctx)
        return {};

    QString uuid = QUuid::createUuid().toString();
    auto *callbackCtx =
        new CallbackContext{ "mixRegisterDestReadBehavior", uuid, this };

    QByteArray protoUtf8 = proto.toUtf8();

    int ret = libp2p_mix_register_dest_read_behavior(
        ctx,
        protoUtf8.constData(),
        static_cast<Libp2pMixReadBehavior>(behavior),
        sizeParam,
        &Libp2pModulePlugin::libp2pCallback,
        callbackCtx
    );

    if (ret != RET_OK) {
        delete callbackCtx;
        return {};
    }

    return uuid;
}

QString Libp2pModulePlugin::mixSetNodeInfo(
    const QString &multiaddr,
    const QByteArray &mixPrivKey
)
{
    if (!ctx)
        return {};

    if (mixPrivKey.size() != sizeof(libp2p_curve25519_key_t)) {
        qCritical() << "mixSetNodeInfo invalid key size:" << mixPrivKey.size();
        return {};
    }

    QString uuid = QUuid::createUuid().toString();
    auto *callbackCtx =
        new CallbackContext{ "mixSetNodeInfo", uuid, this };

    QByteArray addrUtf8 = multiaddr.toUtf8();

    libp2p_curve25519_key_t key {};
    memcpy(&key, mixPrivKey.constData(), sizeof(key));

    int ret = libp2p_mix_set_node_info(
        ctx,
        addrUtf8.constData(),
        key,
        &Libp2pModulePlugin::libp2pCallback,
        callbackCtx
    );

    if (ret != RET_OK) {
        delete callbackCtx;
        return {};
    }

    return uuid;
}

QString Libp2pModulePlugin::mixNodepoolAdd(
    const QString &peerId,
    const QString &multiaddr,
    const QByteArray &mixPubKey,
    const QByteArray &libp2pPubKey
)
{
    if (!ctx)
        return {};

    QString uuid = QUuid::createUuid().toString();
    auto *callbackCtx =
        new CallbackContext{ "mixNodepoolAdd", uuid, this };

    QByteArray peerIdUtf8 = peerId.toUtf8();
    QByteArray addrUtf8 = multiaddr.toUtf8();

    libp2p_curve25519_key_t mixKey {};
    libp2p_secp256k1_pubkey_t lpKey {};

    if (mixPubKey.size() != sizeof(libp2p_curve25519_key_t) ||
        libp2pPubKey.size() != sizeof(libp2p_secp256k1_pubkey_t)) {
        qCritical() << "mixNodepoolAdd invalid key sizes";
        return {};
    }

    memcpy(&mixKey, mixPubKey.constData(), sizeof(mixKey));
    memcpy(&lpKey, libp2pPubKey.constData(), sizeof(lpKey));

    int ret = libp2p_mix_nodepool_add(
        ctx,
        peerIdUtf8.constData(),
        addrUtf8.constData(),
        mixKey,
        lpKey,
        &Libp2pModulePlugin::libp2pCallback,
        callbackCtx
    );

    if (ret != RET_OK) {
        delete callbackCtx;
        return {};
    }

    return uuid;
}

