#include "libp2p_module_plugin.h"

#include <QtCore/QJsonObject>
#include <QtCore/QEventLoop>
#include <cstring>
#include <QDebug>

namespace {
    
struct Libp2pSyncContext {
    QEventLoop *loop = nullptr;
    int result = -1;
    QString message;
};

QJsonObject makeErrorResponse(
    int code,
    const QString &message
)
{
    QJsonObject error;
    error["code"] = code;
    error["message"] = message;

    QJsonObject response;
    response["error"] = error;
    return response;
}

QJsonObject makeResultResponse(
    const QString &cid
)
{
    QJsonObject response;
    response["result"] = cid;
    return response;
}
} // namespace


Libp2pModulePlugin::Libp2pModulePlugin()
    : ctx(nullptr)
{
    qRegisterMetaType<PeerInfo>("PeerInfo");
    qRegisterMetaType<ServiceInfo>("ServiceInfo");
    qRegisterMetaType<ExtendedPeerRecord>("ExtendedPeerRecord");

    /* ---- QList containers ---- */
    qRegisterMetaType<QList<PeerInfo>>("QList<PeerInfo>");
    qRegisterMetaType<QList<ServiceInfo>>("QList<ServiceInfo>");
    qRegisterMetaType<QList<ExtendedPeerRecord>>("QList<ExtendedPeerRecord>");

    std::memset(&config, 0, sizeof(config));

    config.flags |= LIBP2P_CFG_GOSSIPSUB;
    config.mount_gossipsub = 1;

    config.flags |= LIBP2P_CFG_GOSSIPSUB_TRIGGER_SELF;
    config.gossipsub_trigger_self = 1;

    config.flags |= LIBP2P_CFG_KAD;
    config.mount_kad = 1;

    auto *callbackCtx = new CallbackContext{ "libp2pNew", QUuid::createUuid().toString(), this };

    ctx = libp2p_new(&config, &Libp2pModulePlugin::libp2pCallback, callbackCtx);

    connect(this,
        &Libp2pModulePlugin::libp2pEvent,
        this,
        &Libp2pModulePlugin::onLibp2pEventDefault);
}

Libp2pModulePlugin::~Libp2pModulePlugin()
{
    if (ctx) {
        auto *callbackCtx = new CallbackContext{ "libp2pDestroy", QUuid::createUuid().toString(), this };
        libp2p_destroy(ctx, &Libp2pModulePlugin::libp2pCallback, callbackCtx);
        ctx = nullptr;
    }

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

QJsonObject Libp2pModulePlugin::toCid(const QByteArray &key)
{
    if (key.isEmpty()) {
        return makeErrorResponse(-32602, "Invalid params: key is empty");
    }

    QEventLoop loop;
    Libp2pSyncContext syncCtx{ &loop, -1, QString() };

    int ret = libp2p_create_cid(
        1,                      // CIDv1
        "dag-pb",
        "sha2-256",
        reinterpret_cast<const uint8_t *>(key.constData()),
        size_t(key.size()),
        &Libp2pModulePlugin::Libp2pSyncCallback,
        &syncCtx
    );

    if (ret != RET_OK) {
        return makeErrorResponse(ret, "libp2p_create_cid failed");
    }

    loop.exec();

    if (syncCtx.result == RET_OK) {
        return makeResultResponse(syncCtx.message);
    }

    return makeErrorResponse(syncCtx.result, syncCtx.message);
}

void Libp2pModulePlugin::Libp2pSyncCallback(
    int callerRet,
    const char *msg,
    size_t len,
    void *userData
)
{
    auto *syncCtx = static_cast<Libp2pSyncContext *>(userData);
    if (!syncCtx) return;

    const QString message = (msg && len > 0)
        ? QString::fromUtf8(msg, int(len))
        : QString();

    QMetaObject::invokeMethod(
        syncCtx->loop,
        [syncCtx, callerRet, message]() {
            syncCtx->result = callerRet;
            syncCtx->message = message;
            if (syncCtx->loop) {
                syncCtx->loop->quit();
            }
        },
        Qt::QueuedConnection
    );
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

bool Libp2pModulePlugin::libp2pStart()
{
    qDebug() << "Libp2pModulePlugin::libp2pStart called";
    if (!ctx) {
        qDebug() << "libp2pStart called without a context";
        return false;
    }

    auto *callbackCtx = new CallbackContext{ "libp2pStart", QUuid::createUuid().toString(), this };

    int ret = libp2p_start(ctx, &Libp2pModulePlugin::libp2pCallback, callbackCtx);

    if (ret != RET_OK) {
        delete callbackCtx;
    }

    return ret == RET_OK;
}

bool Libp2pModulePlugin::libp2pStop()
{
    qDebug() << "Libp2pModulePlugin::libp2pStop called";
    if (!ctx) {
        qDebug() << "libp2pStop called without a context";
        return false;
    }

    auto *callbackCtx = new CallbackContext{ "libp2pStop", QUuid::createUuid().toString(), this };

    int ret = libp2p_stop(ctx, &Libp2pModulePlugin::libp2pCallback, callbackCtx);

    if (ret != RET_OK) {
        delete callbackCtx;
    }

    return ret == RET_OK;
}

/* --------------- Connectivity --------------- */
bool Libp2pModulePlugin::connectPeer(
    const QString peerId,
    const QList<QString> multiaddrs,
    int64_t timeoutMs
)
{
    if (!ctx) return false;

    QByteArray peerIdUtf8 = peerId.toUtf8();

    QList<QByteArray> addrBuffers;
    QVector<const char*> addrPtrs;

    addrBuffers.reserve(multiaddrs.size());
    addrPtrs.reserve(multiaddrs.size());

    for (const auto &addr : multiaddrs) {
        addrBuffers.append(addr.toUtf8());
        addrPtrs.append(addrBuffers.last().constData());
    }

    auto *callbackCtx = new CallbackContext{
        "connectPeer",
        QUuid::createUuid().toString(),
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

    if (ret != RET_OK)
        delete callbackCtx;

    return ret == RET_OK;
}

bool Libp2pModulePlugin::disconnectPeer(const QString peerId)
{
    if (!ctx) return false;

    QByteArray peerIdUtf8 = peerId.toUtf8();

    auto *callbackCtx = new CallbackContext{
        "disconnectPeer",
        QUuid::createUuid().toString(),
        this
    };

    int ret = libp2p_disconnect(
        ctx,
        peerIdUtf8.constData(),
        &Libp2pModulePlugin::libp2pCallback,
        callbackCtx
    );

    if (ret != RET_OK)
        delete callbackCtx;

    return ret == RET_OK;
}

bool Libp2pModulePlugin::peerInfo()
{
    if (!ctx) return false;

    auto *callbackCtx = new CallbackContext{
        "peerInfo",
        QUuid::createUuid().toString(),
        this
    };

    int ret = libp2p_peerinfo(
        ctx,
        &Libp2pModulePlugin::peerInfoCallback,
        callbackCtx
    );

    if (ret != RET_OK)
        delete callbackCtx;

    return ret == RET_OK;
}

bool Libp2pModulePlugin::connectedPeers(int direction)
{
    if (!ctx) return false;

    auto *callbackCtx = new CallbackContext{
        "connectedPeers",
        QUuid::createUuid().toString(),
        this
    };

    int ret = libp2p_connected_peers(
        ctx,
        direction,
        &Libp2pModulePlugin::peersCallback,
        callbackCtx
    );

    if (ret != RET_OK)
        delete callbackCtx;

    return ret == RET_OK;
}

bool Libp2pModulePlugin::dial(const QString peerId, const QString proto)
{
    if (!ctx) return false;

    QByteArray peerIdUtf8 = peerId.toUtf8();
    QByteArray protoUtf8 = proto.toUtf8();

    auto *callbackCtx = new CallbackContext{
        "dial",
        QUuid::createUuid().toString(),
        this
    };

    int ret = libp2p_dial(
        ctx,
        peerIdUtf8.constData(),
        protoUtf8.constData(),
        &Libp2pModulePlugin::connectionCallback,
        callbackCtx
    );

    if (ret != RET_OK)
        delete callbackCtx;

    return ret == RET_OK;
}

// bool Libp2pModulePlugin::streamClose(libp2p_stream_t *conn)
// {
//     if (!ctx || !conn) return false;

//     auto *callbackCtx = new CallbackContext{
//         "libp2pStreamClose",
//         QUuid::createUuid().toString(),
//         this
//     };

//     int ret = libp2p_stream_close(
//         ctx,
//         conn,
//         &Libp2pModulePlugin::libp2pCallback,
//         callbackCtx
//     );

//     if (ret != RET_OK)
//         delete callbackCtx;

//     return ret == RET_OK;
// }

// bool Libp2pModulePlugin::streamCloseEOF(libp2p_stream_t *conn)
// {
//     if (!ctx || !conn) return false;

//     auto *callbackCtx = new CallbackContext{
//         "libp2pStreamCloseEOF",
//         QUuid::createUuid().toString(),
//         this
//     };

//     int ret = libp2p_stream_closeWithEOF(
//         ctx,
//         conn,
//         &Libp2pModulePlugin::libp2pCallback,
//         callbackCtx
//     );

//     if (ret != RET_OK)
//         delete callbackCtx;

//     return ret == RET_OK;
// }

// bool Libp2pModulePlugin::streamRelease(libp2p_stream_t *conn)
// {
//     if (!ctx || !conn) return false;

//     auto *callbackCtx = new CallbackContext{
//         "libp2pStreamRelease",
//         QUuid::createUuid().toString(),
//         this
//     };

//     int ret = libp2p_stream_release(
//         ctx,
//         conn,
//         &Libp2pModulePlugin::libp2pCallback,
//         callbackCtx
//     );

//     if (ret != RET_OK)
//         delete callbackCtx;

//     return ret == RET_OK;
// }

// bool Libp2pModulePlugin::streamReadExactly(libp2p_stream_t *conn, size_t len)
// {
//     if (!ctx || !conn) return false;

//     auto *callbackCtx = new CallbackContext{
//         "libp2pStreamReadExactly",
//         QUuid::createUuid().toString(),
//         this
//     };

//     int ret = libp2p_stream_readExactly(
//         ctx,
//         conn,
//         len,
//         &Libp2pModulePlugin::libp2pBufferCallback,
//         callbackCtx
//     );

//     if (ret != RET_OK)
//         delete callbackCtx;

//     return ret == RET_OK;
// }

// bool Libp2pModulePlugin::streamReadLp(libp2p_stream_t *conn, int64_t maxSize)
// {
//     if (!ctx || !conn) return false;

//     auto *callbackCtx = new CallbackContext{
//         "libp2pStreamReadLp",
//         QUuid::createUuid().toString(),
//         this
//     };

//     int ret = libp2p_stream_readLp(
//         ctx,
//         conn,
//         maxSize,
//         &Libp2pModulePlugin::libp2pBufferCallback,
//         callbackCtx
//     );

//     if (ret != RET_OK)
//         delete callbackCtx;

//     return ret == RET_OK;
// }

// bool Libp2pModulePlugin::streamWrite(libp2p_stream_t *conn, const QByteArray &data)
// {
//     if (!ctx || !conn) return false;

//     auto *callbackCtx = new CallbackContext{
//         "libp2pStreamWrite",
//         QUuid::createUuid().toString(),
//         this
//     };

//     int ret = libp2p_stream_write(
//         ctx,
//         conn,
//         reinterpret_cast<uint8_t *>(const_cast<char *>(data.constData())),
//         data.size(),
//         &Libp2pModulePlugin::libp2pCallback,
//         callbackCtx
//     );

//     if (ret != RET_OK)
//         delete callbackCtx;

//     return ret == RET_OK;
// }

// bool Libp2pModulePlugin::streamWriteLp(libp2p_stream_t *conn, const QByteArray &data)
// {
//     if (!ctx || !conn) return false;

//     auto *callbackCtx = new CallbackContext{
//         "libp2pStreamWriteLp",
//         QUuid::createUuid().toString(),
//         this
//     };

//     int ret = libp2p_stream_writeLp(
//         ctx,
//         conn,
//         reinterpret_cast<uint8_t *>(const_cast<char *>(data.constData())),
//         data.size(),
//         &Libp2pModulePlugin::libp2pCallback,
//         callbackCtx
//     );

//     if (ret != RET_OK)
//         delete callbackCtx;

//     return ret == RET_OK;
// }

/* --------------- Kademlia --------------- */

bool Libp2pModulePlugin::kadFindNode(const QString &peerId)
{
    qDebug() << "Libp2pModulePlugin::kadFindNode called:" << peerId;
    if (!ctx) {
        qDebug() << "kadFindNode called without a context";
        return false;
    }

    auto *callbackCtx = new CallbackContext{ "kadFindNode", QUuid::createUuid().toString(), this };

    int ret = libp2p_kad_find_node(
        ctx,
        peerId.toUtf8().constData(),
        &Libp2pModulePlugin::peersCallback,
        callbackCtx
    );

    if (ret != RET_OK) {
        delete callbackCtx;
    }

    return ret == RET_OK;
}

bool Libp2pModulePlugin::kadPutValue(const QByteArray &key, const QByteArray &value)
{
    qDebug() << "Libp2pModulePlugin::kadPutValue called";
    if (!ctx) {
        qDebug() << "kadPutValue called without a context";
        return false;
    }

    auto *callbackCtx = new CallbackContext{ "kadPutValue", QUuid::createUuid().toString(), this };

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
    }

    return ret == RET_OK;
}

bool Libp2pModulePlugin::kadGetValue(const QByteArray &key, int quorum)
{
    qDebug() << "Libp2pModulePlugin::kadGetValue called";
    if (!ctx) {
        qDebug() << "kadGetValue called without a context";
        return false;
    }

    auto *callbackCtx = new CallbackContext{ "kadGetValue", QUuid::createUuid().toString(), this };

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
    }

    return ret == RET_OK;
}

bool Libp2pModulePlugin::kadAddProvider(const QString &cid)
{
    qDebug() << "Libp2pModulePlugin::kadAddProvider called:" << cid;
    if (!ctx) {
        qDebug() << "kadAddProvider called without a context";
        return false;
    }

    auto *callbackCtx = new CallbackContext{ "kadAddProvider", QUuid::createUuid().toString(), this };

    int ret = libp2p_kad_add_provider(
        ctx,
        cid.toUtf8().constData(),
        &Libp2pModulePlugin::libp2pCallback,
        callbackCtx
    );

    if (ret != RET_OK) {
        delete callbackCtx;
    }

    return ret == RET_OK;
}

bool Libp2pModulePlugin::kadGetProviders(const QString &cid)
{
    qDebug() << "Libp2pModulePlugin::kadGetProviders called:" << cid;
    if (!ctx) {
        qDebug() << "kadGetProviders called without a context";
        return false;
    }

    auto *callbackCtx = new CallbackContext{ "kadGetProviders", QUuid::createUuid().toString(), this };

    int ret = libp2p_kad_get_providers(
        ctx,
        cid.toUtf8().constData(),
        &Libp2pModulePlugin::getProvidersCallback,
        callbackCtx
    );

    if (ret != RET_OK) {
        delete callbackCtx;
    }

    return ret == RET_OK;
}

bool Libp2pModulePlugin::kadStartProviding(const QString &cid)
{
    qDebug() << "Libp2pModulePlugin::kadStartProviding called:" << cid;
    if (!ctx) {
        qDebug() << "kadStartProviding called without a context";
        return false;
    }

    auto *callbackCtx = new CallbackContext{ "kadStartProviding", QUuid::createUuid().toString(), this };

    int ret = libp2p_kad_start_providing(
        ctx,
        cid.toUtf8().constData(),
        &Libp2pModulePlugin::libp2pCallback,
        callbackCtx
    );

    if (ret != RET_OK) {
        delete callbackCtx;
    }

    return ret == RET_OK;
}

bool Libp2pModulePlugin::kadStopProviding(const QString &cid)
{
    qDebug() << "Libp2pModulePlugin::kadStopProviding called:" << cid;
    if (!ctx) {
        qDebug() << "kadStopProviding called without a context";
        return false;
    }

    auto *callbackCtx = new CallbackContext{ "kadStopProviding", QUuid::createUuid().toString(), this };

    int ret = libp2p_kad_stop_providing(
        ctx,
        cid.toUtf8().constData(),
        &Libp2pModulePlugin::libp2pCallback,
        callbackCtx
    );

    if (ret != RET_OK) {
        delete callbackCtx;
    }

    return ret == RET_OK;
}

bool Libp2pModulePlugin::kadGetRandomRecords()
{
    qDebug() << "Libp2pModulePlugin::kadGetRandomRecords called";

    if (!ctx) {
        qDebug() << "kadGetRandomRecords called without a context";
        return false;
    }

    auto *callbackCtx =
        new CallbackContext{ "kadGetRandomRecords", QUuid::createUuid().toString(), this };

    int ret = libp2p_kad_random_records(
        ctx,
        &Libp2pModulePlugin::randomRecordsCallback,
        callbackCtx
    );

    if (ret != RET_OK) {
        delete callbackCtx;
    }

    return ret == RET_OK;
}

bool Libp2pModulePlugin::setEventCallback()
{
    if (!ctx) {
        return false;
    }

    libp2p_set_event_callback(ctx, &Libp2pModulePlugin::libp2pCallback, NULL);
    return true;
}
