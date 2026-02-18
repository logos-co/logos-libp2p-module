#include "libp2p_module_plugin.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <cstring>
#include <QDebug>

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

    auto *callbackCtx = new CallbackContext{
        "libp2pNew",
        QUuid::createUuid().toString(),
        this
    };

    ctx = libp2p_new(&config,
                     &Libp2pModulePlugin::libp2pCallback,
                     callbackCtx);

    connect(this,
            &Libp2pModulePlugin::libp2pEvent,
            this,
            &Libp2pModulePlugin::onLibp2pEventDefault);
}


Libp2pModulePlugin::~Libp2pModulePlugin()
{
    // Stop libp2p
    if (ctx) {
        auto *callbackCtx = new CallbackContext{
            "libp2pDestroy",
            QUuid::createUuid().toString(),
            this
        };

        libp2p_destroy(ctx,
                       &Libp2pModulePlugin::libp2pCallback,
                       callbackCtx);

        ctx = nullptr;
    }

    // Stream Registry cleanup
    {
        QWriteLocker locker(&m_streamsLock);
        m_streams.clear();
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

// ------------------- Stream registry helpers ------------------- /
uint64_t Libp2pModulePlugin::addStream(libp2p_stream_t *stream)
{
    auto id = m_nextStreamId.fetch_add(1);
    QWriteLocker locker(&m_streamsLock);
    m_streams.insert(id, stream);
    return id;
}

libp2p_stream_t* Libp2pModulePlugin::getStream(uint64_t id) const
{
    QReadLocker locker(&m_streamsLock);
    return m_streams.value(id, nullptr);
}

libp2p_stream_t* Libp2pModulePlugin::removeStream(uint64_t id)
{
    QWriteLocker locker(&m_streamsLock);
    return m_streams.take(id);
}

bool Libp2pModulePlugin::hasStream(uint64_t id) const
{
    QReadLocker locker(&m_streamsLock);
    return m_streams.contains(id);
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

/* --------------- Streams --------------- */
QString Libp2pModulePlugin::streamClose(uint64_t streamId)
{

    if (!ctx || streamId == 0)
        return {};

    QString uuid = QUuid::createUuid().toString();
    auto *callbackCtx = new CallbackContext{
        "streamClose",
        uuid,
        this
    };

    auto *stream = removeStream(streamId);
    if (!stream)
        return {};

    int ret = libp2p_stream_close(
        ctx,
        stream,
        &Libp2pModulePlugin::libp2pCallback,
        callbackCtx
    );

    if (ret != RET_OK) {
        delete callbackCtx;
        return {};
    }

    return uuid;
}

QString Libp2pModulePlugin::streamCloseEOF(uint64_t streamId)
{
    if (!ctx || streamId == 0)
        return {};

    QString uuid = QUuid::createUuid().toString();
    auto *callbackCtx = new CallbackContext{
        "streamCloseEOF",
        uuid,
        this
    };

    auto *stream = removeStream(streamId);
    if (!stream)
        return {};

    int ret = libp2p_stream_closeWithEOF(
        ctx,
        stream,
        &Libp2pModulePlugin::libp2pCallback,
        callbackCtx
    );

    if (ret != RET_OK) {
        delete callbackCtx;
        return {};
    }

    return uuid;
}

QString Libp2pModulePlugin::streamRelease(uint64_t streamId)
{
    if (!ctx || streamId == 0)
        return {};

    QString uuid = QUuid::createUuid().toString();
    auto *callbackCtx = new CallbackContext{
        "streamRelease",
        uuid,
        this
    };

    auto *stream = removeStream(streamId);
    if (!stream)
        return {};
    removeStream(streamId);

    int ret = libp2p_stream_release(
        ctx,
        stream,
        &Libp2pModulePlugin::libp2pCallback,
        callbackCtx
    );

    if (ret != RET_OK) {
        delete callbackCtx;
        return {};
    }

    return uuid;
}

QString Libp2pModulePlugin::streamReadExactly(uint64_t streamId, size_t len)
{
    if (!ctx || streamId == 0)
        return {};

    QString uuid = QUuid::createUuid().toString();
    auto *callbackCtx = new CallbackContext{
        "streamReadExactly",
        uuid,
        this
    };

    auto *stream = getStream(streamId);
    if (!stream)
        return {};

    int ret = libp2p_stream_readExactly(
        ctx,
        stream,
        len,
        &Libp2pModulePlugin::libp2pBufferCallback,
        callbackCtx
    );

    if (ret != RET_OK) {
        delete callbackCtx;
        return {};
    }

    return uuid;
}

QString Libp2pModulePlugin::streamReadLp(uint64_t streamId, size_t maxSize)
{
    if (!ctx || streamId == 0)
        return {};

    QString uuid = QUuid::createUuid().toString();
    auto *callbackCtx = new CallbackContext{
        "streamReadLp",
        uuid,
        this
    };

    auto *stream = getStream(streamId);
    if (!stream)
        return {};

    int ret = libp2p_stream_readLp(
        ctx,
        stream,
        maxSize,
        &Libp2pModulePlugin::libp2pBufferCallback,
        callbackCtx
    );

    if (ret != RET_OK) {
        delete callbackCtx;
        return {};
    }

    return uuid;
}

QString Libp2pModulePlugin::streamWrite(uint64_t streamId, const QByteArray &data)
{
    if (!ctx || streamId == 0)
        return {};

    QString uuid = QUuid::createUuid().toString();
    auto *callbackCtx = new CallbackContext{
        "streamWrite",
        uuid,
        this
    };

    auto *stream = getStream(streamId);
    if (!stream)
        return {};

    int ret = libp2p_stream_write(
        ctx,
        stream,
        reinterpret_cast<uint8_t *>(const_cast<char *>(data.constData())),
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

QString Libp2pModulePlugin::streamWriteLp(uint64_t streamId, const QByteArray &data)
{
    if (!ctx || streamId == 0)
        return {};

    QString uuid = QUuid::createUuid().toString();
    auto *callbackCtx = new CallbackContext{
        "streamWriteLp",
        uuid,
        this
    };

    auto *stream = getStream(streamId);
    if (!stream)
        return {};

    int ret = libp2p_stream_writeLp(
        ctx,
        stream,
        reinterpret_cast<uint8_t *>(const_cast<char *>(data.constData())),
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

/* --------------- Kademlia --------------- */

QString Libp2pModulePlugin::kadFindNode(const QString &peerId)
{
    qDebug() << "Libp2pModulePlugin::kadFindNode called:" << peerId;

    if (!ctx) {
        qDebug() << "kadFindNode called without a context";
        return {};
    }

    QString uuid = QUuid::createUuid().toString();
    auto *callbackCtx = new CallbackContext{ "kadFindNode", uuid, this };

    int ret = libp2p_kad_find_node(
        ctx,
        peerId.toUtf8().constData(),
        &Libp2pModulePlugin::peersCallback,
        callbackCtx
    );

    if (ret != RET_OK) {
        delete callbackCtx;
        return {};
    }

    return uuid;
}


QString Libp2pModulePlugin::kadPutValue(const QByteArray &key, const QByteArray &value)
{
    qDebug() << "Libp2pModulePlugin::kadPutValue called";

    if (!ctx)
        return {};

    QString uuid = QUuid::createUuid().toString();
    auto *callbackCtx = new CallbackContext{ "kadPutValue", uuid, this };

    int ret = libp2p_kad_put_value(
        ctx,
        reinterpret_cast<const uint8_t *>(key.constData()),
        key.size(),
        reinterpret_cast<const uint8_t *>(value.constData()),
        value.size(),
        &Libp2pModulePlugin::libp2pCallback,
        callbackCtx
    );

    if (ret != RET_OK) {
        delete callbackCtx;
        return {};
    }

    return uuid;
}

QString Libp2pModulePlugin::kadGetValue(const QByteArray &key, int quorum)
{
    if (!ctx)
        return {};

    QString uuid = QUuid::createUuid().toString();
    auto *callbackCtx = new CallbackContext{ "kadGetValue", uuid, this };

    int ret = libp2p_kad_get_value(
        ctx,
        reinterpret_cast<const uint8_t *>(key.constData()),
        key.size(),
        quorum,
        &Libp2pModulePlugin::libp2pBufferCallback,
        callbackCtx
    );

    if (ret != RET_OK) {
        delete callbackCtx;
        return {};
    }

    return uuid;
}

QString Libp2pModulePlugin::kadAddProvider(const QString &cid)
{
    if (!ctx)
        return {};

    QString uuid = QUuid::createUuid().toString();
    auto *callbackCtx = new CallbackContext{ "kadAddProvider", uuid, this };

    int ret = libp2p_kad_add_provider(
        ctx,
        cid.toUtf8().constData(),
        &Libp2pModulePlugin::libp2pCallback,
        callbackCtx
    );

    if (ret != RET_OK) {
        delete callbackCtx;
        return {};
    }

    return uuid;
}

QString Libp2pModulePlugin::kadGetProviders(const QString &cid)
{
    if (!ctx)
        return {};

    QString uuid = QUuid::createUuid().toString();
    auto *callbackCtx = new CallbackContext{ "kadGetProviders", uuid, this };

    int ret = libp2p_kad_get_providers(
        ctx,
        cid.toUtf8().constData(),
        &Libp2pModulePlugin::getProvidersCallback,
        callbackCtx
    );

    if (ret != RET_OK) {
        delete callbackCtx;
        return {};
    }

    return uuid;
}

QString Libp2pModulePlugin::kadStartProviding(const QString &cid)
{
    if (!ctx)
        return {};

    QString uuid = QUuid::createUuid().toString();
    auto *callbackCtx = new CallbackContext{ "kadStartProviding", uuid, this };

    int ret = libp2p_kad_start_providing(
        ctx,
        cid.toUtf8().constData(),
        &Libp2pModulePlugin::libp2pCallback,
        callbackCtx
    );

    if (ret != RET_OK) {
        delete callbackCtx;
        return {};
    }

    return uuid;
}

QString Libp2pModulePlugin::kadStopProviding(const QString &cid)
{
    if (!ctx)
        return {};

    QString uuid = QUuid::createUuid().toString();
    auto *callbackCtx = new CallbackContext{ "kadStopProviding", uuid, this };

    int ret = libp2p_kad_stop_providing(
        ctx,
        cid.toUtf8().constData(),
        &Libp2pModulePlugin::libp2pCallback,
        callbackCtx
    );

    if (ret != RET_OK) {
        delete callbackCtx;
        return {};
    }

    return uuid;
}

QString Libp2pModulePlugin::kadGetRandomRecords()
{
    if (!ctx)
        return {};

    QString uuid = QUuid::createUuid().toString();
    auto *callbackCtx =
        new CallbackContext{ "kadGetRandomRecords", uuid, this };

    int ret = libp2p_kad_random_records(
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

bool Libp2pModulePlugin::setEventCallback()
{
    if (!ctx) {
        return false;
    }

    libp2p_set_event_callback(ctx, &Libp2pModulePlugin::libp2pCallback, NULL);
    return true;
}

