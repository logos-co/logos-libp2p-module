#include "plugin.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <cstring>
#include <QDebug>
#include <QCoreApplication>
#include <QEventLoop>
#include <QThread>

Libp2pModulePlugin::Libp2pModulePlugin(const Libp2pModuleOptions &options)
    : ctx(nullptr),
      m_bootstrapNodes(options.bootstrapNodes),
      m_addrs(options.addrs)
{
    qRegisterMetaType<PeerInfo>("PeerInfo");
    qRegisterMetaType<QList<PeerInfo>>("QList<PeerInfo>");

    qRegisterMetaType<ServiceInfo>("ServiceInfo");
    qRegisterMetaType<QList<ServiceInfo>>("QList<ServiceInfo>");

    qRegisterMetaType<ExtendedPeerRecord>("ExtendedPeerRecord");
    qRegisterMetaType<QList<ExtendedPeerRecord>>("QList<ExtendedPeerRecord>");

    qRegisterMetaType<Libp2pResult>("Libp2pResult");

    std::memset(&m_libp2pConfig, 0, sizeof(m_libp2pConfig));

    m_libp2pConfig.mount_gossipsub = options.mountGossipsub ? 1 : 0;
    m_libp2pConfig.gossipsub_trigger_self = options.gossipsubTriggerSelf ? 1 : 0;

    m_libp2pConfig.max_connections    = options.maxConnections;
    m_libp2pConfig.max_in             = options.maxInConnections;
    m_libp2pConfig.max_out            = options.maxOutConnections;
    m_libp2pConfig.max_conns_per_peer = options.maxConnsPerPeer;

    m_libp2pConfig.autonat = options.autonat ? 1 : 0;
    m_libp2pConfig.autonat_v2 = options.autonatV2 ? 1 : 0;
    m_libp2pConfig.autonat_v2_server = options.autonatV2Server ? 1 : 0;
    m_libp2pConfig.circuit_relay = options.circuitRelay ? 1 : 0;
    m_libp2pConfig.circuit_relay_client = options.circuitRelayClient ? 1 : 0;

    m_libp2pConfig.transport = options.transport;

    /* -------------------------
     * Save local listen addrs
     * ------------------------- */

    if (!m_addrs.isEmpty()) {
        m_addrsUtf8.reserve(m_addrs.size());
        m_addrsPtr.reserve(m_addrs.size());

        for (const QString &addr : m_addrs) {
            m_addrsUtf8.push_back(addr.toUtf8());
            m_addrsPtr.push_back(m_addrsUtf8.back().data());
        }

        m_libp2pConfig.addrs = const_cast<const char **>(m_addrsPtr.data());
        m_libp2pConfig.addrsLen = m_addrsPtr.size();
    }

    /* -------------------------
     * Bootstrap nodes
     * ------------------------- */

    if (!m_bootstrapNodes.isEmpty()) {
        m_bootstrapCNodes.reserve(m_bootstrapNodes.size());
        m_addrUtf8Storage.reserve(m_bootstrapNodes.size());
        m_addrPtrStorage.reserve(m_bootstrapNodes.size());
        m_peerIdStorage.reserve(m_bootstrapNodes.size());

        for (const PeerInfo &p : m_bootstrapNodes) {
            QVector<QByteArray> utf8List;
            QVector<char*> ptrList;

            utf8List.reserve(p.addrs.size());
            ptrList.reserve(p.addrs.size());

            for (const QString &addr : p.addrs) {
                utf8List.push_back(addr.toUtf8());
                ptrList.push_back(utf8List.back().data());
            }

            m_addrUtf8Storage.push_back(std::move(utf8List));
            m_addrPtrStorage.push_back(std::move(ptrList));

            m_peerIdStorage.push_back(p.peerId.toUtf8());

            libp2p_bootstrap_node_t node{};
            node.peerId = m_peerIdStorage.back().constData();
            node.multiaddrs =
                const_cast<const char**>(m_addrPtrStorage.back().data());
            node.multiaddrsLen = m_addrPtrStorage.back().size();

            m_bootstrapCNodes.push_back(node);
        }

        m_libp2pConfig.kad_bootstrap_nodes = m_bootstrapCNodes.data();
        m_libp2pConfig.kad_bootstrap_nodes_len = m_bootstrapCNodes.size();
    }

    m_libp2pConfig.mount_kad = options.mountKad ? 1 : 0;

    m_libp2pConfig.mount_service_discovery = options.mountServiceDiscovery ? 1 : 0;

    m_libp2pConfig.mount_mix = options.mountMix ? 1 : 0;

    /* -------------------------
     * Generate secp256k1 key
     * ------------------------- */

    auto res = this->syncLibp2pNewPrivateKey();
    if (!res.ok) {
        qFatal("libp2p_new_private_key failed: %s", qPrintable(res.error));
    }
    QByteArray key = res.data.toByteArray();

    uint8_t *buf = (uint8_t*)malloc(key.size());
    memcpy(buf, key.constData(), key.size());

    m_libp2pConfig.priv_key.data = buf;
    m_libp2pConfig.priv_key.dataLen = key.size();

    /* -------------------------
     * Call libp2p_new
     * ------------------------- */

    auto *newCallbackCtx = new CallbackContext{
        "libp2pNew",
        QUuid::createUuid().toString(),
        this
    };

    m_newDone = false;

    ctx = libp2p_new(&m_libp2pConfig,
                     &Libp2pModulePlugin::libp2pCallback,
                     newCallbackCtx);

    QElapsedTimer timer;
    timer.start();
    while (!m_newDone) {
        if (timer.elapsed() > 5000) {
            qFatal("libp2p_new timeout");
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }

    connect(this,
            &Libp2pModulePlugin::libp2pEvent,
            this,
            &Libp2pModulePlugin::onLibp2pEventDefault);
}


Libp2pModulePlugin::~Libp2pModulePlugin()
{
    // Stream Registry cleanup
    QList<uint64_t> streamIds;
    {

        QWriteLocker locker(&m_streamsLock);
        streamIds = m_streams.keys();
    }
    for (uint64_t streamId : streamIds) {
        syncStreamRelease(streamId);
    }

    // Stop libp2p
    if (ctx) {
        auto *callbackCtx = new CallbackContext{
            "libp2pDestroy",
            QUuid::createUuid().toString(),
            this
        };

        m_destroyDone = false;

        libp2p_destroy(
            ctx,
            &Libp2pModulePlugin::libp2pCallback,
            callbackCtx
        );

        QElapsedTimer timer;
        timer.start();

        while (!m_destroyDone) {
            if (timer.elapsed() > 5000) {
                qFatal("libp2p_destroy timeout");
            }
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        }

        ctx = nullptr;

    }

    // Logos cleanup
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

/* ---------------- Helper Functions ----------------- */

QString Libp2pModulePlugin::toCid(const QByteArray &key)
{
    if (key.isEmpty())
        return {};

    QString uuid = QUuid::createUuid().toString();
    auto *callbackCtx = new CallbackContext{ "toCid", uuid, this };

    int ret = libp2p_create_cid(
        1,                      // CIDv1
        "dag-pb",
        "sha2-256",
        reinterpret_cast<const uint8_t *>(key.constData()),
        size_t(key.size()),
        &Libp2pModulePlugin::libp2pCallback,
        callbackCtx
    );

    if (ret != RET_OK) {
        delete callbackCtx;
        return {};
    }

    return uuid;
}

/* --------------- Libp2p Core --------------- */

QString Libp2pModulePlugin::libp2pStart()
{
    qDebug() << "Libp2pModulePlugin::libp2pStart called";
    if (!ctx) {
        qDebug() << "libp2pStart called without a context";
        return {};
    }

    QString uuid = QUuid::createUuid().toString();
    auto *callbackCtx = new CallbackContext{ "libp2pStart", uuid, this };

    int ret = libp2p_start(ctx, &Libp2pModulePlugin::libp2pCallback, callbackCtx);

    if (ret != RET_OK) {
        delete callbackCtx;
        return {};
    }

    return uuid;
}

QString Libp2pModulePlugin::libp2pStop()
{
    qDebug() << "Libp2pModulePlugin::libp2pStop called";
    if (!ctx) {
        qDebug() << "libp2pStop called without a context";
        return {};
    }

    QString uuid = QUuid::createUuid().toString();
    auto *callbackCtx = new CallbackContext{ "libp2pStop", uuid, this };

    int ret = libp2p_stop(ctx, &Libp2pModulePlugin::libp2pCallback, callbackCtx);

    if (ret != RET_OK) {
        delete callbackCtx;
        return {};
    }

    return uuid;
}

QString Libp2pModulePlugin::libp2pPublicKey()
{
    if (!ctx)
        return {};

    QString uuid = QUuid::createUuid().toString();
    auto *callbackCtx =
        new CallbackContext{ "libp2pPublicKey", uuid, this };

    int ret = libp2p_public_key(
        ctx,
        &Libp2pModulePlugin::libp2pBufferCallback,
        callbackCtx
    );

    if (ret != RET_OK) {
        delete callbackCtx;
        return {};
    }

    return uuid;
}

QString Libp2pModulePlugin::libp2pNewPrivateKey()
{
    qDebug() << "Libp2pModulePlugin::libp2pNewPrivateKey called";

    QString uuid = QUuid::createUuid().toString();
    auto *callbackCtx = new CallbackContext{ "libp2pNewPrivateKey", uuid, this };

    int ret = libp2p_new_private_key(
        LIBP2P_PK_SECP256K1,
        &Libp2pModulePlugin::libp2pBufferCallback,
        callbackCtx
    );

    if (ret != RET_OK) {
        delete callbackCtx;
        return {};
    }

    return uuid;
}

/* --------------- Connectivity --------------- */
QString Libp2pModulePlugin::connectPeer(
    const QString &peerId,
    const QList<QString> multiaddrs,
    int64_t timeoutMs
)
{
    if (!ctx) return {};

    QByteArray peerIdUtf8 = peerId.toUtf8();

    QList<QByteArray> addrBuffers;
    QVector<const char*> addrPtrs;

    addrBuffers.reserve(multiaddrs.size());
    addrPtrs.reserve(multiaddrs.size());

    for (const auto &addr : multiaddrs) {
        addrBuffers.append(addr.toUtf8());
        addrPtrs.append(addrBuffers.last().constData());
    }

    QString uuid = QUuid::createUuid().toString();
    auto *callbackCtx = new CallbackContext{
        "connectPeer",
        uuid,
        this
    };

    int ret = libp2p_connect(
        ctx,
        peerIdUtf8.constData(),
        addrPtrs.data(),
        addrPtrs.size(),
        timeoutMs,
        &Libp2pModulePlugin::libp2pCallback,
        callbackCtx
    );

    if (ret != RET_OK) {
        delete callbackCtx;
        return {};
    }

    return uuid;
}

QString Libp2pModulePlugin::disconnectPeer(const QString &peerId)
{
    if (!ctx) return {};

    QByteArray peerIdUtf8 = peerId.toUtf8();

    QString uuid = QUuid::createUuid().toString();
    auto *callbackCtx = new CallbackContext{
        "disconnectPeer",
        uuid,
        this
    };

    int ret = libp2p_disconnect(
        ctx,
        peerIdUtf8.constData(),
        &Libp2pModulePlugin::libp2pCallback,
        callbackCtx
    );

    if (ret != RET_OK) {
        delete callbackCtx;
        return {};
    }

    return uuid;
}

QString Libp2pModulePlugin::peerInfo()
{
    if (!ctx) return {};

    QString uuid = QUuid::createUuid().toString();
    auto *callbackCtx = new CallbackContext{
        "peerInfo",
        uuid,
        this
    };

    int ret = libp2p_peerinfo(
        ctx,
        &Libp2pModulePlugin::peerInfoCallback,
        callbackCtx
    );

    if (ret != RET_OK) {
        delete callbackCtx;
        return {};
    }

    return uuid;
}

QString Libp2pModulePlugin::connectedPeers(int direction)
{
    if (!ctx) return {};

    QString uuid = QUuid::createUuid().toString();
    auto *callbackCtx = new CallbackContext{
        "connectedPeers",
        uuid,
        this
    };

    int ret = libp2p_connected_peers(
        ctx,
        direction,
        &Libp2pModulePlugin::peersCallback,
        callbackCtx
    );

    if (ret != RET_OK) {
        delete callbackCtx;
        return {};
    }

    return uuid;
}

QString Libp2pModulePlugin::dial(const QString &peerId, const QString &proto)
{
    if (!ctx) return {};

    QByteArray peerIdUtf8 = peerId.toUtf8();
    QByteArray protoUtf8 = proto.toUtf8();

    QString uuid = QUuid::createUuid().toString();
    auto *callbackCtx = new CallbackContext{
        "dial",
        uuid,
        this
    };

    int ret = libp2p_dial(
        ctx,
        peerIdUtf8.constData(),
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

/* --------------- Circuit Relay --------------- */

QString Libp2pModulePlugin::circuitRelayReserve(const QString &relayPeerId, const QList<QString> &relayAddrs)
{
    if (!ctx) return {};

    QByteArray peerIdUtf8 = relayPeerId.toUtf8();

    QList<QByteArray> addrBuffers;
    QVector<const char*> addrPtrs;
    addrBuffers.reserve(relayAddrs.size());
    addrPtrs.reserve(relayAddrs.size());
    for (const auto &addr : relayAddrs) {
        addrBuffers.append(addr.toUtf8());
        addrPtrs.append(addrBuffers.last().constData());
    }

    QString uuid = QUuid::createUuid().toString();
    auto *callbackCtx = new CallbackContext{"circuitRelayReserve", uuid, this};

    int ret = libp2p_circuit_relay_reserve(
        ctx,
        peerIdUtf8.constData(),
        addrPtrs.data(),
        addrPtrs.size(),
        &Libp2pModulePlugin::reservationCallback,
        callbackCtx
    );

    if (ret != RET_OK) {
        delete callbackCtx;
        return {};
    }

    return uuid;
}

QString Libp2pModulePlugin::dialCircuitRelay(const QString &dstPeerId, const QString &multiaddr, const QString &proto)
{
    if (!ctx) return {};

    QByteArray peerIdUtf8 = dstPeerId.toUtf8();
    QByteArray addrUtf8 = multiaddr.toUtf8();
    QByteArray protoUtf8 = proto.toUtf8();

    QString uuid = QUuid::createUuid().toString();
    auto *callbackCtx = new CallbackContext{"dialCircuitRelay", uuid, this};

    int ret = libp2p_dial_circuit_relay(
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

bool Libp2pModulePlugin::setEventCallback()
{
    if (!ctx) {
        return false;
    }

    libp2p_set_event_callback(ctx, &Libp2pModulePlugin::libp2pCallback, NULL);
    return true;
}

