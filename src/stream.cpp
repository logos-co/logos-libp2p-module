#include "plugin.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QDebug>

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

/* --------------- Streams --------------- */
QString Libp2pModulePlugin::streamClose(uint64_t streamId)
{

    if (!ctx || streamId == 0)
        return {};

    auto *stream = removeStream(streamId);
    if (!stream)
        return {};

    QString uuid = QUuid::createUuid().toString();
    auto *callbackCtx = new CallbackContext{
        "streamClose",
        uuid,
        this
    };

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

    auto *stream = removeStream(streamId);
    if (!stream)
        return {};

    QString uuid = QUuid::createUuid().toString();
    auto *callbackCtx = new CallbackContext{
        "streamCloseEOF",
        uuid,
        this
    };

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

    auto *stream = removeStream(streamId);
    if (!stream)
        return {};

    QString uuid = QUuid::createUuid().toString();
    auto *callbackCtx = new CallbackContext{
        "streamRelease",
        uuid,
        this
    };

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
