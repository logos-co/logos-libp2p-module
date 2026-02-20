#include "plugin.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <cstring>
#include <QDebug>
#include <QCoreApplication>
#include <QEventLoop>
#include <QThread>

struct KeyCtx {
    libp2p_private_key_t *out;
    std::atomic<bool> done{false};
    int status = RET_OK;
    QString errMsg;
};

static void private_key_handler(
    int callerRet,
    const uint8_t *keyData,
    size_t keyDataLen,
    const char *msg,
    size_t msgLen,
    void *userData)
{
    auto *ctx = static_cast<KeyCtx*>(userData);

    if (callerRet != RET_OK) {
        qCritical() << "libp2p_new_private_key failed:"
                    << QByteArray(msg, int(msgLen));
        ctx->done.store(true, std::memory_order_release);
        return;
    }

    if (!keyData || keyDataLen == 0) {
        qCritical() << "libp2p_new_private_key returned empty key";
        ctx->done.store(true, std::memory_order_release);
        return;
    }

    uint8_t *buf = (uint8_t*)malloc(keyDataLen);
    memcpy(buf, keyData, keyDataLen);

    ctx->out->data = buf;
    ctx->out->dataLen = keyDataLen;

    ctx->done.store(true, std::memory_order_release);
}

Libp2pModulePlugin::Libp2pModulePlugin(const QList<PeerInfo> &bootstrapNodes)
    : ctx(nullptr),
      m_bootstrapNodes(bootstrapNodes)
{
    qRegisterMetaType<PeerInfo>("PeerInfo");
    qRegisterMetaType<QList<PeerInfo>>("QList<PeerInfo>");

    qRegisterMetaType<ServiceInfo>("ServiceInfo");
    qRegisterMetaType<QList<ServiceInfo>>("QList<ServiceInfo>");

    qRegisterMetaType<ExtendedPeerRecord>("ExtendedPeerRecord");
    qRegisterMetaType<QList<ExtendedPeerRecord>>("QList<ExtendedPeerRecord>");

    qRegisterMetaType<Libp2pResult>("Libp2pResult");

    std::memset(&config, 0, sizeof(config));

    config.flags |= LIBP2P_CFG_GOSSIPSUB;
    config.mount_gossipsub = 1;

    config.flags |= LIBP2P_CFG_GOSSIPSUB_TRIGGER_SELF;
    config.gossipsub_trigger_self = 1;

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

        config.flags |= LIBP2P_CFG_KAD_BOOTSTRAP_NODES;
        config.kad_bootstrap_nodes = m_bootstrapCNodes.data();
        config.kad_bootstrap_nodes_len = m_bootstrapCNodes.size();
    }

    config.flags |= LIBP2P_CFG_KAD;
    config.mount_kad = 1;

    config.flags |= LIBP2P_CFG_KAD_DISCOVERY;
    config.mount_kad_discovery = 1;

    config.flags |= LIBP2P_CFG_MIX;
    config.flags |= LIBP2P_CFG_PRIVATE_KEY;
    config.mount_mix = 1;

    /* -------------------------
     * Generate secp256k1 key
     * ------------------------- */

    std::memset(&m_privKey, 0, sizeof(m_privKey));

    auto *keyCtx = new KeyCtx();
    keyCtx->out = &m_privKey;

    libp2p_new_private_key(
        LIBP2P_PK_SECP256K1,
        private_key_handler,
        keyCtx
    );

    // Wait for handler
    while (!keyCtx->done.load(std::memory_order_acquire)) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 1);
    }
    delete keyCtx;

    // Check result
    if (!m_privKey.data || m_privKey.dataLen == 0) {
        qFatal("Failed to generate private key, cannot continue");
    }

    config.priv_key = m_privKey;

    qDebug() << "privKey len:" << config.priv_key.dataLen
             << "ptr:" << config.priv_key.data;

    /* -------------------------
     * Call libp2p_new
     * ------------------------- */

    auto *callbackCtx = new CallbackContext{
        "libp2pNew",
        QUuid::createUuid().toString(),
        this
    };

    m_newDone = false;

    ctx = libp2p_new(&config,
                     &Libp2pModulePlugin::libp2pCallback,
                     callbackCtx);

    while (!m_newDone.load(std::memory_order_acquire)) {
        QThread::msleep(1);
    }

    connect(this,
            &Libp2pModulePlugin::libp2pEvent,
            this,
            &Libp2pModulePlugin::onLibp2pEventDefault);
}


Libp2pModulePlugin::~Libp2pModulePlugin()
{
    // Stream Registry cleanup
    {
        QList<uint64_t> streamIds;

        QWriteLocker locker(&m_streamsLock);
        streamIds = m_streams.keys();

        for (uint64_t streamId : streamIds) {
            syncStreamRelease(streamId);
        }
    }

    // Free private key memory
    if (m_privKey.data) {
        free(m_privKey.data);
    }
    m_privKey.data = nullptr;
    m_privKey.dataLen = 0;

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

bool Libp2pModulePlugin::foo(const QString &bar)
{
    qDebug() << "Libp2pModulePlugin::foo called with:" << bar;

    QVariantList eventData;
    eventData << bar;
    eventData << QDateTime::currentDateTime().toString(Qt::ISODate);

    if (logosAPI) {
        qDebug() << "Libp2pModulePlugin: Triggering event 'fooTriggered' with data:" << eventData;
        logosAPI->getClient("core_manager")->onEventResponse(this, "fooTriggered", eventData);
        qDebug() << "Libp2pModulePlugin: Event 'fooTriggered' triggered with data:" << eventData;
    } else {
        qWarning() << "Libp2pModulePlugin: LogosAPI not available, cannot trigger event";
    }

    return true;
}

/* --------------- Start/stop --------------- */

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


bool Libp2pModulePlugin::setEventCallback()
{
    if (!ctx) {
        return false;
    }

    libp2p_set_event_callback(ctx, &Libp2pModulePlugin::libp2pCallback, NULL);
    return true;
}

